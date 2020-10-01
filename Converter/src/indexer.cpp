
#include <cerrno>
#include <execution>
#include <algorithm>

#include "indexer.h"

#include "Attributes.h"
#include "logger.h"
#include "PotreeConverter.h"
#include "DbgWriter.h"
#include "brotli/encode.h"

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

		for (int64_t i = 0; i < 8; i++) {
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
			int64_t size = jsAttribute["size"];
			int64_t numElements = jsAttribute["numElements"];
			int64_t elementSize = jsAttribute["elementSize"];
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

		logger::INFO("start reloadChunkRoots");

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

		logger::INFO("end reloadChunkRoots");
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
	ss << t(1) << s("encoding") << ": " << s(options.encoding) << "," << endl;
	ss << t(1) << s("attributes") << ": " << getAttributesJsonString() << endl;
	ss << t(0) << "}" << endl;

	string str = ss.str();


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
void buildHierarchy(Indexer* indexer, Node* node, shared_ptr<Buffer> points, int64_t numPoints, int64_t depth = 0) {

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
	int64_t bpp = attributes.bytes;
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

		ix = std::max(int64_t(0), std::min(ix, counterGridSize - 1));
		iy = std::max(int64_t(0), std::min(iy, counterGridSize - 1));
		iz = std::max(int64_t(0), std::min(iz, counterGridSize - 1));

		int64_t index = mortonEncode_magicbits(iz, iy, ix);

		return index;
	};

	// COUNTING
	for (int64_t i = 0; i < numPoints; i++) {
		auto index = gridIndexOf(i);
		counters[index]++;
	}

	{ // DISTRIBUTING
		vector<int64_t> offsets(counters.size(), 0);
		for (int64_t i = 1; i < counters.size(); i++) {
			offsets[i] = offsets[i - 1] + counters[i - 1];
		}

		if(numPoints * bpp < 0){
			stringstream ss;

			auto size = numPoints * bpp;
			ss << "invalid call to malloc(" << to_string(size) << ")\n";
			ss << "in function buildHierarchy()\n";
			ss << "node: " << node->name << "\n";
			ss << "#points: " << node->numPoints<< "\n";
			ss << "min: " << node->min.toString() << "\n";
			ss << "max: " << node->max.toString() << "\n";

			logger::ERROR(ss.str());
		}

		Buffer tmp(numPoints * bpp);

		for (int64_t i = 0; i < numPoints; i++) {
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
		for (int64_t i = startName.size(); i < fullName.size(); i++) {
			int64_t index = fullName.at(i) - '0';

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

		if (bytes < 0) {
			stringstream ss;

			ss << "invalid call to malloc(" << to_string(bytes) << ")\n";
			ss << "in function buildHierarchy()\n";
			ss << "node: " << node->name << "\n";
			ss << "#points: " << node->numPoints << "\n";
			ss << "min: " << node->min.toString() << "\n";
			ss << "max: " << node->max.toString() << "\n";

			logger::ERROR(ss.str());
		}

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

	int64_t sanityCheck = 0;
	for (int64_t nodeIndex = 0; nodeIndex < needRefinement.size(); nodeIndex++) {
		auto subject = needRefinement[nodeIndex];
		auto buffer = subject->points;
		
		if (sanityCheck > needRefinement.size() * 2) {
			logger::ERROR("failed to partition point cloud in indexer::buildHierarchy().");
		}

		if (subject->numPoints == numPoints) {
			// the subsplit has the same number of points than the input -> ERROR

			unordered_map<string, int> counters;

			auto bpp = attributes.bytes;

			for (int64_t i = 0; i < numPoints; i++) {

				int64_t sourceOffset = i * bpp;

				int32_t X, Y, Z;
				memcpy(&X, buffer->data_u8 + sourceOffset + 0, 4);
				memcpy(&Y, buffer->data_u8 + sourceOffset + 4, 4);
				memcpy(&Z, buffer->data_u8 + sourceOffset + 8, 4);

				stringstream ss;
				ss << X << ", " << Y << ", " << Z;

				string key = ss.str();
				counters[key]++;
			}

			int64_t numPointsInBox = subject->numPoints;
			int64_t numUniquePoints = counters.size();
			int64_t numDuplicates = numPointsInBox - numUniquePoints;

			if (numDuplicates < maxPointsPerChunk / 2) {
				// few uniques, just unfavouribly distributed points
				// print warning but continue

				stringstream ss;	
				ss << "Encountered unfavourable point distribution. Conversion continues anyway because not many duplicates were encountered. ";
				ss << "However, issues may arise. If you find an error, please report it at github. \n";
				ss << "#points in box: " << numPointsInBox << ", #unique points in box: " << numUniquePoints << ", ";
				ss << "min: " << subject->min.toString() << ", max: " << subject->max.toString();

				logger::WARN(ss.str());
			} else {

				// remove the duplicates, then try again

				vector<int64_t> distinct;
				unordered_map<string, int> handled;

				auto contains = [](auto map, auto key) {
					return map.find(key) != map.end();
				};

				for (int64_t i = 0; i < numPoints; i++) {

					int64_t sourceOffset = i * bpp;

					int32_t X, Y, Z;
					memcpy(&X, buffer->data_u8 + sourceOffset + 0, 4);
					memcpy(&Y, buffer->data_u8 + sourceOffset + 4, 4);
					memcpy(&Z, buffer->data_u8 + sourceOffset + 8, 4);

					stringstream ss;
					ss << X << ", " << Y << ", " << Z;

					string key = ss.str();
					
					if (contains(counters, key)) {
						if (!contains(handled, key)) {
							distinct.push_back(i);
							handled[key] = true;
						}
					} else {
						distinct.push_back(i);
					}

				}

				//cout << "#distinct: " << distinct.size() << endl;

				stringstream msg;
				msg << "Too many duplicate points were encountered. #points: " << subject->numPoints;
				msg << ", #unique points: " << distinct.size() << endl;
				msg << "Duplicates inside node will be dropped! ";
				msg << "min: " << subject->min.toString() << ", max: " << subject->max.toString();

				logger::WARN(msg.str());

				shared_ptr<Buffer> distinctBuffer = make_shared<Buffer>(distinct.size() * bpp);

				for(int64_t i = 0; i < distinct.size(); i++){
					distinctBuffer->write(buffer->data_u8 + i * bpp, bpp);
				}

				subject->points = distinctBuffer;
				subject->numPoints = distinct.size();

				// try again
				nodeIndex--;
			}

		}

		int64_t nextNumPoins = subject->numPoints;

		subject->points = nullptr;
		subject->numPoints = 0;

		buildHierarchy(indexer, subject, buffer, nextNumPoins, depth + 1);
	}

}

struct MortonCode {
	uint64_t lower;
	uint64_t upper;
	uint64_t whatever;
	uint64_t index;
};

struct SoA {
	unordered_map<string, shared_ptr<Buffer>> buffers;
	vector<MortonCode> mcs;
};

SoA toStructOfArrays(Node* node, Attributes attributes) {

	auto numPoints = node->numPoints;
	uint8_t* source = node->points->data_u8;

	unordered_map<string, shared_ptr<Buffer>> buffers;
	vector<MortonCode> mcs;

	for (Attribute attribute : attributes.list) {

		int64_t bytes = attribute.size * numPoints;
		//auto buffer = make_shared<Buffer>(bytes);
		auto attributeOffset = attributes.getOffset(attribute.name);

		if (attribute.name == "rgb") {

			auto bufferMC = make_shared<Buffer>(8 * numPoints);

			for (int64_t i = 0; i < numPoints; i++) {

				int64_t pointOffset = i * attributes.bytes;


				uint16_t r, g, b;
				memcpy(&r, source + pointOffset + attributeOffset + 0, 2);
				memcpy(&g, source + pointOffset + attributeOffset + 2, 2);
				memcpy(&b, source + pointOffset + attributeOffset + 4, 2);


				auto mc = mortonEncode_magicbits(r, g, b);
				bufferMC->write(&mc, 8);
			}

			buffers["rgb_morton"] = bufferMC;

		} else if (attribute.name == "position"){

			struct P {
				int32_t x, y, z;
			};
			vector<P> ps;
			P min;
			min.x = std::numeric_limits<int64_t>::max();
			min.y = std::numeric_limits<int64_t>::max();
			min.z = std::numeric_limits<int64_t>::max();
		
			for (int64_t i = 0; i < numPoints; i++) {

				int64_t pointOffset = i * attributes.bytes;

				// MORTON

				int32_t XYZ[3];
				memcpy(XYZ, source + pointOffset + attributeOffset, 12);

				P p;
				p.x = XYZ[0];
				p.y = XYZ[1];
				p.z = XYZ[2];

				min.x = std::min(min.x, p.x);
				min.y = std::min(min.y, p.y);
				min.z = std::min(min.z, p.z);

				ps.push_back(p);
			}


			int64_t i = 0;
			for (P p : ps) {

				uint32_t mx = p.x - min.x;
				uint32_t my = p.y - min.y;
				uint32_t mz = p.z - min.z;

				uint32_t mx_l = (mx & 0x0000'ffff);
				uint32_t my_l = (my & 0x0000'ffff);
				uint32_t mz_l = (mz & 0x0000'ffff);

				uint32_t mx_h = mx >> 16;
				uint32_t my_h = my >> 16;
				uint32_t mz_h = mz >> 16;

				auto mc_l = mortonEncode_magicbits(mx_l, my_l, mz_l);
				auto mc_h = mortonEncode_magicbits(mx_h, my_h, mz_h);

				{ // try decode and compare

					uint32_t x_decoded = 0;
					uint32_t y_decoded = 0;
					uint32_t z_decoded = 0;

					for (int i = 0; i < 21; i++) {

						uint64_t mask = (mc_l >> (3 * i)) & 0b111;

						x_decoded = x_decoded | (((mask >> 0) & 0b001) << i);
						y_decoded = y_decoded | (((mask >> 1) & 0b001) << i);
						z_decoded = z_decoded | (((mask >> 2) & 0b001) << i);


					}

					bool okayX = x_decoded == mx_l;
					bool okayY = y_decoded == my_l;
					bool okayZ = z_decoded == mz_l;

					if (!okayX || !okayY || !okayZ) {
						int a = 10;

						cout << "could not revert morton code!!!" << endl;

						exit(123);
					}

				}


				MortonCode mc;
				mc.lower = mc_l;
				mc.upper = mc_h;
				mc.whatever = mortonEncode_magicbits(mx, my, mz);
				mc.index = i;

				mcs.push_back(mc);

				i++;

			}

			{
				auto bufferMc = make_shared<Buffer>(16 * numPoints);

				for (int i = 0; i < numPoints; i++) {
					auto mc = mcs[i];

					bufferMc->write(&mc.upper, 8);
					bufferMc->write(&mc.lower, 8);
				}


				
				buffers["position_morton"] = bufferMc;
			}


		
		} 

		{

			auto buffer = make_shared<Buffer>(bytes);

			for (int64_t i = 0; i < numPoints; i++) {

				int64_t pointOffset = i * attributes.bytes;

				buffer->write(source + pointOffset + attributeOffset, attribute.size);
			}

			buffers[attribute.name] = buffer;
		}

		

		//vector<uint8_t> dbg1(buffer->data_u8, buffer->data_u8 + buffer->size);

	}

	SoA soa;
	soa.buffers = buffers;
	soa.mcs = mcs;

	return soa;
}




//static int64_t totalUncompressed = 0;
//static int64_t totalCompressed = 0;
//static unordered_map<string, int64_t> uncompressedCounters;
//static unordered_map<string, int64_t> compressedCounters;
//static mutex mtx_dbg_compress;

shared_ptr<Buffer> compress(Node* node, Attributes attributes) {

	auto numPoints = node->numPoints;
	auto soa = toStructOfArrays(node, attributes);

	std::sort(soa.mcs.begin(), soa.mcs.end(), [](MortonCode& a, MortonCode& b) {

		if (a.upper == b.upper) {
			return a.lower < b.lower;
		} else {
			return a.upper < b.upper;
		}

	});

	auto mapName = [](string name) {
		if (name == "position") {
			return string("position_morton");
		} else if (name == "rgb") {
			return string("rgb_morton");
		} else {
			return name;
		}
	};

	int64_t bufferSize = 0;
	for (Attribute& attribute : attributes.list) {
		string name = mapName(attribute.name);
		auto buffer = soa.buffers[name];

		bufferSize += buffer->size;
	}

	auto bufferMerged = make_shared<Buffer>(bufferSize);
	for (Attribute& attribute : attributes.list) {

		string name = mapName(attribute.name);

		auto buffer = soa.buffers[name];

		int64_t bufferAttributeSize = buffer->size / numPoints;

		for (int i = 0; i < numPoints; i++) {
			int sourceIndex = soa.mcs[i].index;

			bufferMerged->write(buffer->data_u8 + sourceIndex * bufferAttributeSize, bufferAttributeSize);
		}
	}

	shared_ptr<Buffer> out;
	{
		auto buffer = bufferMerged;

		int quality = 6;
		int lgwin = BROTLI_DEFAULT_WINDOW;
		auto mode = BROTLI_DEFAULT_MODE;
		uint8_t* input_buffer = buffer->data_u8;
		size_t input_size = buffer->size;

		size_t encoded_size = input_size * 1.5 + 1'000;
		shared_ptr<Buffer> outputBuffer = make_shared<Buffer>(encoded_size);
		uint8_t* encoded_buffer = outputBuffer->data_u8;

		BROTLI_BOOL success = BROTLI_FALSE;

		for (int i = 0; i < 5; i++) {
			success = BrotliEncoderCompress(quality, lgwin, mode, input_size, input_buffer, &encoded_size, encoded_buffer);

			if (success == BROTLI_TRUE) {
				break;
			} else {
				encoded_size = (encoded_size + 1024) * 1.5;
				outputBuffer = make_shared<Buffer>(encoded_size);
				encoded_buffer = outputBuffer->data_u8;

				logger::WARN("reserved encoded_buffer size was too small. Trying again with size " + formatNumber(encoded_size) + ".");
			}
		}

		if (success == BROTLI_FALSE) {
			stringstream ss;
			ss << "failed to compress node " << node->name << ". aborting conversion." ;
			logger::ERROR(ss.str());

			exit(123);
		}

		out = make_shared<Buffer>(encoded_size);
		memcpy(out->data, encoded_buffer, encoded_size);
		
		//{ // DEBUG
		//	lock_guard<mutex> lock(mtx_dbg_compress);

		//	totalUncompressed += input_size;
		//	totalCompressed += encoded_size;
		//}
	}

	//{
	//	lock_guard<mutex> lock(mtx_dbg_compress);

	//	static int i = 0;
	//	if ((i % 100) == 0) {

	//		stringstream ss;
	//		ss << "===================================================" << endl;

	//		{
	//			double ratio = double(totalCompressed) / double(totalUncompressed);

	//			ss << "[total] " << formatNumber(totalUncompressed) << " > " << formatNumber(totalCompressed) << " - " << formatNumber(100.0 * ratio, 1) << endl;
	//			cout << ss.str();
	//		}

	//		cout << ss.str();

	//	}
	//	i++;


	//}

	return out;
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
	string encoding = indexer->options.encoding;

	shared_ptr<Buffer> sourceBuffer;

	if (encoding == "BROTLI") {
		sourceBuffer = compress(node, attributes);
	} else {
		sourceBuffer = node->points;
	}
	

	int64_t byteSize = sourceBuffer->size;

	node->byteSize = byteSize;

	auto errorCheck = [node](int64_t size) {
		if (size < 0) {
			stringstream ss;

			ss << "invalid call to malloc(" << to_string(size) << ")\n";
			ss << "in function writeAndUnload()\n";
			ss << "node: " << node->name << "\n";
			ss << "#points: " << node->numPoints << "\n";
			ss << "min: " << node->min.toString() << "\n";
			ss << "max: " << node->max.toString() << "\n";

			logger::ERROR(ss.str());
		}
	};

	shared_ptr<Buffer> buffer = nullptr;
	int64_t targetOffset = 0;
	{
		lock_guard<mutex> lock(mtx);

		int64_t byteOffset = indexer->byteOffset.fetch_add(byteSize);
		node->byteOffset = byteOffset;

		if (activeBuffer == nullptr) {
			errorCheck(capacity);
			activeBuffer = make_shared<Buffer>(capacity);
		} else if (activeBuffer->pos + byteSize > capacity) {
			backlog.push_back(activeBuffer);

			capacity = std::max(capacity, byteSize);
			errorCheck(capacity);
			activeBuffer = make_shared<Buffer>(capacity);
		}

		buffer = activeBuffer;
		targetOffset = activeBuffer->pos;

		activeBuffer->pos += byteSize;
	}	

	memcpy(buffer->data_char + targetOffset, sourceBuffer->data, byteSize);

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
	indexer.options = options;
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
		int64_t bpp = attributes.bytes;

		indexer.waitUntilWriterBacklogBelow(1'000);
		activeThreads++;

		auto filesize = fs::file_size(chunk->file);

		stringstream msg;
		msg << "start indexing chunk " + chunk->id << "\n";
		msg << "filesize: " << formatNumber(filesize) << "\n";
		msg << "min: " << chunk->min.toString() << "\n";
		msg << "max: " << chunk->max.toString();
		logger::INFO(msg.str());

		indexer.bytesInMemory += filesize;
		auto pointBuffer = readBinaryFile(chunk->file);

		auto tStartChunking = now();

		if (!options.keepChunks) {
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

		logger::INFO("finished indexing chunk " + chunk->id);

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

	{
		cout << "deleting temporary files" << endl;

		// delete chunk directory
		if (!options.keepChunks) {
			string chunksMetadataPath = targetDir + "/chunks/metadata.json";

			fs::remove(chunksMetadataPath);
			fs::remove(targetDir + "/chunks");
		}

		// delete chunk roots data
		string octreePath = targetDir + "/tmpChunkRoots.bin";
		fs::remove(octreePath);
	}

	double duration = now() - tStart;
	state.values["duration(indexing)"] = formatNumber(duration, 3);


}


}




