
#include <cerrno>
#include <execution>
#include <algorithm>
#include <print>
#include <format>

#include "indexer.h"

#include "Attributes.h"
#include "logger.h"
#include "PotreeConverter.h"
#include "DbgWriter.h"
#include "brotli/encode.h"
#include "brotli/decode.h"
#include "HierarchyBuilder.h"
#include "VBuffer.h"
#include "VBufferPool.h"

using std::unique_lock;
using std::println;
using std::format;

namespace indexer{

	constexpr int hierarchyStepSize = 4;

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
			strID = stringReplace(strID, ".br", "");

			return strID;
		};

		vector<shared_ptr<Chunk>> chunksToLoad;
		for (const auto& entry : fs::directory_iterator(chunkDirectory)) {
			string filename = entry.path().filename().string();
			string chunkID = toID(filename);


			if (iEndsWith(filename, ".bin") || iEndsWith(filename, ".br")) {
				// acceptable
			}else{
				// not a chunk format
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
			
			// .\PotreeConverter.exe -i "G:\swisssurface3d" --encoding BROTLI --attributes rgb intensity --compress-chunks --no-chunking --keep-chunks --chunkdir "H:\swisssurface3d_chunks" --method random -o "E:\swisssurface3d_chunks_converted"
			// if(chunksToLoad.size() >= 30'000) break; // works
			// if(chunksToLoad.size() >= 60'000) break; // works; 355 billion points
			// if(chunksToLoad.size() >= 70'000) break; // works; 425 billion points
			// if(chunksToLoad.size() >= 80'000) break; // does not work. 488 billion points
			// if(chunksToLoad.size() >= 100'000) break; // does not work; 630 billion points
		}

		auto chunks = make_shared<Chunks>(chunksToLoad, min, max);
		chunks->attributes = attributes;

		return chunks;
	}

	void Indexer::flushChunkRoot(shared_ptr<Node> chunkRoot) {

		lock_guard<mutex> lock(mtx_chunkRoot);

		static int64_t offset = 0;
		int64_t size = chunkRoot->points->size;

		fChunkRoots.write((const char*)chunkRoot->points->ptr, size);

		FlushedChunkRoot fcr;
		fcr.node = chunkRoot;
		fcr.offset = offset;
		fcr.size = size;

		chunkRoot->points = nullptr;

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
			shared_ptr<CRNode> node = nodesMap[fcr.node->name];
			
			node->fcrs.push_back(fcr);
			node->numPoints += fcr.node->numPoints;
		}

		// recursively merge leaves if sum(points) < threshold
		auto cr_root = nodesMap["r"];
		static int64_t threshold = 5'000'000;

		cr_root->traversePost([](CRNode* node){
			
			if(node->isLeaf()){

			}else{

				i64 numPoints = 0;
				for(auto child : node->children){
					if(!child) continue;

					numPoints += child->numPoints;
				}
				node->numPoints = numPoints;

				if(node->numPoints < threshold){
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
			// cout << node->name << ", #points: " << node->numPoints << ", #fcrs: " << node->fcrs.size() << endl;

			if(node->fcrs.size() > 0){
				CRNode crnode = *node;
				tasks.push_back(crnode);
			}
		});

		return tasks;
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

			// auto buffer = make_shared<Buffer>(size);
			auto buffer = VBufferPool::acquire();
			buffer->commit(size);
			readBinaryFile(octreePath, start, size, buffer->ptr);

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
		return format("{:f}", value);

	};

	auto s = [](string str) {
		return format("\"{}\"", str);
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
		ss.imbue(std::locale::classic());
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
		ss.imbue(std::locale::classic());
		
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
	ss.imbue(std::locale::classic());

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

	// type + childMask + numPoints + offset + size
	constexpr int bytesPerNode = 1 + 1 + 4 + 8 + 8;

	auto chunkSize = [](HierarchyChunk& chunk) {
		return chunk.nodes.size() * bytesPerNode;
	};

	auto chunks = createHierarchyChunks(root.get(), hierarchyStepSize);

	// string dbgChunksPath = path + "/../dbg_chunks";
	// fs::create_directories(dbgChunksPath);
	// for(auto& chunk : chunks){

	// 	stringstream ss;

	// 	for(auto node : chunk.nodes){
	// 		ss << node->name << endl;
	// 	}


	// 	writeFile(dbgChunksPath + "/" + chunk.name + ".txt", ss.str());
	// }

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

struct Pyramid{
	i64 maxLevel; // starting from zero. maxLevel 2  ->  0: 1x1x1, 1: 2x2x2; 2: 4x4x4
	vector<i64*> counters;
	vector<i64*> prefixSum;
};

void computeSumPyramid(Pyramid* pyramid){

	// Compute counters in lower LODs
	for (int level = pyramid->maxLevel - 1; level >= 0; level--) {
		
		i64 currentGridSize = pow(2, level);

		for (int x = 0; x < currentGridSize; x++) {
			for (int y = 0; y < currentGridSize; y++) {
				for (int z = 0; z < currentGridSize; z++) {

					auto index = mortonEncode_magicbits(z, y, x);
					auto index_p1 = mortonEncode_magicbits(2 * z, 2 * y, 2 * x);

					int64_t sum = 0;
					for (int i = 0; i < 8; i++) {
						sum += pyramid->counters[level + 1][index_p1 + i];
					}
					
					pyramid->counters[level][index] = sum;
				}
			}
		}
	}
	
	// Compute prefix sum
	for(int level = 0; level <= pyramid->maxLevel; level++){
		
		i64 gridsize = pow(2, level);
		i64 numCells = gridsize * gridsize * gridsize;
		
		i64* counters = pyramid->counters[level];
		i64* prefixSum = pyramid->prefixSum[level];
		prefixSum[0] = 0;
		
		for (i64 i = 1; i < numCells; i++) {
			prefixSum[i] = prefixSum[i - 1] + counters[i - 1];
		}
	}

}

vector<NodeCandidate> createNodes(Pyramid* pyramid) {

	vector<NodeCandidate> nodes;

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
		
		auto index = mortonEncode_magicbits(z, y, x);
		i64 numPoints = pyramid->counters[level][index];

		if (level == pyramid->maxLevel) {
			// don't split further at this time. May be split further in another pass

			if (numPoints > 0) {
				nodes.push_back(candidate);
			}
		} else if (numPoints > maxPointsPerChunk) {
			// split (too many points in node)

			for (int i = 0; i < 8; i++) {

				auto index_p1 = mortonEncode_magicbits(2 * z, 2 * y, 2 * x) + i;
				auto count = pyramid->counters[level + 1][index_p1];

				if (count > 0) {
					NodeCandidate child;
					child.level = level + 1;
					child.name = candidate.name + to_string(i);
					child.indexStart = pyramid->prefixSum[level + 1][index_p1];
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

inline i64 gridIndexOf(
	i64 pointIndex, 
	shared_ptr<Buffer> &points, 
	i64 bpp, 
	Vector3 scale, 
	Vector3 offset, 
	Vector3 min, 
	Vector3 size, 
	i64 counterGridSize
){

	i64 pointOffset = pointIndex * bpp;
	int32_t* xyz = reinterpret_cast<int32_t*>(points->data_u8 + pointOffset);

	double x = (xyz[0] * scale.x) + offset.x;
	double y = (xyz[1] * scale.y) + offset.y;
	double z = (xyz[2] * scale.z) + offset.z;

	i64 ix = double(counterGridSize) * (x - min.x) / size.x;
	i64 iy = double(counterGridSize) * (y - min.y) / size.y;
	i64 iz = double(counterGridSize) * (z - min.z) / size.z;

	ix = std::max(i64(0), std::min(ix, counterGridSize - 1));
	iy = std::max(i64(0), std::min(iy, counterGridSize - 1));
	iz = std::max(i64(0), std::min(iz, counterGridSize - 1));

	i64 index = mortonEncode_magicbits(iz, iy, ix);

	return index;
}

Node* expandTo(Node* node, NodeCandidate& candidate) {

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

// 1. Counter grid
// 2. Hierarchy from counter grid
// 3. identify nodes that need further refinment
// 4. Recursively repeat at 1. for identified nodes
void buildHierarchy(Indexer* indexer, Node* node, shared_ptr<Buffer> points, int64_t numPoints, int64_t depth = 0) {

	if (numPoints < maxPointsPerChunk) {
		Node* realization = node;
		realization->indexStart = 0;
		realization->numPoints = numPoints;
		realization->points = VBufferPool::acquire();
		realization->points->commit(points->size);
		memcpy(realization->points->ptr, points->data, points->size);

		return;
	}


	auto tStart = now();

	constexpr i64 levels = 5; // = gridSize 32
	constexpr i64 counterGridSize = 32; // pow(2, levels);
	constexpr i64 counterGridNumElements = counterGridSize * counterGridSize * counterGridSize; 
	
	thread_local Pyramid* pyramid = nullptr;
	
	// init counter pyramid data
	if(!pyramid){
		pyramid = new Pyramid();
		pyramid->maxLevel = levels;
		pyramid->counters.resize(pyramid->maxLevel + 1);
		pyramid->prefixSum.resize(pyramid->maxLevel + 1);
		for(int level = 0; level <= pyramid->maxLevel; level++){
			i64 gridSize = pow(2, level);
			i64 numCells = gridSize * gridSize * gridSize;
			pyramid->counters[level] = (i64*)malloc(sizeof(i64) * numCells);
			pyramid->prefixSum[level] = (i64*)malloc(sizeof(i64) * numCells);
		}
	}

	Vector3 min = node->min;
	Vector3 max = node->max;
	Vector3 size = max - min;
	Attributes attributes = indexer->attributes;
	i64 bpp = attributes.bytes;
	Vector3 scale = attributes.posScale;
	Vector3 offset = attributes.posOffset;

	// COUNTING
	memset(pyramid->counters[pyramid->maxLevel], 0, counterGridNumElements * sizeof(i64));
	for (int64_t i = 0; i < numPoints; i++) {
		auto index = gridIndexOf(i, points, bpp, scale, offset, min, size, counterGridSize);
		pyramid->counters[pyramid->maxLevel][index]++;
	}
	
	// Update counters in lower levels of pyramid, and compute prefix sum
	computeSumPyramid(pyramid);

	{ // DISTRIBUTING

		// Buffer tmp(numPoints * bpp);
		thread_local shared_ptr<VBuffer> tmp = VBuffer::create(2'000'000'000);
		tmp->commit(numPoints * bpp);

		thread_local i64 offsets[counterGridNumElements];
		memcpy(offsets, pyramid->prefixSum[pyramid->maxLevel], sizeof(offsets));

		for (i64 i = 0; i < numPoints; i++) {
			i64 index = gridIndexOf(i, points, bpp, scale, offset, min, size, counterGridSize);
			i64 targetIndex = offsets[index]++;

			if (targetIndex * bpp >= tmp->comittedCapacity) {
				__debugbreak();
			}

			memcpy(tmp->ptr + targetIndex * bpp, points->data_u8 + i * bpp, bpp);
		}

		memcpy(points->data, tmp->ptr, numPoints * bpp);
	}

	vector<NodeCandidate> nodes = createNodes(pyramid);
	vector<Node*> needRefinement;

	// Turn candidates into actual nodes
	int64_t octreeDepth = 0;
	for (NodeCandidate& candidate : nodes) {

		Node* realization = expandTo(node, candidate);
		realization->indexStart = candidate.indexStart;
		realization->numPoints = candidate.numPoints;
		int64_t bytes = candidate.numPoints * bpp;

		// auto buffer = make_shared<Buffer>(bytes);
		shared_ptr<VBuffer> buffer = VBufferPool::acquire();
		buffer->commit(bytes);
		memcpy(buffer->ptr,
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
		// auto buffer = subject->points;
		shared_ptr<Buffer> buffer = make_shared<Buffer>(subject->points->size);
		memcpy(buffer->data, subject->points->ptr, subject->points->size);
		
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

				stringstream msg;
				msg << "Too many duplicate points were encountered. #points: " << subject->numPoints;
				msg << ", #unique points: " << distinct.size() << endl;
				msg << "Duplicates inside node will be dropped! ";
				msg << "min: " << subject->min.toString() << ", max: " << subject->max.toString();

				logger::WARN(msg.str());

				// shared_ptr<Buffer> distinctBuffer = make_shared<Buffer>(distinct.size() * bpp);
				shared_ptr<VBuffer> distinctBuffer = VBufferPool::acquire();
				distinctBuffer->commit(distinct.size() * bpp);

				for(int64_t i = 0; i < distinct.size(); i++){
					// distinctBuffer->write(buffer->ptr + distinct[i] * bpp, bpp);
					memcpy(
						distinctBuffer->ptr + i * bpp,
						buffer->data_u8 + distinct[i] * bpp,
						bpp
					);
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

void serialize_stage_chunkroots(
	Indexer& indexer, 
	shared_ptr<indexer::Chunks> chunks,
	vector<shared_ptr<Node>>& nodes,
	State& state, 
	i64 totalPoints, 
	i64 totalBytes, 
	i64 pointsProcessed
){
	// Persist the state of the chunk-roots stage to <targetDir>/stage_chunkroots.
	// At this point, everything below the chunk roots has already been sampled and
	// permanently written out (point data via indexer.writer to octree.bin, hierarchy
	// records via indexer.hierarchyFlusher to .hierarchyChunks). The only thing that is
	// still "in flight" is the leftover, unsampled data of each chunk root itself, which
	// sits in tmpChunkRoots.bin at the [offset, offset + size) ranges recorded in
	// indexer.flushedChunkRoots.
	//
	// A node's bounding box is fully determined by its name and the root bounding box
	// (see childBoundingBoxOf/addDescendant), so we don't need to serialize the whole
	// node tree - just the root bounding box plus, for every chunk root, its name,
	// byte range in tmpChunkRoots.bin, and point count. That's enough to reconstruct
	// both indexer.root (via addDescendant) and indexer.flushedChunkRoots.
	//
	// load_stage_chunkroots restores this state at the beginning of doMerging.

	// make sure everything that was already handed to the writer/hierarchy flusher
	// is actually durable on disk before we call this stage "checkpointed".
	// closing the writer also ensures that octree.bin's file size matches the
	// writer's final writePos, which the resumed writer continues from.
	indexer.writer->closeAndWait();
	indexer.hierarchyFlusher->flush(hierarchyStepSize);

	string stageDir = indexer.targetDir + "/stage_chunkroots";
	fs::create_directories(stageDir);

	auto vec3ToJson = [](Vector3 value){
		return json::array({value.x, value.y, value.z});
	};

	json js;
	js["version"] = 1;
	js["totalPoints"] = totalPoints;
	js["totalBytes"] = totalBytes;
	js["pointsProcessed"] = pointsProcessed;

	js["indexer"]["root"]["name"] = indexer.root->name;
	js["indexer"]["root"]["min"] = vec3ToJson(indexer.root->min);
	js["indexer"]["root"]["max"] = vec3ToJson(indexer.root->max);
	js["indexer"]["spacing"] = indexer.spacing;
	js["indexer"]["octreeDepth"] = indexer.octreeDepth;

	{ // indexer.attributes (== chunks->attributes)
		// same field names as chunks/metadata.json, so loading can reuse the parsing in getChunks
		json jsAttributes = json::array();
		for(auto& attribute : indexer.attributes.list){
			json jsAttribute;
			jsAttribute["name"] = attribute.name;
			jsAttribute["description"] = attribute.description;
			jsAttribute["size"] = attribute.size;
			jsAttribute["numElements"] = attribute.numElements;
			jsAttribute["elementSize"] = attribute.elementSize;
			jsAttribute["type"] = getAttributeTypename(attribute.type);
			jsAttribute["min"] = vec3ToJson(attribute.min);       // nlohmann dumps Infinity as null,
			jsAttribute["max"] = vec3ToJson(attribute.max);       // which is what getChunks expects
			jsAttribute["scale"] = vec3ToJson(attribute.scale);
			jsAttribute["offset"] = vec3ToJson(attribute.offset);
			jsAttribute["histogram"] = attribute.histogram;

			jsAttributes.push_back(jsAttribute);
		}
		js["indexer"]["attributes"] = jsAttributes;
		js["indexer"]["attributes_posScale"] = vec3ToJson(indexer.attributes.posScale);
		js["indexer"]["attributes_posOffset"] = vec3ToJson(indexer.attributes.posOffset);
	}

	js["state"]["name"] = state.name;
	js["state"]["pointsTotal"] = i64(state.pointsTotal);
	js["state"]["pointsProcessed"] = i64(state.pointsProcessed);
	js["state"]["bytesProcessed"] = i64(state.bytesProcessed);
	js["state"]["duration"] = state.duration;
	js["state"]["numPasses"] = state.numPasses;
	js["state"]["currentPass"] = state.currentPass;
	js["state"]["values"] = state.values;

	// chunk list; min/max of each chunk are derived from id + root bounding box on load,
	// attributes are stored in js["indexer"]["attributes"]
	js["chunks"]["min"] = vec3ToJson(chunks->min);
	js["chunks"]["max"] = vec3ToJson(chunks->max);
	json jsChunkList = json::array();
	for(auto& chunk : chunks->list){
		json jsChunk;
		jsChunk["id"] = chunk->id;
		jsChunk["file"] = chunk->file;

		jsChunkList.push_back(jsChunk);
	}
	js["chunks"]["list"] = jsChunkList;

	json jsChunkRoots = json::array();
	for(auto& fcr : indexer.flushedChunkRoots){
		json jsChunkRoot;
		jsChunkRoot["name"] = fcr.node->name;
		jsChunkRoot["offset"] = fcr.offset;
		jsChunkRoot["size"] = fcr.size;
		jsChunkRoot["numPoints"] = fcr.node->numPoints;
		jsChunkRoot["byteOffset"] = fcr.node->byteOffset;
		jsChunkRoot["byteSize"] = fcr.node->byteSize;
		jsChunkRoot["sampled"] = fcr.node->sampled;

		jsChunkRoots.push_back(jsChunkRoot);
	}
	js["chunkRoots"] = jsChunkRoots;

	// the <nodes> vector holds the same chunk-root Node objects that flushedChunkRoots
	// references, so their data is already stored in js["chunkRoots"] - only the names
	// are needed to rebuild the vector (order preserved)
	json jsNodes = json::array();
	for(auto& node : nodes){
		jsNodes.push_back(node->name);
	}
	js["nodes"] = jsNodes;


	string statePath = stageDir + "/state.json";
	writeFile(statePath, js.dump(2));

	logger::INFO(format("serialized chunk-roots stage to '{}' ({} chunk roots)", statePath, indexer.flushedChunkRoots.size()));
}

void load_stage_chunkroots(
	string targetDir,
	Options& options,
	Indexer* indexer,
	indexer::Chunks* chunks,
	vector<shared_ptr<Node>>* nodes,
	State* state,
	i64* totalPoints,
	i64* totalBytes,
	i64* pointsProcessed
){
	// Load the state that was written by serialize_stage_chunkroots and reconstruct
	// indexer, chunks, nodes and state as they were at the end of the chunk-roots
	// stage of doIndexing.

	string statePath = targetDir + "/stage_chunkroots/state.json";

	if(!fs::exists(statePath)){
		println("ERROR: could not find serialized chunk-roots stage at '{}'", statePath);
		exit(45123);
	}

	json js = json::parse(readTextFile(statePath));

	// serialize_stage_chunkroots dumps non-finite doubles (e.g. Infinity in attribute
	// min/max) as null, so restore nulls to the given fallback
	auto jsToVec3 = [](json js, double fallback) -> Vector3 {
		auto d = [fallback](json value) -> double {
			return value.is_null() ? fallback : double(value);
		};

		return { d(js[0]), d(js[1]), d(js[2]) };
	};

	*totalPoints = js["totalPoints"];
	*totalBytes = js["totalBytes"];
	*pointsProcessed = js["pointsProcessed"];

	state->name = js["state"]["name"];
	state->pointsTotal = i64(js["state"]["pointsTotal"]);
	state->pointsProcessed = i64(js["state"]["pointsProcessed"]);
	state->bytesProcessed = i64(js["state"]["bytesProcessed"]);
	state->duration = js["state"]["duration"];
	state->numPasses = js["state"]["numPasses"];
	state->currentPass = js["state"]["currentPass"];
	state->values = js["state"]["values"].get<std::map<string, string>>();

	vector<Attribute> attributeList;
	for(auto jsAttribute : js["indexer"]["attributes"]){
		string name = jsAttribute["name"];
		int size = jsAttribute["size"];
		int numElements = jsAttribute["numElements"];
		int elementSize = jsAttribute["elementSize"];
		AttributeType type = typenameToType(jsAttribute["type"]);

		Attribute attribute(name, size, numElements, elementSize, type);
		attribute.description = jsAttribute["description"];
		attribute.min = jsToVec3(jsAttribute["min"], Infinity);
		attribute.max = jsToVec3(jsAttribute["max"], -Infinity);
		attribute.scale = jsToVec3(jsAttribute["scale"], 1.0);
		attribute.offset = jsToVec3(jsAttribute["offset"], 0.0);
		attribute.histogram = jsAttribute["histogram"].get<vector<int64_t>>();

		attributeList.push_back(attribute);
	}
	Attributes attributes(attributeList);
	attributes.posScale = jsToVec3(js["indexer"]["attributes_posScale"], 1.0);
	attributes.posOffset = jsToVec3(js["indexer"]["attributes_posOffset"], 0.0);

	indexer->targetDir = targetDir;
	indexer->options = options;
	indexer->attributes = attributes;
	indexer->spacing = js["indexer"]["spacing"];
	indexer->octreeDepth = js["indexer"]["octreeDepth"];

	// resume writing where the chunk-roots stage left off, instead of starting
	// over - octree.bin is appended to, .hierarchyChunks is kept
	indexer->writer = make_shared<Writer>(indexer, true);
	indexer->hierarchyFlusher = make_shared<HierarchyFlusher>(targetDir + "/.hierarchyChunks", false);
	// note: fChunkRoots stays unopened. The merging stage only reads tmpChunkRoots.bin,
	// and opening the stream for writing would truncate it.

	Vector3 rootMin = jsToVec3(js["indexer"]["root"]["min"], 0.0);
	Vector3 rootMax = jsToVec3(js["indexer"]["root"]["max"], 0.0);
	indexer->root = make_shared<Node>(js["indexer"]["root"]["name"], rootMin, rootMax);

	// bounding boxes are not serialized; they are fully determined by
	// the node/chunk name and the root bounding box
	auto boundsOf = [rootMin, rootMax](string name) -> BoundingBox {
		BoundingBox box = {rootMin, rootMax};

		for(int i = 1; i < name.size(); i++){
			int index = name[i] - '0';
			box = childBoundingBoxOf(box.min, box.max, index);
		}

		return box;
	};

	// restore flushed chunk roots and insert them into the node tree.
	// FlushedChunkRoot::node and the tree node must be the same object,
	// just like in doIndexing.
	unordered_map<string, shared_ptr<Node>> chunkRootsByName;
	for(auto& jsChunkRoot : js["chunkRoots"]){
		string name = jsChunkRoot["name"];
		auto box = boundsOf(name);

		auto node = make_shared<Node>(name, box.min, box.max);
		node->numPoints = jsChunkRoot["numPoints"];
		node->byteOffset = jsChunkRoot["byteOffset"];
		node->byteSize = jsChunkRoot["byteSize"];
		node->sampled = jsChunkRoot["sampled"];

		FlushedChunkRoot fcr;
		fcr.node = node;
		fcr.offset = jsChunkRoot["offset"];
		fcr.size = jsChunkRoot["size"];

		indexer->flushedChunkRoots.push_back(fcr);

		// add chunk root, provided it isn't the root - same as in doIndexing
		if(name.size() > 1){
			indexer->root->addDescendant(node);
		}

		chunkRootsByName[name] = node;
	}

	for(string name : js["nodes"]){
		nodes->push_back(chunkRootsByName[name]);
	}

	chunks->min = jsToVec3(js["chunks"]["min"], 0.0);
	chunks->max = jsToVec3(js["chunks"]["max"], 0.0);
	chunks->attributes = attributes;
	for(auto& jsChunk : js["chunks"]["list"]){
		auto chunk = make_shared<Chunk>();
		chunk->id = jsChunk["id"];
		chunk->file = jsChunk["file"];

		auto box = boundsOf(chunk->id);
		chunk->min = box.min;
		chunk->max = box.max;

		chunks->list.push_back(chunk);
	}

	logger::INFO(format("loaded chunk-roots stage from '{}' ({} chunk roots)", statePath, indexer->flushedChunkRoots.size()));
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

	string chunkdir = targetDir;
	if(options.chunkdir != ""){
		chunkdir = options.chunkdir;
	}
	shared_ptr<indexer::Chunks> chunks = getChunks(chunkdir);
	Attributes attributes = chunks->attributes;

	Indexer indexer(targetDir);
	indexer.options = options;
	indexer.attributes = attributes;
	indexer.root = make_shared<Node>("r", chunks->min, chunks->max);
	indexer.spacing = (chunks->max - chunks->min).x / 128.0;

	auto onNodeCompleted = [&indexer](Node* node) {
		indexer.writer->writeAndUnload(node);
		indexer.hierarchyFlusher->write(node, hierarchyStepSize);
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
		indexer.writer->writeAndUnload(node);
	};

	atomic_int64_t activeThreads = 0;
	mutex mtx_nodes;
	vector<shared_ptr<Node>> nodes;
	// int numThreads = numSampleThreads() + 4;
	int numThreads = numSampleThreads() / 3 + 2;
	// numThreads = 1;
	TaskPool<Task> pool(numThreads, [&onNodeCompleted, &onNodeDiscarded, &writeAndUnload, &state, &options, &activeThreads, tStart, &lastReport, &totalPoints, totalBytes, &pointsProcessed, chunks, &indexer, &nodes, &mtx_nodes, &sampler](auto task) {
		
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

		shared_ptr<Buffer> pointBuffer = nullptr;
		
		if(iEndsWith(chunk->file, "bin")){
			pointBuffer = readBinaryFile(chunk->file);
		}else if(iEndsWith(chunk->file, "br")){
			shared_ptr<Buffer> compressed = readBinaryFile(chunk->file);

			// first, let's figure out the size of the total uncompressed buffer, which we stored with each compressed batch
			uint64_t uncompressedSize = 0;
			uint64_t offset = 0;
			while(offset < compressed->size){
				uint64_t uncompressedBatchSize = compressed->get<uint64_t>(offset + 0);
				uint64_t compressedBatchSize = compressed->get<uint64_t>(offset + 8);

				// println("uncompressedBatchSize: {}, compressedBatchSize: {}", uncompressedBatchSize, compressedBatchSize);

				offset = offset + 16 + compressedBatchSize;
				uncompressedSize = uncompressedSize + uncompressedBatchSize;
			}
			// println("uncompressedSize: {}", uncompressedSize);

			// allocate sufficient memory for all decompressed chunks
			size_t decoded_size = uncompressedSize;
			pointBuffer = make_shared<Buffer>(decoded_size);

			// now decompress chunks
			uint64_t offset_in = 0;
			uint64_t offset_out = 0;
			while(offset_in < compressed->size){
				uint64_t uncompressedBatchSize = compressed->get<uint64_t>(offset_in + 0);
				uint64_t compressedBatchSize = compressed->get<uint64_t>(offset_in + 8);

				size_t encoded_size = compressedBatchSize;
				const uint8_t* encoded_buffer = compressed->data_u8 + offset_in + 16;
				size_t actualDecodedSize = uncompressedBatchSize; // brotli uses this var as input, and overwrites it with the actual decoded size afterwards
				uint8_t* decoded_buffer = pointBuffer->data_u8 + offset_out;

				auto result = BrotliDecoderDecompress(encoded_size, encoded_buffer, &actualDecodedSize, decoded_buffer);

				if(result != BROTLI_DECODER_RESULT_SUCCESS){
					println("Failed to decode brotli-compressed chunk.");
					exit(54256);
				}else if(uncompressedBatchSize != actualDecodedSize){
					println("Mismatch in recorded (and expected) uncompressed size vs. actual uncompressed size. {} != {}", 
						uncompressedBatchSize, actualDecodedSize);
					exit(7345);
				}

				offset_in += 16 + compressedBatchSize;
				offset_out += uncompressedBatchSize;
			}



			// BrotliDecoderState* state = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
			// BrotliDecoderResult result = BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT;

			// size_t encoded_size = brotliBuffer->size;
			// const uint8_t* encoded_buffer = (const uint8_t*)brotliBuffer->data;
			// size_t decoded_size = decoded_buffer->size;
			// uint8_t* decoded_buffer = (uint8_t*)decodedBuffer->data;
			// BrotliDecoderDecompress(encoded_size, encoded_buffer, decoded_size, decoded_buffer);



		}else{
			println("ERROR: Tried loading chunk with unhandled extension: {}", chunk->file);
			exit(5234);
		}

		auto tStartChunking = now();

		if (!options.keepChunks) {
			fs::remove(chunk->file);
		}
		
		// { // DEBUG: Convert brotli compressed chunks to csv files
			
		// 	// Write the point cloud in <pointBuffer> into a .csv file with attributes: x, y, z, intensity
		// 	auto scale = attributes.posScale;
		// 	auto offset = attributes.posOffset;
		// 	int offsetIntensity = attributes.getOffset("intensity");
		// 	int64_t numPointsInChunk = pointBuffer->size / bpp;

		// 	fs::path csvPath = fs::path("E:/temp") / (chunk->id + ".csv");
		// 	fs::create_directories(csvPath.parent_path());
			
		// 	println("Writing chunk {}", csvPath.string());

		// 	std::ofstream csv(csvPath.string());
		// 	csv << "x, y, z, intensity\n";

		// 	for (int64_t i = 0; i < numPointsInChunk; i++) {
		// 		int64_t pointOffset = i * bpp;

		// 		int32_t* xyz = reinterpret_cast<int32_t*>(pointBuffer->data_u8 + pointOffset);
		// 		double x = (xyz[0] * scale.x) + offset.x;
		// 		double y = (xyz[1] * scale.y) + offset.y;
		// 		double z = (xyz[2] * scale.z) + offset.z;

		// 		uint16_t* intensity = reinterpret_cast<uint16_t*>(pointBuffer->data_u8 + pointOffset + offsetIntensity);

		// 		csv << format("{}, {}, {}, {}\n", x, y, z, intensity[0]);
		// 	}

		// 	csv.close();
			
		// 	println("Writing chunk {} finished", csvPath.string());
		// }

		int64_t numPoints = pointBuffer->size / bpp;

		buildHierarchy(&indexer, chunkRoot.get(), pointBuffer, numPoints);

		sampler.sample(chunkRoot.get(), attributes, indexer.spacing, onNodeCompleted, onNodeDiscarded);

		// detach anything below the chunk root. Will be reloaded from
		// temporarily flushed hierarchy during creation of the hierarchy file
		chunkRoot->children.clear();

		indexer.flushChunkRoot(chunkRoot);

		// add chunk root, provided it isn't the root.
		if (chunkRoot->name.size() > 1) {
			indexer.root->addDescendant(chunkRoot);
		}

		lock_guard<mutex> lock(mtx_nodes);
		
		static i64 totalNumPoints = 0;
		totalNumPoints += numPoints;
		println("processed points: {:L}", totalNumPoints);

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

	logger::INFO("All tasks submitted, waiting for finish");
	pool.waitTillEmpty();
	logger::INFO("Closing Task Pool");
	pool.close();
	
	logger::INFO("Closing fChunkRoots stream");
	indexer.fChunkRoots.close();
	
	serialize_stage_chunkroots(indexer, chunks, nodes, state, totalPoints, totalBytes, pointsProcessed);

}

void doMerging(string targetDir, State& state, Options& options, Sampler& sampler) {
	
	cout << endl;
	cout << "=======================================" << endl;
	cout << "=== MERGING                            " << endl;
	cout << "=======================================" << endl;
	
	auto tStart = now();
	
	Indexer indexer;
	i64 totalPoints = 0;
	i64 totalBytes = 0;
	i64 pointsProcessed = 0;
	
	indexer::Chunks chunks;
	vector<shared_ptr<Node>> nodes;

	load_stage_chunkroots(targetDir, options, &indexer, &chunks, &nodes, &state, &totalPoints, &totalBytes, &pointsProcessed);

	Attributes attributes = chunks.attributes;
	
	state.name = "MERGING";

	auto onNodeCompleted = [&indexer](Node* node) {
		indexer.writer->writeAndUnload(node);
		indexer.hierarchyFlusher->write(node, hierarchyStepSize);
	};

	auto onNodeDiscarded = [&indexer](Node* node) {};
	
	
	
	{ // process chunk roots in batches
	
		logger::INFO("Start processing chunk roots");
		
		string tmpChunkRootsPath = targetDir + "/tmpChunkRoots.bin";
		auto tasks = indexer.processChunkRoots();

		for(auto& task : tasks){

			for(auto& fcr : task.fcrs){
				
				logger::INFO(format("Processing FlushedChunkRoot '{}'", fcr.node->name));
				
				shared_ptr<VBuffer> buffer = VBufferPool::acquire();
				buffer->commit(fcr.size);
				readBinaryFile(tmpChunkRootsPath, fcr.offset, fcr.size, buffer->ptr);

				fcr.node->points = buffer;
			}

			logger::INFO(format("sampling node '{}'", task.node->name));
			sampler.sample(task.node, attributes, indexer.spacing, onNodeCompleted, onNodeDiscarded);

			task.node->children.clear();
		}
	}

	// sample up to root node
	logger::INFO("sampling to root node");
	if (chunks.list.size() == 1) {
		auto node = nodes[0];

		indexer.root = node;
	} else if (!indexer.root->sampled){
		sampler.enableTrace = true;
		sampler.sample(indexer.root.get(), attributes, indexer.spacing, onNodeCompleted, onNodeDiscarded);
	}

	// root is automatically finished after subsampling all descendants
	onNodeCompleted(indexer.root.get());
	logger::INFO("Finished root node");

	printElapsedTime("sampling", tStart);

	indexer.writer->closeAndWait();

	printElapsedTime("flushing", tStart);


	//string hierarchyPath = targetDir + "/hierarchy.bin";
	//Hierarchy hierarchy = indexer.createHierarchy(hierarchyPath);
	//writeBinaryFile(hierarchyPath, hierarchy.buffer);

	indexer.hierarchyFlusher->flush(hierarchyStepSize);

	string hierarchyDir = indexer.targetDir + "/.hierarchyChunks";
	HierarchyBuilder builder(hierarchyDir, hierarchyStepSize);
	builder.build();

	Hierarchy hierarchy = {
		.stepSize = hierarchyStepSize,
		.firstChunkSize = builder.batch_root->byteSize,
	};

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
		//fs::remove(octreePath);
	}

	double duration = now() - tStart;
	state.values["duration(indexing)"] = formatNumber(duration, 3);
}


}




