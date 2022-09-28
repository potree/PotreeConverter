
#pragma once

#include <string>
#include <filesystem>

#include "structures.h"

namespace fs = std::filesystem;

namespace HB{
	template<class T>
	void sortBreadthFirst(vector<T>& nodes) {
		sort(nodes.begin(), nodes.end(), [](const T& a, const T& b) {
			if (a.name.size() != b.name.size()) {
				return a.name.size() < b.name.size();
			} else {
				return a.name < b.name;
			}
		});
	};
};

using namespace std;

struct HierarchyBuilder{

	// records have this structure, but guaranteed to be packed
	// struct Record{                 size   offset
	// 	uint8_t name[31];               31        0
	// 	uint32_t numPoints;              4       31
	// 	int64_t byteOffset;              8       35
	// 	int32_t byteSize;                4       43
	// 	uint8_t end = '\n';              1       47
	// };                              ===
	//                                  48

	int hierarchyStepSize = 0;
	string path = "";

	enum TYPE {
		NORMAL = 0,
		LEAF   = 1,
		PROXY  = 2,
	};

	struct HNode{
		string name;
		int numPoints;
		uint64_t byteOffset;
		uint64_t byteSize;
		TYPE type = TYPE::LEAF;
		uint64_t proxyByteOffset;
		uint64_t proxyByteSize;
	};

	struct HChunk{
		string name;
		vector<shared_ptr<HNode>> nodes;
		int64_t byteOffset = 0;
	};

	struct HBatch{
		string name;
		string path;
		int numNodes = 0;
		int64_t byteSize = 0;
		vector<shared_ptr<HNode>> nodes;
		vector<shared_ptr<HChunk>> chunks;
		unordered_map<string, shared_ptr<HNode>> nodeMap;
		unordered_map<string, shared_ptr<HChunk>> chunkMap;
	};

	HierarchyBuilder(string path, int hierarchyStepSize){
		this->path = path;
		this->hierarchyStepSize = hierarchyStepSize;
	}

	shared_ptr<HBatch> loadBatch(string path){

		shared_ptr<Buffer> buffer = readBinaryFile(path);

		auto batch = make_shared<HBatch>();
		batch->path = path;
		batch->name = fs::path(path).filename().string();
		batch->numNodes = buffer->size / 48;

		// group this batch in chunks of <hierarchyStepSize>
		for(int i = 0; i < batch->numNodes; i++){

			int recordOffset = 48 * i;

			string nodeName = string(buffer->data_char + recordOffset, 31);
			nodeName = stringReplace(nodeName, " ", "");
			nodeName.erase(std::remove(nodeName.begin(), nodeName.end(), ' '), nodeName.end());

			auto node = make_shared<HNode>();
			node->name       = nodeName;
			node->numPoints  = buffer->get<uint32_t>(recordOffset + 31);
			node->byteOffset = buffer->get< int64_t>(recordOffset + 35);
			node->byteSize   = buffer->get< int32_t>(recordOffset + 43);

			// r: 0, r0123: 1, r01230123: 2
			int chunkLevel = (node->name.size() - 2) / 4;
			string key = node->name.substr(0, hierarchyStepSize * chunkLevel + 1);

			if(batch->chunkMap.find(key) == batch->chunkMap.end()){
				auto chunk = make_shared<HChunk>();
				chunk->name = key;
				batch->chunkMap[key] = chunk;
				batch->chunks.push_back(chunk);
			}

			batch->chunkMap[key]->nodes.push_back(node);
			batch->nodes.push_back(node);
			batch->nodeMap[node->name] = batch->nodes[batch->nodes.size() - 1];
		}

		// breadth-first sorted list of chunks
		sort(batch->chunks.begin(), batch->chunks.end(), [](shared_ptr<HChunk> a, shared_ptr<HChunk> b) {
			if (a->name.size() != b->name.size()) {
				return a->name.size() < b->name.size();
			} else {
				return a->name < b->name;
			}
		});

		// initialize all nodes as leaf nodes, turn into "normal" if child appears
		for(auto node : batch->nodes){

			node->type = TYPE::LEAF;

			string parentName = node->name.substr(0, node->name.size() - 1);
			
			if(batch->nodeMap.find(parentName) != batch->nodeMap.end()){
				batch->nodeMap[parentName]->type = TYPE::NORMAL;
			}
		}

		return batch;
	}

	void processBatch(shared_ptr<HBatch> batch){

		// compute byte offsets of chunks relative to batch
		int64_t byteOffset = 0;
		for(auto chunk : batch->chunks){
			chunk->byteOffset = byteOffset;

			cout << "set offset: " << chunk->name << ", " << chunk->byteOffset << endl;

			if(chunk->name != batch->name){
				// this chunk is not the root of the batch.
				// find parent chunk within batch.
				// there must be a leaf node in the parent chunk,
				// which is the proxy node / pointer to this chunk.
				string parentName = chunk->name.substr(0, chunk->name.size() - hierarchyStepSize);
				if(batch->chunkMap.find(parentName) != batch->chunkMap.end()){
					auto parent = batch->chunkMap[parentName];

					auto proxyNode = batch->nodeMap[chunk->name];

					if(proxyNode == nullptr){
						cout << "ERROR: didn't find proxy node " << chunk->name << endl;
						exit(123);
					}

					proxyNode->type = TYPE::PROXY;
					proxyNode->proxyByteOffset = chunk->byteOffset;
					proxyNode->proxyByteSize = 22 * chunk->nodes.size();

					cout << "make proxy: " << proxyNode->name << ", offset: " << proxyNode->byteOffset << endl;

				}else{
					cout << "ERROR: didn't find chunk " << chunk->name << endl;
					exit(123);
				}
			}

			byteOffset += 48 * chunk->nodes.size();
		}

	}

