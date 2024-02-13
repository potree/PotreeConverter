
#include <cerrno>
#include <execution>
#include <algorithm>
#include <format>
#include <atomic>

#include "indexer.h"

#include "Attributes.h"
#include "PotreeConverter.h"
#include "DbgWriter.h"
#include "brotli/encode.h"
#include "HierarchyBuilder.h"
#include "sampler_weighted.h"
#include "OctreeSerializer.h"

using std::unique_lock;

// Metadata is written to the start of the file after the conversion,
// so we reserve a sizeable amount (100kb) at the beginning, and modify at the end
constexpr uint32_t METADATA_CAPACITY_BYTES = 100 * 1024;

namespace indexer{

	constexpr int hierarchyStepSize = 6;
	static vector<uint64_t> byteOffsets;
	static vector<uint64_t> unfilteredByteOffsets;

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
			auto jsScale = jsAttribute["scale"];
			auto jsOffset = jsAttribute["offset"];

			// int64_t mask = 0;
			// if(jsAttribute.contains("mask")){
			// 	mask = jsAttribute["mask"];
			// }

			vector<int64_t> histogram(256, 0);
			if(jsAttribute.contains("histogram")){
				auto jsHistogram = jsAttribute["histogram"];

				for(int i = 0; i < jsHistogram.size(); i++){
					histogram[i] = jsHistogram[i];
				}
			}

			Attribute attribute(name, size, numElements, elementSize, type);
			attribute.description = description;
			// attribute.mask = mask;
			attribute.histogram = histogram;

			if (numElements >= 1) {
				attribute.min.x = jsMin[0] == nullptr ? Infinity : double(jsMin[0]);
				attribute.max.x = jsMax[0] == nullptr ? Infinity : double(jsMax[0]);
				attribute.scale.x = jsScale[0] == nullptr ? 1.0 : double(jsScale[0]);
				attribute.offset.x = jsOffset[0] == nullptr ? 0.0 : double(jsOffset[0]);
			}
			if (numElements >= 2) {
				attribute.min.y = jsMin[1] == nullptr ? Infinity : double(jsMin[1]);
				attribute.max.y = jsMax[1] == nullptr ? Infinity : double(jsMax[1]);
				attribute.scale.y = jsScale[1] == nullptr ? 1.0 : double(jsScale[1]);
				attribute.offset.y = jsOffset[1] == nullptr ? 0.0 : double(jsOffset[1]);
			}
			if (numElements >= 3) {
				attribute.min.z = jsMin[2] == nullptr ? Infinity : double(jsMin[2]);
				attribute.max.z = jsMax[2] == nullptr ? Infinity : double(jsMax[2]);
				attribute.scale.z = jsScale[2] == nullptr ? 1.0 : double(jsScale[2]);
				attribute.offset.z = jsOffset[2] == nullptr ? 0.0 : double(jsOffset[2]);
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
		int64_t size = 0;
		
		if(chunkRoot->numPoints > 0){
			size = chunkRoot->points->size;
			fChunkRoots.write(chunkRoot->points->data_char, size);
		}else if(chunkRoot->voxels.size() > 0){
			fChunkRoots.write((const char*)chunkRoot->voxels.data(), chunkRoot->voxels.size() * sizeof(Voxel));
			size += chunkRoot->voxels.size() * sizeof(Voxel);
			
			fChunkRoots.write(chunkRoot->unfilteredVoxelData->data_char, chunkRoot->unfilteredVoxelData->size);
			size += chunkRoot->unfilteredVoxelData->size;
		}

		FlushedChunkRoot fcr;
		fcr.node = chunkRoot;
		fcr.offset = offset;
		fcr.size = size;

		chunkRoot->points = nullptr;
		chunkRoot->voxels = vector<Voxel>();

		flushedChunkRoots.push_back(fcr);

		offset += size;
	}

