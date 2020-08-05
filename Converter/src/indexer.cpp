
#include <cerrno>
#include <execution>
#include <algorithm>

#include "indexer.h"

#include "Attributes.h"

using std::unique_lock;

namespace indexer{

	struct Point {
		double x;
		double y;
		double z;
		int32_t pointIndex;
		int32_t childIndex;
	};

	void sortBreadthFirst(vector<Node*>& nodes) {
		sort(nodes.begin(), nodes.end(), [](Node* a, Node* b) {
			if (a->name.size() != b->name.size()) {
				return a->name.size() < b->name.size();
			} else {
				return a->name < b->name;
			}
		});
	}

	uint8_t childMaskOf(Node* node) {
		uint8_t mask = 0;

		for (int i = 0; i < 8; i++) {
			auto child = node->children[i];

			if (child != nullptr) {
				mask = mask | (1 << i);
			}
		}

		return mask;
	}

	shared_ptr<Chunks> getChunks(string pathIn) {
		string chunkDirectory = pathIn + "/chunks";

		string metadataText = readTextFile(chunkDirectory + "/metadata.json");
		json js = json::parse(metadataText);

		Vector3 min = {
			js["min"][0].get<double>(),
			js["min"][1].get<double>(),
			js["min"][2].get<double>()
		};

		Vector3 max = {
			js["max"][0].get<double>(),
			js["max"][1].get<double>(),
			js["max"][2].get<double>()
		};

		vector<Attribute> attributeList;
		auto jsAttributes = js["attributes"];
		for (auto jsAttribute : jsAttributes) {

			string name = jsAttribute["name"];
			string description = jsAttribute["description"];
			int size = jsAttribute["size"];
			int numElements = jsAttribute["numElements"];
			int elementSize = jsAttribute["elementSize"];
			AttributeType type = typenameToType(jsAttribute["type"]);

			auto jsMin = jsAttribute["min"];
			auto jsMax = jsAttribute["max"];

			Attribute attribute(name, size, numElements, elementSize, type);

			if (numElements >= 1) {
				attribute.min.x = jsMin[0] == nullptr ? Infinity : double(jsMin[0]);
				attribute.max.x = jsMax[0] == nullptr ? Infinity : double(jsMax[0]);
			}
			if (numElements >= 2) {
				attribute.min.y = jsMin[1] == nullptr ? Infinity : double(jsMin[1]);
				attribute.max.y = jsMax[1] == nullptr ? Infinity : double(jsMax[1]);
			}
			if (numElements >= 3) {
				attribute.min.z = jsMin[2] == nullptr ? Infinity : double(jsMin[2]);
				attribute.max.z = jsMax[2] == nullptr ? Infinity : double(jsMax[2]);
			}

			attributeList.push_back(attribute);
		}

		double scaleX = js["scale"][0];
		double scaleY = js["scale"][1];
		double scaleZ = js["scale"][2];

		double offsetX = js["offset"][0];
		double offsetY = js["offset"][1];
		double offsetZ = js["offset"][2];

		Attributes attributes(attributeList);
		attributes.posScale = { scaleX, scaleY, scaleZ };
		attributes.posOffset = { offsetX, offsetY, offsetZ };
		

		auto toID = [](string filename) -> string {
			string strID = stringReplace(filename, "chunk_", "");
			strID = stringReplace(strID, ".bin", "");

			return strID;
		};

		vector<shared_ptr<Chunk>> chunksToLoad;
		for (const auto& entry : fs::directory_iterator(chunkDirectory)) {
			string filename = entry.path().filename().string();
			string chunkID = toID(filename);

			if (!iEndsWith(filename, ".bin")) {
				continue;
			}

			shared_ptr<Chunk> chunk = make_shared<Chunk>();
			chunk->file = entry.path().string();
			chunk->id = chunkID;

			BoundingBox box = { min, max };

			for (int i = 1; i < chunkID.size(); i++) {
				int index = chunkID[i] - '0'; // this feels so wrong...

				box = childBoundingBoxOf(box.min, box.max, index);
			}

			chunk->min = box.min;
			chunk->max = box.max;

			chunksToLoad.push_back(chunk);
		}

		auto chunks = make_shared<Chunks>(chunksToLoad, min, max);
		chunks->attributes = attributes;

		return chunks;
	}