	shared_ptr<Buffer> serializeBatch(shared_ptr<HBatch> batch){

	}

	void build(){

		fstream fout(path + "/../hierachy.new.bin", ios::binary | ios::out);
		int64_t bytesWritten = 0;

		auto batch_root = loadBatch(path + "/r.txt");

		//vector<shared_ptr<HBatch>> batches;
		for(auto& entry : fs::directory_iterator(path)){
			auto filepath = entry.path();
			// r0626.txt
			// auto filepath = fs::path("D:/dev/pointclouds/Riegl/retz_converted/.hierarchyChunks/r0626.txt");

			// skip root. it get's special treatment
			if(entry.path().filename().string() == "r.txt") continue;

			auto batch = loadBatch(filepath.string());

			processBatch(batch);
			auto buffer = serializeBatch(batch);

			auto proxyNode = batch_root->nodeMap[batch->name];
			proxyNode->type = TYPE::PROXY;
			proxyNode->byteOffset = bytesWritten;
			proxyNode->byteSize = batch->byteSize;

			fout.write(buffer->data_char, buffer->size);
			bytesWritten += buffer->size;

			//batches.push_back(batch);
		}

		return;

		// HB::sortBreadthFirst(batches);
		// std::sort(batches.begin(), batches.end(), [](const HBatch& a, const HBatch& b){
		// 	if (a.name.size() != b.name.size()) {
		// 		return a.name.size() < b.name.size();
		// 	} else {
		// 		return a.name < b.name;
		// 	}
		// });

		// { // zero-fill the first chunk in the file. replaced later

		// 	// this should always exist
		// 	auto rootBatch = batches[0];

		// 	Buffer buffer(22 * rootBatch.numNodes);
		// 	memset(buffer.data, 0, buffer.size);

		// 	fout.write(buffer.data_char, buffer.size);
		// 	bytesWritten += buffer.size;
		// }


		// for(auto batch : batches){

		// 	// skip root for now
		// 	if(batch.name == "r.txt") continue;

		// 	shared_ptr<Buffer> buffer = readBinaryFile(batch.path);

		// 	// group this batch in chunks of <hierarchyStepSize>
		// 	unordered_map<string, HChunk> chunks;
		// 	for(int i = 0; i < batch.numNodes; i++){

		// 		int recordOffset = 48 * i;

		// 		string nodeName = string(buffer->data_char + recordOffset, 31);
		// 		nodeName = stringReplace(nodeName, " ", "");
		// 		nodeName.erase(std::remove(nodeName.begin(), nodeName.end(), ' '), nodeName.end());

		// 		HNode node;
		// 		node.name       = nodeName;
		// 		node.numPoints  = buffer->get<uint32_t>(recordOffset + 31);
		// 		node.byteOffset = buffer->get< int64_t>(recordOffset + 35);
		// 		node.byteSize   = buffer->get< int32_t>(recordOffset + 43);

		// 		// cout << node.name << ": " << node.numPoints << ", " << node.byteOffset << ", " << node.byteSize << endl;

		// 		// r: 0, r0123: 1, r01230123: 2
		// 		int chunkLevel = (node.name.size() - 2) / 4;
		// 		string key = node.name.substr(0, hierarchyStepSize * chunkLevel + 1);

		// 		if(chunks.find(key) == chunks.end()){
		// 			HChunk chunk;
		// 			chunk.name = key;
		// 			chunks[key] = chunk;
		// 		}

		// 		chunks[key].nodes.push_back(node);
		// 	}

		// 	// breadth-first sorted list of chunks
		// 	vector<HChunk*> chunkList;
		// 	for(auto& [chunkName, chunk] : chunks){
		// 		chunkList.push_back(&chunk);
		// 	}
		// 	sort(chunkList.begin(), chunkList.end(), [](const HChunk* a, const HChunk* b) {
		// 		if (a->name.size() != b->name.size()) {
		// 			return a->name.size() < b->name.size();
		// 		} else {
		// 			return a->name < b->name;
		// 		}
		// 	});

		// 	// compute byte offsets of chunks inside file
		// 	int64_t byteOffset = bytesWritten;
		// 	for(HChunk* chunk : chunkList){
		// 		chunk->byteOffset = byteOffset;

		// 		cout << "set offset: " << chunk->name << ", " << chunk->byteOffset << endl;

		// 		if(chunk->name != batch.name){
		// 			// this chunk is not the root of the batch.
		// 			// find parent chunk within batch.
		// 			// there must be a leaf node in the parent chunk,
		// 			// which is the proxy node / pointer to ths chunk.
		// 			string parentName = chunk->name.substr(0, chunk->name.size() - hierarchyStepSize);
		// 			if(chunks.find(parentName) != chunks.end()){
		// 				HChunk& parent = chunks[parentName];

		// 				auto proxyNode = std::find_if(parent.nodes.begin(), parent.nodes.end(), [chunk](const HNode& node){
		// 					return node.name == chunk->name;
		// 				});

		// 				if(proxyNode == parent.nodes.end()){
		// 					cout << "ERROR: didn't find proxy node " << chunk->name << endl;
		// 					exit(123);
		// 				}

		// 				proxyNode->type = TYPE::PROXY;
		// 				proxyNode->byteOffset = chunk->byteOffset;
		// 				proxyNode->byteSize = 22 * chunk->nodes.size();

		// 				cout << "make proxy: " << proxyNode->name << ", offset: " << proxyNode->byteOffset << endl;

		// 			}else{
		// 				cout << "ERROR: didn't find chunk " << chunk->name << endl;
		// 				exit(123);
		// 			}
		// 		}

		// 		byteOffset += 48 * chunk->nodes.size();
		// 	}


		// }

	}

};