	vector<CRNode> Indexer::processChunkRoots(){

		unordered_map<string, shared_ptr<CRNode>> nodesMap;
		vector<shared_ptr<CRNode>> nodesList;

		// create/copy nodes
		this->root->traverse([&nodesMap, &nodesList](Node* node){
			auto crnode = make_shared<CRNode>();
			crnode->name = node->name;
			crnode->node = node;
			crnode->children.resize(node->children.size());

			nodesList.push_back(crnode);
			nodesMap[crnode->name] = crnode;
		});

		// establish hierarchy
		for(auto crnode : nodesList){

			string parentName = crnode->name.substr(0, crnode->name.size() - 1);

			if(parentName != ""){
				auto parent = nodesMap[parentName];
				int index = crnode->name.at(crnode->name.size() - 1) - '0';

				parent->children[index] = crnode;
			}
		}

		// mark/flag/insert flushed chunk roots
		for(auto fcr : flushedChunkRoots){
			auto node = nodesMap[fcr.node->name];
			
			node->fcrs.push_back(fcr);
			// why += ?
			node->numPoints += fcr.node->numPoints;
			node->numVoxels += fcr.node->numVoxels;
		}

		// recursively merge leaves if sum(points) < threshold
		auto cr_root = nodesMap["r"];
		static int64_t threshold = 5'000'000;

		cr_root->traversePost([](CRNode* node){
			
			if(node->isLeaf()){

			}else{
				
				int numSamples = 0;
				int numPoints = 0;
				int numVoxels = 0;

				for(auto child : node->children){
					if(!child) continue;

					numPoints += child->numPoints;
					numVoxels += child->numVoxels;
					numSamples += child->numPoints + child->numVoxels;
				}

				node->numPoints = numPoints;
				node->numVoxels = numVoxels;
				node->numSamples = numSamples;

				if(node->numSamples < threshold){
					// merge children into this node
					for(auto child : node->children){
						if(!child) continue;

						node->fcrs.insert(node->fcrs.end(), child->fcrs.begin(), child->fcrs.end());
					}

					node->children.clear();
				}
			}
		});

		vector<CRNode> tasks;
		cr_root->traverse([&tasks](CRNode* node){
			if(node->fcrs.size() > 0){
				CRNode crnode = *node;
				tasks.push_back(crnode);
			}
		});

		return tasks;
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
	};