	void Indexer::flushChunkRoot(shared_ptr<Node> chunkRoot) {

		lock_guard<mutex> lock(mtx_chunkRoot);

		static int64_t offset = 0;
		int64_t size = chunkRoot->points->size;

		fChunkRoots.write(chunkRoot->points->data_char, size);

		FlushedChunkRoot fcr;
		fcr.node = chunkRoot;
		fcr.offset = offset;
		fcr.size = size;

		chunkRoot->points = nullptr;

		flushedChunkRoots.push_back(fcr);

		offset += size;
	}

	void Indexer::reloadChunkRoots() {

		fChunkRoots.close();

		struct LoadTask {
			shared_ptr<Node> node;
			int64_t offset;
			int64_t size;

			LoadTask(shared_ptr<Node> node, int64_t offset, int64_t size) {
				this->node = node;
				this->offset = offset;
				this->size = size;
			}
		};

		string targetDir = this->targetDir;
		TaskPool<LoadTask> pool(16, [targetDir](shared_ptr<LoadTask> task) {
			string octreePath = targetDir + "/tmpChunkRoots.bin";

			shared_ptr<Node> node = task->node;
			int64_t start = task->offset;
			int64_t size = task->size;

			auto buffer = make_shared<Buffer>(size);
			readBinaryFile(octreePath, start, size, buffer->data);

			node->points = buffer;
		});

		for (auto fcr : flushedChunkRoots) {
			auto task = make_shared<LoadTask>(fcr.node, fcr.offset, fcr.size);
			pool.addTask(task);
		}

		pool.close();
	}

	void Indexer::waitUntilWriterBacklogBelow(int maxMegabytes) {
		using namespace std::chrono_literals;

		while (true) {
			auto backlog = writer->backlogSizeMB();

			if (backlog > maxMegabytes) {
				std::this_thread::sleep_for(10ms);
			} else {
				break;
			}
		}
	}

	void Indexer::waitUntilMemoryBelow(int maxMegabytes) {
		using namespace std::chrono_literals;

		while (true) {
			auto memoryData = getMemoryData();
			auto usedMemoryMB = memoryData.virtual_usedByProcess / (1024 * 1024);

			if (usedMemoryMB > maxMegabytes) {
				std::this_thread::sleep_for(10ms);
			} else {
				break;
			}
		}
	}

string Indexer::createMetadata(Options options, State& state, Hierarchy hierarchy) {

	auto min = root->min;
	auto max = root->max;

	auto d = [](double value) {
		auto digits = std::numeric_limits<double>::max_digits10;

		std::stringstream ss;
		ss << std::setprecision(digits);
		ss << value;
		
		return ss.str();
	};

	auto s = [](string str) {
		return "\"" + str + "\"";
	};

	auto t = [](int numTabs) {
		return string(numTabs, '\t');
	};

	auto toJson = [d](Vector3 value) {
		return "[" + d(value.x) + ", " + d(value.y) + ", " + d(value.z) + "]";
	};

	auto vecToJson = [d](vector<double> values) {

		stringstream ss;
		ss << "[";

		for (int i = 0; i < values.size(); i++) {

			ss << d(values[i]);

			if (i < values.size() - 1) {
				ss << ", ";
			}
		}
		ss << "]";

		return ss.str();


		//return "[" + d(value.x) + ", " + d(value.y) + ", " + d(value.z) + "]";
	};

	auto octreeDepth = this->octreeDepth;
	auto getHierarchyJsonString = [hierarchy, octreeDepth, t, s]() {

		stringstream ss;
		ss << "{" << endl;
		ss << t(2) << s("firstChunkSize") << ": " << hierarchy.firstChunkSize << ", " << endl;
		ss << t(2) << s("stepSize") << ": " << hierarchy.stepSize << ", " << endl;
		ss << t(2) << s("depth") << ": " << octreeDepth << endl;
		ss << t(1) << "}";

		return ss.str();
	};

	auto getBoundingBoxJsonString = [min, max, t, s, toJson, vecToJson]() {

		stringstream ss;
		ss << "{" << endl;
		ss << t(2) << s("min") << ": " << toJson(min) << ", " << endl;
		ss << t(2) << s("max") << ": " << toJson(max) << endl;
		ss << t(1) << "}";

		return ss.str();
	};

	Attributes& attributes = this->attributes;
	auto getAttributesJsonString = [&attributes, t, s, toJson, vecToJson]() {

		stringstream ss;
		ss << "[" << endl;

		for (int i = 0; i < attributes.list.size(); i++) {
			auto& attribute = attributes.list[i];

			if (i == 0) {
				ss << t(2) << "{" << endl;
			}

			ss << t(3) << s("name") << ": " << s(attribute.name) << "," << endl;
			ss << t(3) << s("description") << ": " << s(attribute.description) << "," << endl;
			ss << t(3) << s("size") << ": " << attribute.size << "," << endl;
			ss << t(3) << s("numElements") << ": " << attribute.numElements << "," << endl;
			ss << t(3) << s("elementSize") << ": " << attribute.elementSize << "," << endl;
			ss << t(3) << s("type") << ": " << s(getAttributeTypename(attribute.type)) << "," << endl;

			if (attribute.numElements == 1) {
				ss << t(3) << s("min") << ": " << vecToJson(vector<double>{ attribute.min.x }) << "," << endl;
				ss << t(3) << s("max") << ": " << vecToJson(vector<double>{ attribute.max.x }) << endl;
			} else if (attribute.numElements == 2) {
				ss << t(3) << s("min") << ": " << vecToJson(vector<double>{ attribute.min.x, attribute.min.y }) << "," << endl;
				ss << t(3) << s("max") << ": " << vecToJson(vector<double>{ attribute.max.x, attribute.max.y }) << endl;
			} else if (attribute.numElements == 3) {
				ss << t(3) << s("min") << ": " << vecToJson(vector<double>{ attribute.min.x, attribute.min.y, attribute.min.z }) << "," << endl;
				ss << t(3) << s("max") << ": " << vecToJson(vector<double>{ attribute.max.x, attribute.max.y, attribute.max.z }) << endl;
			}

			if (i < attributes.list.size() - 1) {
				ss << t(2) << "},{" << endl;
			} else {
				ss << t(2) << "}" << endl;
			}

		}
		

		ss << t(1) << "]";

		return ss.str();
	};

	stringstream ss;

	ss << t(0) << "{" << endl;
	ss << t(1) << s("version") << ": " << s("2.0") << "," << endl;
	ss << t(1) << s("name") << ": " << s(options.name) << "," << endl;
	ss << t(1) << s("description") << ": " << s("") << "," << endl;
	ss << t(1) << s("points") << ": " << state.pointsTotal << "," << endl;
	ss << t(1) << s("projection") << ": " << s("") << "," << endl;
	ss << t(1) << s("hierarchy") << ": " << getHierarchyJsonString() << "," << endl;
	ss << t(1) << s("offset") << ": " << toJson(attributes.posOffset) << "," << endl;
	ss << t(1) << s("scale") << ": " << toJson(attributes.posScale) << "," << endl;
	ss << t(1) << s("spacing") << ": " << d(spacing) << "," << endl;
	ss << t(1) << s("boundingBox") << ": " << getBoundingBoxJsonString() << "," << endl;
	ss << t(1) << s("attributes") << ": " << getAttributesJsonString() << endl;
	ss << t(0) << "}" << endl;

	string str = ss.str();

	//json js;

	//js["name"] = "abc";
	//js["boundingBox"]["min"] = { min.x, min.y, min.z };
	//js["boundingBox"]["max"] = { max.x, max.y, max.z };
	//js["projection"] = "";
	//js["description"] = "";
	//js["points"] = int64_t(state.pointsTotal);
	//js["spacing"] = spacing;

	//js["scale"] = vector<double>{
	//	attributes.posScale.x,
	//	attributes.posScale.y,
	//	attributes.posScale.z
	//};

	//js["offset"] = vector<double>{
	//	attributes.posOffset.x,
	//	attributes.posOffset.y,
	//	attributes.posOffset.z
	//};

	//js["hierarchy"] = {
	//	{"stepSize", hierarchy.stepSize},
	//	{"firstChunkSize", hierarchy.firstChunkSize}
	//};

	//json jsAttributes;
	//for (auto attribute : attributes.list) {
	//	json jsAttribute;

	//	jsAttribute["name"] = attribute.name;
	//	jsAttribute["description"] = attribute.description;
	//	jsAttribute["size"] = attribute.size;
	//	jsAttribute["numElements"] = attribute.numElements;
	//	jsAttribute["elementSize"] = attribute.elementSize;
	//	jsAttribute["type"] = getAttributeTypename(attribute.type);

	//	if (attribute.numElements == 1) {
	//		jsAttribute["min"] = vector<double>{ attribute.min.x };
	//		jsAttribute["max"] = vector<double>{ attribute.max.x };
	//	} else if (attribute.numElements == 2) {
	//		jsAttribute["min"] = vector<double>{ attribute.min.x, attribute.min.y };
	//		jsAttribute["max"] = vector<double>{ attribute.max.x, attribute.max.y };
	//	} else if (attribute.numElements == 3) {
	//		jsAttribute["min"] = vector<double>{ attribute.min.x, attribute.min.y, attribute.min.z };
	//		jsAttribute["max"] = vector<double>{ attribute.max.x, attribute.max.y, attribute.max.z };
	//	}

	//	jsAttributes.push_back(jsAttribute);
	//}

	//js["attributes"] = jsAttributes;

	//string str = js.dump(4);

	return str;
}

HierarchyChunk Indexer::gatherChunk(Node* start, int levels) {
	// create vector containing start node and all descendants up to and including levels deeper
	// e.g. start 0 and levels 5 -> all nodes from level 0 to inclusive 5.

	int64_t startLevel = start->name.size() - 1;

	HierarchyChunk chunk;
	chunk.name = start->name;

	vector<Node*> stack = { start };
	while (!stack.empty()) {
		Node* node = stack.back();
		stack.pop_back();

		chunk.nodes.push_back(node);

		int64_t childLevel = node->name.size();
		if (childLevel <= startLevel + levels) {

			for (auto child : node->children) {
				if (child == nullptr) {
					continue;
				}

				stack.push_back(child.get());
			}

		}
	}

	return chunk;
}

vector<HierarchyChunk> Indexer::createHierarchyChunks(Node* root, int hierarchyStepSize) {

	vector<HierarchyChunk> hierarchyChunks;
	vector<Node*> stack = { root };
	while (!stack.empty()) {
		Node* chunkRoot = stack.back();
		stack.pop_back();

		auto chunk = gatherChunk(chunkRoot, hierarchyStepSize);

		for (auto node : chunk.nodes) {
			bool isProxy = node->level() == chunkRoot->level() + hierarchyStepSize;

			if (isProxy) {
				stack.push_back(node);
			}

		}

		hierarchyChunks.push_back(chunk);
	}

	return hierarchyChunks;
}

Hierarchy Indexer::createHierarchy(string path) {

	constexpr int hierarchyStepSize = 4;
	// type + childMask + numPoints + offset + size
	constexpr int bytesPerNode = 1 + 1 + 4 + 8 + 8;

	auto chunkSize = [](HierarchyChunk& chunk) {
		return chunk.nodes.size() * bytesPerNode;
	};

	auto chunks = createHierarchyChunks(root.get(), hierarchyStepSize);

	unordered_map<string, int> chunkPointers;
	vector<int64_t> chunkByteOffsets(chunks.size(), 0);
	int64_t hierarchyBufferSize = 0;

	for (size_t i = 0; i < chunks.size(); i++) {
		auto& chunk = chunks[i];
		chunkPointers[chunk.name] = i;

		sortBreadthFirst(chunk.nodes);

		if (i >= 1) {
			chunkByteOffsets[i] = chunkByteOffsets[i - 1] + chunkSize(chunks[i - 1]);
		}

		hierarchyBufferSize += chunkSize(chunk);
	}

	vector<uint8_t> hierarchyBuffer(hierarchyBufferSize);

	enum TYPE {
		NORMAL = 0,
		LEAF = 1,
		PROXY = 2,
	};

	int offset = 0;
	for (int i = 0; i < chunks.size(); i++) {
		auto& chunk = chunks[i];
		auto chunkLevel = chunk.name.size() - 1;

		for (auto node : chunk.nodes) {
			bool isProxy = node->level() == chunkLevel + hierarchyStepSize;

			uint8_t childMask = childMaskOf(node);
			uint64_t targetOffset = 0;
			uint64_t targetSize = 0;
			uint32_t numPoints = uint32_t(node->numPoints);
			uint8_t type = node->isLeaf() ? TYPE::LEAF : TYPE::NORMAL;

			if (isProxy) {
				int targetChunkIndex = chunkPointers[node->name];
				auto targetChunk = chunks[targetChunkIndex];

				type = TYPE::PROXY;
				targetOffset = chunkByteOffsets[targetChunkIndex];
				targetSize = chunkSize(targetChunk);
			} else {
				targetOffset = node->byteOffset;
				targetSize = node->byteSize;
			}

			memcpy(hierarchyBuffer.data() + offset + 0, &type, 1);
			memcpy(hierarchyBuffer.data() + offset + 1, &childMask, 1);
			memcpy(hierarchyBuffer.data() + offset + 2, &numPoints, 4);
			memcpy(hierarchyBuffer.data() + offset + 6, &targetOffset, 8);
			memcpy(hierarchyBuffer.data() + offset + 14, &targetSize, 8);

			offset += bytesPerNode;
		}

	}

	Hierarchy hierarchy;
	hierarchy.stepSize = hierarchyStepSize;
	hierarchy.buffer = hierarchyBuffer;
	hierarchy.firstChunkSize = chunks[0].nodes.size() * bytesPerNode;

	return hierarchy;

}


struct NodeCandidate {
	string name = "";
	int64_t indexStart = 0;
	int64_t numPoints = 0;
	int64_t level = 0;
	int64_t x = 0;
	int64_t y = 0;
	int64_t z = 0;
};

vector<vector<int64_t>> createSumPyramid(vector<int64_t>& grid, int gridSize) {

	auto tStart = now();

	int maxLevel = std::log2(gridSize);
	int currentGridSize = gridSize / 2;

	vector<vector<int64_t>> sumPyramid(maxLevel + 1);
	for (int level = 0; level < maxLevel; level++) {
		auto cells = pow(8, level);
		sumPyramid[level].resize(cells, 0);
	}
	sumPyramid[maxLevel] = grid;

	for (int level = maxLevel - 1; level >= 0; level--) {

		for (int x = 0; x < currentGridSize; x++) {
		for (int y = 0; y < currentGridSize; y++) {
		for (int z = 0; z < currentGridSize; z++) {

			auto index = mortonEncode_magicbits(z, y, x);
			auto index_p1 = mortonEncode_magicbits(2 * z, 2 * y, 2 * x);

			int64_t sum = 0;
			for (int i = 0; i < 8; i++) {
				sum += sumPyramid[level + 1][index_p1 + i];
			}

			if (sum > 0) {
				int a = 10;
			}

			sumPyramid[level][index] = sum;

		}
		}
		}

		currentGridSize = currentGridSize / 2;

	}

	return sumPyramid;
}

vector<NodeCandidate> createNodes(vector<vector<int64_t>>& pyramid) {

	vector<NodeCandidate> nodes;

	vector<vector<int64_t>> pyramidOffsets;
	for (auto& counters : pyramid) {

		if (counters.size() == 1) {
			pyramidOffsets.push_back({ 0 });
		} else {

			vector<int64_t> offsets(counters.size(), 0);
			for (int64_t i = 1; i < counters.size(); i++) {
				int64_t offset = offsets[i - 1] + counters[i - 1];

				offsets[i] = offset;
			}

			pyramidOffsets.push_back(offsets);
		}
	}

	// pyramid starts at level 0 -> gridSize = 1
	// 2 levels -> levels 0 and 1 -> maxLevel 1
	auto maxLevel = pyramid.size() - 1;

	NodeCandidate root;
	root.name = "";
	root.level = 0;
	root.x = 0;
	root.y = 0;
	root.z = 0;

	vector<NodeCandidate> stack = { root };

	while (!stack.empty()) {

		NodeCandidate candidate = stack.back();
		stack.pop_back();

		auto level = candidate.level;
		auto x = candidate.x;
		auto y = candidate.y;
		auto z = candidate.z;

		auto& grid = pyramid[level];
		auto index = mortonEncode_magicbits(z, y, x);
		int64_t numPoints = grid[index];

		if (level == maxLevel) {
			// don't split further at this time. May be split further in another pass

			if (numPoints > 0) {
				nodes.push_back(candidate);
			}
		} else if (numPoints > maxPointsPerChunk) {
			// split (too many points in node)

			for (int i = 0; i < 8; i++) {

				auto index_p1 = mortonEncode_magicbits(2 * z, 2 * y, 2 * x) + i;
				auto count = pyramid[level + 1][index_p1];

				if (count > 0) {
					NodeCandidate child;
					child.level = level + 1;
					child.name = candidate.name + to_string(i);
					child.indexStart = pyramidOffsets[level + 1][index_p1];
					child.numPoints = count;
					child.x = 2 * x + ((i & 0b100) >> 2);
					child.y = 2 * y + ((i & 0b010) >> 1);
					child.z = 2 * z + ((i & 0b001) >> 0);

					stack.push_back(child);

					if (child.name == "75") {
						int a = 10;
					}
				}
			}

		} else if(numPoints > 0 ){
			// accept (small enough)
			nodes.push_back(candidate);
		}

	}

	return nodes;
}

// 1. Counter grid
// 2. Hierarchy from counter grid
// 3. identify nodes that need further refinment
// 4. Recursively repeat at 1. for identified nodes
void buildHierarchy(Indexer* indexer, Node* node, shared_ptr<Buffer> points, int numPoints, int depth = 0) {

	if (numPoints < maxPointsPerChunk) {
		Node* realization = node;
		realization->indexStart = 0;
		realization->numPoints = numPoints;
		realization->points = points;

		return;
	}


	auto tStart = now();

	int64_t levels = 5; // = gridSize 32
	int64_t counterGridSize = pow(2, levels);
	vector<int64_t> counters(counterGridSize * counterGridSize * counterGridSize, 0);

	auto min = node->min;
	auto max = node->max;
	auto size = max - min;
	auto attributes = indexer->attributes;
	int bpp = attributes.bytes;
	auto scale = attributes.posScale;
	auto offset = attributes.posOffset;

	//vector<int32_t> dbg(pointBuffer->data_i32, pointBuffer->data_i32 + 10);

	auto gridIndexOf = [&points, bpp, scale, offset, min, size, counterGridSize](int64_t pointIndex) {

		int64_t pointOffset = pointIndex * bpp;
		int32_t* xyz = reinterpret_cast<int32_t*>(points->data_u8 + pointOffset);

		double x = (xyz[0] * scale.x) + offset.x;
		double y = (xyz[1] * scale.y) + offset.y;
		double z = (xyz[2] * scale.z) + offset.z;

		int64_t ix = double(counterGridSize) * (x - min.x) / size.x;
		int64_t iy = double(counterGridSize) * (y - min.y) / size.y;
		int64_t iz = double(counterGridSize) * (z - min.z) / size.z;

		ix = std::max(0ll, std::min(ix, counterGridSize - 1));
		iy = std::max(0ll, std::min(iy, counterGridSize - 1));
		iz = std::max(0ll, std::min(iz, counterGridSize - 1));

		int64_t index = mortonEncode_magicbits(iz, iy, ix);

		return index;
	};

	// COUNTING
	for (int i = 0; i < numPoints; i++) {
		auto index = gridIndexOf(i);
		counters[index]++;
	}

	{ // DISTRIBUTING
		vector<int64_t> offsets(counters.size(), 0);
		for (int i = 1; i < counters.size(); i++) {
			offsets[i] = offsets[i - 1] + counters[i - 1];
		}

		Buffer tmp(numPoints * bpp);

		for (int i = 0; i < numPoints; i++) {
			auto index = gridIndexOf(i);
			auto targetIndex = offsets[index]++;

			memcpy(tmp.data_u8 + targetIndex * bpp, points->data_u8 + i * bpp, bpp);
		}

		memcpy(points->data, tmp.data, numPoints * bpp);
	}

	auto pyramid = createSumPyramid(counters, counterGridSize);

	auto nodes = createNodes(pyramid);

	auto expandTo = [node](NodeCandidate& candidate) {

		string startName = node->name;
		string fullName = startName + candidate.name;

		// e.g. startName: r, fullName: r031
		// start iteration with char at index 1: "0"

		Node* currentNode = node;
		for (int i = startName.size(); i < fullName.size(); i++) {
			int index = fullName.at(i) - '0';

			if (currentNode->children[index] == nullptr) {
				auto childBox = childBoundingBoxOf(currentNode->min, currentNode->max, index);
				string childName = currentNode->name + to_string(index);

				shared_ptr<Node> child = make_shared<Node>();
				child->min = childBox.min;
				child->max = childBox.max;
				child->name = childName;
				child->children.resize(8);

				currentNode->children[index] = child;
				currentNode = child.get();
			} else {
				currentNode = currentNode->children[index].get();
			}

			
		}

		return currentNode;
	};

	vector<Node*> needRefinement;

	int64_t octreeDepth = 0;
	for (NodeCandidate& candidate : nodes) {

		Node* realization = expandTo(candidate);
		realization->indexStart = candidate.indexStart;
		realization->numPoints = candidate.numPoints;
		int64_t bytes = candidate.numPoints * bpp;
		auto buffer = make_shared<Buffer>(bytes);
		memcpy(buffer->data,
			points->data_u8 + candidate.indexStart * bpp,
			candidate.numPoints * bpp
		);

		realization->points = buffer;

		if (realization->numPoints > maxPointsPerChunk) {
			needRefinement.push_back(realization);
		}

		octreeDepth = std::max(octreeDepth, realization->level());
	}

	{
		lock_guard<mutex> lock(indexer->mtx_depth);

		indexer->octreeDepth = std::max(indexer->octreeDepth, octreeDepth);
	}

	for (auto subject : needRefinement) {
		auto buffer = subject->points;
		auto numPoints = subject->numPoints;

		subject->points = nullptr;
		subject->numPoints = 0;

		buildHierarchy(indexer, subject, buffer, numPoints, depth + 1);
	}

}


Writer::Writer(Indexer* indexer) {
	this->indexer = indexer;

	string octreePath = indexer->targetDir + "/octree.bin";
	fsOctree.open(octreePath, ios::out | ios::binary);

	launchWriterThread();
}

mutex mtx_backlog;
int64_t Writer::backlogSizeMB() {
	lock_guard<mutex> lock(mtx_backlog);

	int64_t backlogBytes = backlog.size() * capacity;
	int64_t backlogMB = backlogBytes / (1024 * 1024);

	return backlogMB;
}

void Writer::writeAndUnload(Node* node) {
	auto attributes = indexer->attributes;

	static int64_t counter = 0;
	counter += node->numPoints;

	int64_t numPoints = node->numPoints;
	int64_t byteSize = numPoints * attributes.bytes;

	node->byteSize = byteSize;

	shared_ptr<Buffer> buffer = nullptr;
	int64_t targetOffset = 0;
	{
		lock_guard<mutex> lock(mtx);

		int64_t byteOffset = indexer->byteOffset.fetch_add(byteSize);
		node->byteOffset = byteOffset;

		if (activeBuffer == nullptr) {
			activeBuffer = make_shared<Buffer>(capacity);
		} else if (activeBuffer->pos + byteSize > capacity) {
			backlog.push_back(activeBuffer);
			activeBuffer = make_shared<Buffer>(capacity);
		}

		buffer = activeBuffer;
		targetOffset = activeBuffer->pos;

		activeBuffer->pos += byteSize;
	}	

	memcpy(buffer->data_char + targetOffset, node->points->data_char, byteSize);

	node->points = nullptr;
}

void Writer::launchWriterThread() {
	thread([&]() {

		while (true) {

			shared_ptr<Buffer> buffer = nullptr;

			{
				lock_guard<mutex> lock(mtx);

				if (backlog.size() > 0) {
					buffer = backlog.front();
					backlog.pop_front();
				} else if (backlog.size() == 0 && closeRequested) {
					// DONE! No more work and close requested. quit thread.

					cvClose.notify_one();

					break;
				}
			}

			if (buffer != nullptr) {
				int64_t numBytes = buffer->pos;
				indexer->bytesWritten += numBytes;
				indexer->bytesToWrite -= numBytes;

				fsOctree.write(buffer->data_char, numBytes);

				indexer->bytesInMemory -= numBytes;
			} else {
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(10ms);
			}
			
		}

	}).detach();
}

void Writer::closeAndWait() {
	if (closed) {
		return;
	}

	unique_lock<mutex> lock(mtx);
	if (activeBuffer != nullptr) {
		backlog.push_back(activeBuffer);
	}

	closeRequested = true;
	cvClose.wait(lock);

	fsOctree.close();

}







void doIndexing(string targetDir, State& state, Options& options, Sampler& sampler) {

	cout << endl;
	cout << "=======================================" << endl;
	cout << "=== INDEXING                           " << endl;
	cout << "=======================================" << endl;

	auto tStart = now();

	state.name = "INDEXING";
	state.currentPass = 3;
	state.pointsProcessed = 0;
	state.bytesProcessed = 0;
	state.duration = 0;

	auto chunks = getChunks(targetDir);
	auto attributes = chunks->attributes;

	Indexer indexer(targetDir);
	indexer.attributes = attributes;
	indexer.root = make_shared<Node>("r", chunks->min, chunks->max);
	indexer.spacing = (chunks->max - chunks->min).x / 128.0;

	struct Task {
		shared_ptr<Chunk> chunk;

		Task(shared_ptr<Chunk> chunk) {
			this->chunk = chunk;
		}
	};

	int64_t totalPoints = 0;
	int64_t totalBytes = 0;
	for (auto chunk : chunks->list) {
		auto filesize = fs::file_size(chunk->file);
		totalPoints += filesize / attributes.bytes;
		totalBytes += filesize;
	}

	int64_t pointsProcessed = 0;
	double lastReport = now();

	auto writeAndUnload = [&indexer](Node* node) {
		indexer.writer->writeAndUnload(node);
	};

	atomic_int64_t activeThreads = 0;
	mutex mtx_nodes;
	vector<shared_ptr<Node>> nodes;
	TaskPool<Task> pool(numSampleThreads(), [&writeAndUnload, &state, &options, &activeThreads, tStart, &lastReport, &totalPoints, totalBytes, &pointsProcessed, chunks, &indexer, &nodes, &mtx_nodes, &sampler](auto task) {
		
		auto chunk = task->chunk;
		auto chunkRoot = make_shared<Node>(chunk->id, chunk->min, chunk->max);
		auto attributes = chunks->attributes;
		int bpp = attributes.bytes;

		indexer.waitUntilWriterBacklogBelow(1'000);
		activeThreads++;

		auto filesize = fs::file_size(chunk->file);
		indexer.bytesInMemory += filesize;
		auto pointBuffer = readBinaryFile(chunk->file);

		auto tStartChunking = now();

		if (!options.hasFlag("keep-chunks")) {
			fs::remove(chunk->file);
		}

		int64_t numPoints = pointBuffer->size / bpp;

		buildHierarchy(&indexer, chunkRoot.get(), pointBuffer, numPoints);

		auto onNodeCompleted = [&indexer](Node* node) {
			indexer.writer->writeAndUnload(node);
		};

		sampler.sample(chunkRoot, attributes, indexer.spacing, onNodeCompleted);

		indexer.flushChunkRoot(chunkRoot);

		// add chunk root, provided it isn't the root.
		if (chunkRoot->name.size() > 1) {
			indexer.root->addDescendant(chunkRoot);
		}

		lock_guard<mutex> lock(mtx_nodes);

		pointsProcessed = pointsProcessed + numPoints;
		double progress = double(pointsProcessed) / double(totalPoints);


		if (now() - lastReport > 1.0) {
			state.pointsProcessed = pointsProcessed;
			state.duration = now() - tStart;

			lastReport = now();
		}

		nodes.push_back(chunkRoot);

		//{ // debug
		//	auto duration = now() - tStartChunking;
		//	double throughput = (double(numPoints) / duration) / 1'000'000.0;
		//	string msg = "indexing chunk: " + formatNumber(duration, 3) + "s, " + formatNumber(numPoints) + " points, " + formatNumber(throughput, 1) + " MP/s \n";
		//	cout << msg;
		//}
		

		activeThreads--;
	});

	for (auto chunk : chunks->list) {
		auto task = make_shared<Task>(chunk);
		pool.addTask(task);
	}

	pool.waitTillEmpty();
	pool.close();

	indexer.reloadChunkRoots();

	if (chunks->list.size() == 1) {
		auto node = nodes[0];

		indexer.root = node;
	} else {

		auto onNodeCompleted = [&indexer](Node* node) {
			indexer.writer->writeAndUnload(node);
		};

		sampler.sample(indexer.root, attributes, indexer.spacing, onNodeCompleted);
	}
	
	indexer.writer->writeAndUnload(indexer.root.get());

	printElapsedTime("sampling", tStart);

	indexer.writer->closeAndWait();

	printElapsedTime("flushing", tStart);

	string hierarchyPath = targetDir + "/hierarchy.bin";
	Hierarchy hierarchy = indexer.createHierarchy(hierarchyPath);
	writeBinaryFile(hierarchyPath, hierarchy.buffer);

	string metadataPath = targetDir + "/metadata.json";
	string metadata = indexer.createMetadata(options, state, hierarchy);
	writeFile(metadataPath, metadata);

	printElapsedTime("metadata & hierarchy", tStart);

	double duration = now() - tStart;
	state.values["duration(indexing)"] = formatNumber(duration, 3);


}


}