	auto vecI64ToJson = [](vector<int64_t> &values) {

		stringstream ss;
		ss << "[";

		for (int i = 0; i < values.size(); i++) {

			ss << values[i];

			if (i < values.size() - 1) {
				ss << ", ";
			}
		}
		ss << "]";

		return ss.str();
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
	auto getAttributesJsonString = [&attributes, t, s, toJson, vecToJson, vecI64ToJson]() {

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

			bool emptyHistogram = true;
			for(int i = 0; i < attribute.histogram.size(); i++){
				if(attribute.histogram[i] != 0){
					emptyHistogram = false;
				}
			}

			 if(attribute.size == 1 && !emptyHistogram){
			 	ss << t(3) << s("histogram") << ": " << vecI64ToJson(attribute.histogram) << ", " << endl;
			 }

			if (attribute.numElements == 1) {
				ss << t(3) << s("min") << ": " << vecToJson(vector<double>{ attribute.min.x }) << "," << endl;
				ss << t(3) << s("max") << ": " << vecToJson(vector<double>{ attribute.max.x }) << ","<< endl;
				ss << t(3) << s("scale") << ": " << vecToJson(vector<double>{ attribute.scale.x }) << ","<< endl;
				ss << t(3) << s("offset") << ": " << vecToJson(vector<double>{ attribute.offset.x }) << endl;
			} else if (attribute.numElements == 2) {
				ss << t(3) << s("min") << ": " << vecToJson(vector<double>{ attribute.min.x, attribute.min.y }) << "," << endl;
				ss << t(3) << s("max") << ": " << vecToJson(vector<double>{ attribute.max.x, attribute.max.y }) << ","<< endl;
				ss << t(3) << s("scale") << ": " << vecToJson(vector<double>{ attribute.scale.x, attribute.scale.y }) << ","<< endl;
				ss << t(3) << s("offset") << ": " << vecToJson(vector<double>{ attribute.offset.x, attribute.offset.y }) << endl;
			} else if (attribute.numElements == 3) {
				ss << t(3) << s("min") << ": " << vecToJson(vector<double>{ attribute.min.x, attribute.min.y, attribute.min.z }) << "," << endl;
				ss << t(3) << s("max") << ": " << vecToJson(vector<double>{ attribute.max.x, attribute.max.y, attribute.max.z }) << ","<< endl;
				ss << t(3) << s("scale") << ": " << vecToJson(vector<double>{ attribute.scale.x, attribute.scale.y, attribute.scale.z }) << ","<< endl;
				ss << t(3) << s("offset") << ": " << vecToJson(vector<double>{ attribute.offset.x, attribute.offset.y, attribute.offset.z }) << endl;
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
	ss << t(1) << s("projection") << ": " << s(options.projection) << "," << endl;
	ss << t(1) << s("hierarchy") << ": " << getHierarchyJsonString() << "," << endl;
	ss << t(1) << s("offset") << ": " << toJson(attributes.posOffset) << "," << endl;
	ss << t(1) << s("scale") << ": " << toJson(attributes.posScale) << "," << endl;
	ss << t(1) << s("spacing") << ": " << d(spacing) << "," << endl;
	ss << t(1) << s("boundingBox") << ": " << getBoundingBoxJsonString() << "," << endl;
	ss << t(1) << s("encoding") << ": " << s(options.encoding) << "," << endl;
	ss << t(1) << std::format(R"("pointBuffer":     {{"offset": {:12}, "size": {:12}}},)", state.pointBufferOffset, state.pointBufferSize) << "\n";
	ss << t(1) << std::format(R"("hierarchyBuffer": {{"offset": {:12}, "size": {:12}}},)", state.hierarchyBufferOffset, state.hierarchyBufferSize) << "\n";
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

			uint8_t childMask           = childMaskOf(node);
			uint64_t targetOffset       = 0;
			uint32_t byteSize           = 0;
			uint32_t byteSizePosition   = 0;
			uint32_t byteSizeFiltered   = 0;
			uint32_t byteSizeUnfiltered = 0;
			uint32_t numPoints          = uint32_t(node->numPoints);
			uint8_t type                = node->isLeaf() ? TYPE::LEAF : TYPE::NORMAL;

			if (isProxy) {
				int targetChunkIndex = chunkPointers[node->name];
				auto targetChunk = chunks[targetChunkIndex];

				type = TYPE::PROXY;
				targetOffset = chunkByteOffsets[targetChunkIndex];
				byteSize = chunkSize(targetChunk);
			} else {
				targetOffset = node->byteOffset;
				
				if(node->numPoints > 0){
					byteSize = node->sPointsSize;
				}else{
					byteSizePosition = node->sPositionSize;
					byteSizeFiltered = node->sFilteredSize;
					byteSizeUnfiltered = node->sUnfilteredSize;
					byteSize = byteSizePosition + byteSizeFiltered;
				}
			}

			// 30 bytes
			memcpy(hierarchyBuffer.data() + offset +  0, &type,                   1);
			memcpy(hierarchyBuffer.data() + offset +  1, &childMask,              1);
			memcpy(hierarchyBuffer.data() + offset +  2, &numPoints,              4);
			memcpy(hierarchyBuffer.data() + offset +  6, &targetOffset,           8);
			memcpy(hierarchyBuffer.data() + offset + 14, &byteSize,               4);
			memcpy(hierarchyBuffer.data() + offset + 18, &byteSizePosition,       4);
			memcpy(hierarchyBuffer.data() + offset + 22, &byteSizeFiltered,       4);
			memcpy(hierarchyBuffer.data() + offset + 26, &byteSizeUnfiltered,     4);

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

			printfmt("ERROR: {} \n", ss.str());
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

			printfmt("ERROR: {} \n", ss.str());
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
			printfmt("ERROR: failed to partition point cloud in indexer::buildHierarchy().");
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

				printfmt("WARNING: {} \n", ss.str());
			} else {

				// remove the duplicates, then try again

				vector<int64_t> distinct;
				unordered_map<string, int> handled;

				auto contains = [](auto const & map, auto const & key) {
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

				printfmt("WARNING: {} \n", msg.str());

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

				//{ // try decode and compare

				//	uint32_t x_decoded = 0;
				//	uint32_t y_decoded = 0;
				//	uint32_t z_decoded = 0;

				//	for (int i = 0; i < 21; i++) {

				//		uint64_t mask = (mc_l >> (3 * i)) & 0b111;

				//		x_decoded = x_decoded | (((mask >> 0) & 0b001) << i);
				//		y_decoded = y_decoded | (((mask >> 1) & 0b001) << i);
				//		z_decoded = z_decoded | (((mask >> 2) & 0b001) << i);


				//	}

				//	bool okayX = x_decoded == mx_l;
				//	bool okayY = y_decoded == my_l;
				//	bool okayZ = z_decoded == mz_l;

				//	if (!okayX || !okayY || !okayZ) {

				//		cout << "could not revert morton code!!!" << endl;

				//		exit(123);
				//	}

				//}


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

				printfmt("WARNING: reserved encoded_buffer size was too small. Trying again with size {} \n", formatNumber(encoded_size));
			}
		}

		if (success == BROTLI_FALSE) {
			printfmt("ERROR: failed to compress node {}. aborting conversion. \n", node->name);

			exit(123);
		}

		out = make_shared<Buffer>(encoded_size);
		memcpy(out->data, encoded_buffer, encoded_size);
	}

	return out;
}



Writer::Writer(Indexer* indexer) {
	this->indexer = indexer;

	fsPotree.open(indexer->targetPath, ios::out | ios::binary);

	// reserve first <METADATA_CAPACITY_BYTES>, 
	// to be filled with metadata at the end
	uint8_t data[METADATA_CAPACITY_BYTES];
	memset(data, 0, METADATA_CAPACITY_BYTES);
	fsPotree.write((const char*)data, METADATA_CAPACITY_BYTES);

	launchWriterThread();
}

mutex mtx_backlog;
int64_t Writer::backlogSizeMB() {
	lock_guard<mutex> lock(mtx_backlog);

	int64_t backlogBytes = backlog.size() * capacity;
	int64_t backlogMB = backlogBytes / (1024 * 1024);

	return backlogMB;
}

// - writes children of <node> into one consecutive byte region
// - If it <node> is the root, it also writes the root node to the file
void Writer::writeAndUnload(Node* node) {

	//if(node->serializedBuffer == nullptr) return;

	auto attributes = indexer->attributes;
	string encoding = indexer->options.encoding;

	lock_guard<mutex> lock(mtx);

	// shared_ptr<Buffer> sourceBuffer;
	// if (encoding == "BROTLI") {
	// 	sourceBuffer = compress(node, attributes);
	// } else {
	// 	sourceBuffer = node->points;
	// }

	// if(node->unfilteredVoxelData)
	// { // DEBUG
	// 	vector<uint16_t> dbg(10, 0);
	// 	memcpy(dbg.data(), node->unfilteredVoxelData->data, 20);

	// 	printfmt("writeAndUnload - start {}: {}, {}, {}, {}, {} \n", node->name, dbg[0], dbg[1], dbg[2], dbg[3], dbg[4]);
	// }

	if (node->isLeaf()) {
		return;
	}


	vector<Node*> nodesToWrite;
	if(node->name == "r"){
		nodesToWrite.push_back(node);
	}
	for(shared_ptr<Node> child : node->children){
		if(child == nullptr) continue;
		nodesToWrite.push_back(child.get());
	}


	int64_t chunkByteSize = 0;

	uint64_t maxSerializationIndex = 0;
	for(Node* node : nodesToWrite){
		chunkByteSize += node->serializedPoints       ? node->serializedPoints->size        : 0;
		chunkByteSize += node->serializedPosition     ? node->serializedPosition->size      : 0;
		chunkByteSize += node->serializedFiltered     ? node->serializedFiltered->size      : 0;
		chunkByteSize += node->serializedUnfiltered   ? node->serializedUnfiltered->size    : 0;

		maxSerializationIndex = max(maxSerializationIndex, node->serializationIndex);
	}

	uint64_t chunkByteOffset = indexer->byteOffset.fetch_add(chunkByteSize);
	shared_ptr<Buffer> chunkBuffer = make_shared<Buffer>(chunkByteSize);

	if(byteOffsets.size() <= maxSerializationIndex){
		byteOffsets.resize(1.5f * float(maxSerializationIndex));
	}
	if(unfilteredByteOffsets.size() <= maxSerializationIndex){
		unfilteredByteOffsets.resize(1.5f * float(maxSerializationIndex));
	}

	uint64_t chunkBytesProcessed = 0;

	// POINTS
	for(Node* node : nodesToWrite){

		if(node->serializedPoints){
			uint64_t nodeByteOffset = chunkByteOffset + chunkBytesProcessed;
			byteOffsets[node->serializationIndex] = nodeByteOffset;

			memcpy(
				chunkBuffer->data_u8 + chunkBytesProcessed, 
				node->serializedPoints->data,
				node->serializedPoints->size
			);
			chunkBytesProcessed += node->serializedPoints->size;
		}
	}

	// VOXEL POSITION AND FILTERED DATA
	for(Node* node : nodesToWrite){
		if(node->serializedPosition){
			uint64_t nodeByteOffset = chunkByteOffset + chunkBytesProcessed;
			byteOffsets[node->serializationIndex] = nodeByteOffset;

			memcpy(
				chunkBuffer->data_u8 + chunkBytesProcessed, 
				node->serializedPosition->data, 
				node->serializedPosition->size
			);
			chunkBytesProcessed += node->serializedPosition->size;

			memcpy(
				chunkBuffer->data_u8 + chunkBytesProcessed, 
				node->serializedFiltered->data, 
				node->serializedFiltered->size
			);
			chunkBytesProcessed += node->serializedFiltered->size;
		}
	}

	// VOXEL UNFILTERED DATA
	for(Node* node : nodesToWrite){
		if(node->serializedUnfiltered){
			uint64_t nodeByteOffset = chunkByteOffset + chunkBytesProcessed;
			unfilteredByteOffsets[node->serializationIndex] = nodeByteOffset;

			memcpy(
				chunkBuffer->data_u8 + chunkBytesProcessed, 
				node->serializedUnfiltered->data, 
				node->serializedUnfiltered->size
			);
			chunkBytesProcessed += node->serializedUnfiltered->size;
		}
	}

	// DEBUG INFOS
	// for(Node* node : nodesToWrite){
	// 	if(node->name.size() <= 3){

	// 		int64_t nodeByteSize = 0;
	// 		nodeByteSize += node->serializedPoints       ? node->serializedPoints->size        : 0;
	// 		nodeByteSize += node->serializedPosition     ? node->serializedPosition->size      : 0;
	// 		nodeByteSize += node->serializedFiltered     ? node->serializedFiltered->size      : 0;
	// 		nodeByteSize += node->serializedUnfiltered   ? node->serializedUnfiltered->size    : 0;

	// 		printfmt("[{:5}] offsets: {:11L}, unfiltered: {:11L} | sizes: points: {:11L}, position: {:11L}, filtered: {:11L}, unfiltered: {:11L} \n",
	// 			node->name, byteOffsets[node->serializationIndex], unfilteredByteOffsets[node->serializationIndex],
	// 			(node->serializedPoints       ? node->serializedPoints->size        : 0),
	// 			(node->serializedPosition     ? node->serializedPosition->size      : 0),
	// 			(node->serializedFiltered     ? node->serializedFiltered->size      : 0),
	// 			(node->serializedUnfiltered   ? node->serializedUnfiltered->size    : 0)
	// 		);
	// 	}
	// }

	auto errorCheck = [node](int64_t size) {
		if (size < 0) {
			stringstream ss;

			ss << "invalid call to malloc(" << to_string(size) << ")\n";
			ss << "in function writeAndUnload()\n";
			ss << "node: " << node->name << "\n";
			ss << "#points: " << node->numPoints << "\n";
			ss << "min: " << node->min.toString() << "\n";
			ss << "max: " << node->max.toString() << "\n";

			printfmt("ERROR: {} \n", ss.str());
		}
	};

	shared_ptr<Buffer> buffer = nullptr;
	int64_t targetOffset = 0;
	{
		if (activeBuffer == nullptr) {
			errorCheck(capacity);
			activeBuffer = make_shared<Buffer>(capacity);
		} else if (activeBuffer->pos + chunkByteSize > capacity) {
			backlog.push_back(activeBuffer);

			capacity = std::max(capacity, int64_t(chunkByteSize));
			errorCheck(capacity);
			activeBuffer = make_shared<Buffer>(capacity);
		}

		buffer = activeBuffer;
		targetOffset = activeBuffer->pos;

		activeBuffer->pos += chunkByteSize;
	}	

	memcpy(buffer->data_char + targetOffset, chunkBuffer->data, chunkByteSize);
}

void Writer::launchWriterThread() {

	cout << "launch writer thread \n";

	thread([&]() {

		while (true) {

			shared_ptr<Buffer> buffer = nullptr;

			{
				// cout << "lock 000 \n";
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

				//fsOctree.write(buffer->data_char, numBytes);
				fsPotree.write(buffer->data_char, numBytes);

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

	cout << "lock 100 \n";
	unique_lock<mutex> lock(mtx);
	if (activeBuffer != nullptr) {
		backlog.push_back(activeBuffer);
	}

	closeRequested = true;
	cvClose.wait(lock);

	//fsOctree.close();

}







void doIndexing(string targetPath, State& state, Options& options) {

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

	string targetWorkdir = targetPathToWorkdir(targetPath);

	auto chunks = getChunks(targetWorkdir);
	auto attributes = chunks->attributes;

	Indexer indexer(targetPath);
	indexer.options = options;
	indexer.attributes = attributes;
	indexer.root = make_shared<Node>("r", chunks->min, chunks->max);
	indexer.spacing = (chunks->max - chunks->min).x / 128.0;

	auto onNodeCompleted = [&indexer](Node* node) {

		// printfmt("onNodeCompleted({}) \n", node->name);

		// if(node->isLeaf())
		// { // DEBUG: write as CSV

		// 	int stride = indexer.attributes.bytes;
		// 	int offset_rgb = indexer.attributes.getOffset("rgb");

		// 	if(offset_rgb == 0) exit(123);

		// 	stringstream ss;
		// 	for(int i = 0; i < node->numPoints; i++){
		// 		int X = node->points->get<int32_t>(i * stride + 0);
		// 		int Y = node->points->get<int32_t>(i * stride + 4);
		// 		int Z = node->points->get<int32_t>(i * stride + 8);

		// 		double x = double(X) * indexer.scale;
		// 		double y = double(Y) * indexer.scale;
		// 		double z = double(Z) * indexer.scale;

		// 		auto normalize = [](int value) {return value < 256 ? value : value / 256; };
		// 		int r = normalize(node->points->get<uint16_t>(i * stride + offset_rgb));
		// 		int g = normalize(node->points->get<uint16_t>(i * stride + offset_rgb));
		// 		int b = normalize(node->points->get<uint16_t>(i * stride + offset_rgb));

		// 		ss << format("{:.3f}, {:.3f}, {:.3f}, {}, {}, {} \n", x, y, z, r, g, b);
		// 	}

		// 	string str = ss.str();
		// 	writeFile(format("D:/temp/debug/{}.csv", node->name), str);
		// }

		OctreeSerializer::serialize(node, &indexer.attributes);

		if(node->isLeaf()) return;

		indexer.writer->writeAndUnload(node);
		for(auto child : node->children){
			if(child == nullptr) continue;

			indexer.hierarchyFlusher->write(child.get(), hierarchyStepSize);
		}

		if(node->name == "r"){
			indexer.hierarchyFlusher->write(node, hierarchyStepSize);
		}
	};

	auto onNodeDiscarded = [&indexer](Node* node) {};

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
		// indexer.writer->writeAndUnload(node);
	};

	atomic_int64_t activeThreads = 0;
	mutex mtx_nodes;
	vector<shared_ptr<Node>> nodes;
	int numThreads = numSampleThreads() + 4;
	//numThreads = 1;
	TaskPool<Task> pool(numThreads, [&onNodeCompleted, &onNodeDiscarded, &writeAndUnload, &state, &options, &activeThreads, tStart, &lastReport, &totalPoints, totalBytes, &pointsProcessed, chunks, &indexer, &nodes, &mtx_nodes](auto task) {
		
		auto sampler = make_shared<SamplerWeighted>();
		auto chunk = task->chunk;
		auto chunkRoot = make_shared<Node>(chunk->id, chunk->min, chunk->max);
		auto attributes = chunks->attributes;
		int64_t bpp = attributes.bytes;

		indexer.waitUntilWriterBacklogBelow(1'000);
		activeThreads++;

		auto filesize = fs::file_size(chunk->file);

		// printfmt("start indexing chunk {:>8}. filesize: {:11L}, min: {}, max: {} \n", 
		// 	chunk->id, filesize, chunk->min.toString(), chunk->max.toString()
		// );

		indexer.bytesInMemory += filesize;
		auto pointBuffer = readBinaryFile(chunk->file);

		auto tStartChunking = now();

		if (!options.keepChunks) {
			fs::remove(chunk->file);
		}

		int64_t numPoints = pointBuffer->size / bpp;

		buildHierarchy(&indexer, chunkRoot.get(), pointBuffer, numPoints);

		sampler->sample(chunkRoot.get(), attributes, indexer.spacing, onNodeCompleted, onNodeDiscarded);

		// detach anything below the chunk root. Will be reloaded from
		// temporarily flushed hierarchy during creation of the hierarchy file
		chunkRoot->children.clear();

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

		// printfmt("finished indexing chunk {} \n", chunk->id);

		activeThreads--;
	});


	for (auto chunk : chunks->list) {
		auto task = make_shared<Task>(chunk);
		pool.addTask(task);
	}

	pool.waitTillEmpty();
	pool.close();

	indexer.fChunkRoots.close();

	{ // process chunk roots in batches

		auto sampler = make_shared<SamplerWeighted>();
		
		string tmpChunkRootsPath = targetWorkdir + "/tmpChunkRoots.bin";
		auto tasks = indexer.processChunkRoots();

		for(auto& task : tasks){

			cout << format("task {} \n", task.node->name);

			for(auto& fcr : task.fcrs){
				auto buffer = make_shared<Buffer>(fcr.size);
				readBinaryFile(tmpChunkRootsPath, fcr.offset, fcr.size, buffer->data);
				// printfmt("reloading {} \n", fcr.node->name);

				if(fcr.node->numPoints > 0){
					fcr.node->points = buffer;
				}else if(fcr.node->numVoxels > 0){
					
					int mainVoxelDataSize = fcr.node->numVoxels * sizeof(Voxel);
					fcr.node->voxels = vector<Voxel>(fcr.node->numVoxels);
					memcpy(fcr.node->voxels.data(), buffer->data, mainVoxelDataSize);

					int unfilteredDataSize = fcr.size - mainVoxelDataSize;
					fcr.node->unfilteredVoxelData = make_shared<Buffer>(unfilteredDataSize);
					memcpy(fcr.node->unfilteredVoxelData->data, buffer->data_u8 + mainVoxelDataSize, unfilteredDataSize);
				}

			}

			sampler->sample(task.node, attributes, indexer.spacing, onNodeCompleted, onNodeDiscarded);

			task.node->children.clear();
		}
	}


	// sample up to root node
	if (chunks->list.size() == 1) {
		auto node = nodes[0];

		indexer.root = node;
	} else if (!indexer.root->sampled){
		auto sampler = make_shared<SamplerWeighted>();
		sampler->sample(indexer.root.get(), attributes, indexer.spacing, onNodeCompleted, onNodeDiscarded);
	}

	// root is automatically finished after subsampling all descendants
	//onNodeCompleted(indexer.root.get());

	printElapsedTime("sampling", tStart);

	indexer.writer->closeAndWait();

	printElapsedTime("flushing", tStart);

	indexer.hierarchyFlusher->flush(hierarchyStepSize);

	state.pointBufferOffset = METADATA_CAPACITY_BYTES;
	state.pointBufferSize = indexer.bytesWritten;
	state.hierarchyBufferOffset = indexer.writer->fsPotree.tellp();

	string hierarchyDir = indexer.targetWorkDir + "/.hierarchyChunks";
	HierarchyBuilder builder(hierarchyDir, hierarchyStepSize, &byteOffsets, &unfilteredByteOffsets);
	builder.build(indexer.writer->fsPotree);

	state.hierarchyBufferSize = uint64_t(indexer.writer->fsPotree.tellp()) - state.hierarchyBufferOffset;

	Hierarchy hierarchy = {
		.stepSize = hierarchyStepSize,
		.firstChunkSize = builder.batch_root->byteSize,
	};

	// string metadataPath = targetDir + "/metadata.json";
	string metadata = indexer.createMetadata(options, state, hierarchy);
	// writeFile(metadataPath, metadata);

	// indexer.writer->fsPotree.close();

	// write metadata at beginning of file
	// indexer.writer->fsPotree.open(indexer.writer->outPath, ios::out);
	indexer.writer->fsPotree.seekp(0);
	uint32_t metadataSize = metadata.size();
	indexer.writer->fsPotree.write((const char*)&metadataSize, 4);
	indexer.writer->fsPotree.write(metadata.c_str(), metadata.size());

	indexer.writer->fsPotree.close();

	printElapsedTime("metadata & hierarchy", tStart);

	{
		printfmt("deleting temporary files \n");

		// delete chunk directory
		if (!options.keepChunks) {
			string chunksMetadataPath = indexer.targetWorkDir + "/chunks/metadata.json";

			fs::remove(chunksMetadataPath);
			fs::remove(indexer.targetWorkDir + "/chunks");
		}

		// delete chunk roots data
		string octreePath = indexer.targetWorkDir + "/tmpChunkRoots.bin";
		fs::remove(octreePath);

		fs::remove(indexer.targetWorkDir);
	}

	double duration = now() - tStart;
	state.values["duration(indexing)"] = formatNumber(duration, 3);


}


}




