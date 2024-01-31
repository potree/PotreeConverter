
#pragma once

#include <string>
#include <filesystem>
#include <bitset>

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

constexpr int HIERARCHY_NODE_BYTESIZE = 38;

struct HierarchyBuilder{

	// this structure, but guaranteed to be packed
	// struct Record{                 size   offset
	// 	uint8_t name[31];               31        0
	// 	uint32_t numPoints;              4       31
	// 	int64_t byteOffset;              8      35
	// 	int32_t byteSizePoints;          4       43
	// 	int32_t byteSizePosition;        4       47
	// 	int32_t byteSizeFiltered;        4       51
	// 	int32_t byteSizeUnfiltered;      4       55
	// 	uint64_t nodeIndex               8       59
	// 	uint8_t end = '\n';              1       67
	// };                              ===
	//                                  68 

	int hierarchyStepSize = 0;
	string path = "";
	vector<uint64_t>* byteOffsets = nullptr;
	vector<uint64_t>* unfilteredByteOffsets = nullptr;

	enum TYPE {
		NORMAL = 0,
		LEAF   = 1,
		PROXY  = 2,
	};

	struct HNode{
		string   name                 = "";
		int      numSamples           = 0;
		uint8_t  childMask            = 0;
		uint64_t byteOffset           = 0;
		uint64_t unfilteredByteOffset = 0;
		uint32_t byteSizePoints       = 0;
		uint32_t byteSizePosition     = 0;
		uint32_t byteSizeFiltered     = 0;
		uint32_t byteSizeUnfiltered   = 0;
		uint64_t nodeIndex            = 0;
		TYPE     type                 = TYPE::LEAF;
		uint64_t proxyByteOffset      = 0;
		uint64_t proxyByteSize        = 0;
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

	shared_ptr<HBatch> batch_root;

	HierarchyBuilder(
		string path, 
		int hierarchyStepSize, 
		vector<uint64_t>* byteOffsets, 
		vector<uint64_t>* unfilteredByteOffsets
	){
		this->path = path;
		this->hierarchyStepSize = hierarchyStepSize;
		this->byteOffsets = byteOffsets;
		this->unfilteredByteOffsets = unfilteredByteOffsets;
	}

	shared_ptr<HBatch> loadBatch(string path){

		shared_ptr<Buffer> buffer = readBinaryFile(path);

		auto batch = make_shared<HBatch>();
		batch->path = path;
		batch->name = fs::path(path).stem().string();
		batch->numNodes = buffer->size / 68;

		// group this batch in chunks of <hierarchyStepSize>
		for(int i = 0; i < batch->numNodes; i++){

			int recordOffset = 68 * i;

			string nodeName = string(buffer->data_char + recordOffset, 31);
			nodeName = stringReplace(nodeName, " ", "");
			nodeName.erase(std::remove(nodeName.begin(), nodeName.end(), ' '), nodeName.end());

			auto node = make_shared<HNode>();
			node->name                  = nodeName;
			node->numSamples            = buffer->get<uint32_t>(recordOffset + 31);
			node->byteSizePoints        = buffer->get< int32_t>(recordOffset + 43);
			node->byteSizePosition      = buffer->get< int32_t>(recordOffset + 47);
			node->byteSizeFiltered      = buffer->get< int32_t>(recordOffset + 51);
			node->byteSizeUnfiltered    = buffer->get< int32_t>(recordOffset + 55);
			node->nodeIndex             = buffer->get<uint64_t>(recordOffset + 59);
			node->byteOffset            = byteOffsets->at(node->nodeIndex);
			node->unfilteredByteOffset  = unfilteredByteOffsets->at(node->nodeIndex);

			// DEBUG: switching from node->byteOffset to byteOffsets array
			// check if these match for now
			// assert(byteOffsets->at(node->nodeIndex) == node->byteOffset);

			// r: 0, r0123: 1, r01230123: 2
			int chunkLevel = (node->name.size() - 2) / hierarchyStepSize;
			string key = node->name.substr(0, hierarchyStepSize * chunkLevel + 1);
			if(node->name == batch->name){
				key = node->name;
			}

			if(batch->chunkMap.find(key) == batch->chunkMap.end()){
				auto chunk = make_shared<HChunk>();
				chunk->name = key;
				batch->chunkMap[key] = chunk;
				batch->chunks.push_back(chunk);
			}

			batch->chunkMap[key]->nodes.push_back(node);
			batch->nodes.push_back(node);
			batch->nodeMap[node->name] = batch->nodes[batch->nodes.size() - 1];

			bool isChunkKey = ((node->name.size() - 1) % hierarchyStepSize) == 0;
			bool isBatchSubChunk = node->name.size() > hierarchyStepSize + 1;
			if(isChunkKey && isBatchSubChunk){
				if(batch->chunkMap.find(node->name) == batch->chunkMap.end()){
					auto chunk = make_shared<HChunk>();
					chunk->name = node->name;
					batch->chunkMap[node->name] = chunk;
					batch->chunks.push_back(chunk);
				}

				batch->chunkMap[node->name]->nodes.push_back(node);
			}
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
		// also notify parent that it has a child!
		for(auto node : batch->nodes){

			if(node->childMask == 0){
				node->type = TYPE::LEAF;
			}else{
				node->type = TYPE::NORMAL;
			}

			string parentName = node->name.substr(0, node->name.size() - 1);

			auto ptrParent = batch->nodeMap.find(parentName);

			if(ptrParent != batch->nodeMap.end()){

				int childIndex = node->name.back() - '0';
				ptrParent->second->type = TYPE::NORMAL;
				ptrParent->second->childMask = ptrParent->second->childMask | (1 << childIndex);
			}
			
		}

		// find and flag proxy nodes (pseudo-leaf in one chunk pointing to root of a child-chunk)
		for(auto chunk : batch->chunks){

			//if(chunk->name == batch->name) continue;
			
			auto ptr = batch->nodeMap.find(chunk->name);

			if(ptr != batch->nodeMap.end()){
				ptr->second->type = TYPE::PROXY;
			}else{
				// could not find a node with the chunk's name
				// should only happen if this chunk's root  
				// is equal to the batch root
				if(chunk->name != batch->name){
					cout << "ERROR: could not find chunk " << chunk->name << " in batch " << batch->name << endl;
					exit(123);
				}
			}
		}

		// sort nodes in chunks in breadth-first order
		for(auto chunk : batch->chunks){
			sort(chunk->nodes.begin(), chunk->nodes.end(), [](shared_ptr<HNode> a, shared_ptr<HNode> b) {
				if (a->name.size() != b->name.size()) {
					return a->name.size() < b->name.size();
				} else {
					return a->name < b->name;
				}
			});
		}

		return batch;
	}

	void processBatch(shared_ptr<HBatch> batch){

		// compute byte offsets of chunks relative to batch
		int64_t byteOffset = 0;
		for(auto chunk : batch->chunks){
			chunk->byteOffset = byteOffset;

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
					proxyNode->proxyByteSize = HIERARCHY_NODE_BYTESIZE * chunk->nodes.size();
				}else{
					cout << "ERROR: didn't find chunk " << chunk->name << endl;
					exit(123);
				}
			}

			byteOffset += HIERARCHY_NODE_BYTESIZE * chunk->nodes.size();
		}

		batch->byteSize = byteOffset;

	}

	shared_ptr<Buffer> serializeBatch(shared_ptr<HBatch> batch, int64_t bytesWritten){
		
		int numRecords = 0;
		for(auto chunk : batch->chunks){
			numRecords += chunk->nodes.size();  // all nodes in chunk except chunk root
		}

		auto buffer = make_shared<Buffer>(HIERARCHY_NODE_BYTESIZE * numRecords);

		int recordsProcessed = 0;
		for(auto chunk : batch->chunks){

			//auto chunkRoot = batch->nodes[chunk->name];
			// TODO...;

			int i = 0; 
			for(auto node : chunk->nodes){

				// proxy nodes exist twice - in the chunk and the parent-chunk that points to this chunk
				// only the node in the parent-chunk is a proxy (to its non-proxy counterpart)
				bool isProxyNode = (node->type == TYPE::PROXY) && node->name != chunk->name;
				
				TYPE type = node->type;
				if(node->type == TYPE::PROXY && !isProxyNode){
					type = TYPE::NORMAL;
				}

				// dumb hack, like everything in this file.
				if(chunk->nodes.size() == 1){
					type = TYPE::LEAF;
				}

				uint64_t byteOffset = (isProxyNode ? bytesWritten + node->proxyByteOffset : node->byteOffset);
				uint64_t unfilteredByteOffset = node->unfilteredByteOffset;
				uint64_t byteSize = 0;

				if(isProxyNode){
					byteSize = node->proxyByteSize;
				}else{
					byteSize = std::max(
						node->byteSizePoints, 
						node->byteSizePosition + node->byteSizeFiltered);
				}

				if(node->name == "r402233"){
					int a = 10;
				}

				buffer->set<uint8_t >(type                     , HIERARCHY_NODE_BYTESIZE * recordsProcessed +  0);
				buffer->set<uint8_t >(node->childMask          , HIERARCHY_NODE_BYTESIZE * recordsProcessed +  1);
				buffer->set<uint32_t>(node->numSamples         , HIERARCHY_NODE_BYTESIZE * recordsProcessed +  2);
				buffer->set<uint64_t>(byteOffset               , HIERARCHY_NODE_BYTESIZE * recordsProcessed +  6);
				buffer->set<uint64_t>(unfilteredByteOffset     , HIERARCHY_NODE_BYTESIZE * recordsProcessed + 14);
				buffer->set<uint32_t>(byteSize                 , HIERARCHY_NODE_BYTESIZE * recordsProcessed + 22);
				buffer->set<uint32_t>(node->byteSizePosition   , HIERARCHY_NODE_BYTESIZE * recordsProcessed + 26);
				buffer->set<uint32_t>(node->byteSizeFiltered   , HIERARCHY_NODE_BYTESIZE * recordsProcessed + 30);
				buffer->set<uint32_t>(node->byteSizeUnfiltered , HIERARCHY_NODE_BYTESIZE * recordsProcessed + 34);

				recordsProcessed++;
				i++;
			}
		}

		batch->byteSize = buffer->size;

		return buffer;
	}

	void build(fstream& fout_potree){

		uint64_t initialFilePos = fout_potree.tellp();

		// string hierarchyFilePath = path + "/../hierarchy.bin";
		// fstream fout(hierarchyFilePath, ios::binary | ios::out);
		int64_t bytesWritten = 0;

		auto batch_root = loadBatch(path + "/r.bin");
		this->batch_root = batch_root;

		{ // reserve the first <x> bytes in the file for the root chunk
			Buffer tmp(HIERARCHY_NODE_BYTESIZE * batch_root->nodes.size());
			memset(tmp.data, 0, tmp.size);
			// fout.write(tmp.data_char, tmp.size);
			fout_potree.write(tmp.data_char, tmp.size);
			bytesWritten = tmp.size;
		}

		// now write all hierarchy batches, except root
		// update proxy nodes in root with byteOffsets of written batches.
		for(auto& entry : fs::directory_iterator(path)){
			auto filepath = entry.path();
			// r0626.txt

			// skip root. it get's special treatment
			if(filepath.filename().string() == "r.bin") continue;
			// skip non *.bin files
			if(!iEndsWith(filepath.string(), ".bin")) continue;

			auto batch = loadBatch(filepath.string());

			processBatch(batch);
			auto buffer = serializeBatch(batch, bytesWritten);

			
			if(batch->nodes.size() > 1){

				auto proxyNode = batch_root->nodeMap[batch->name];

				proxyNode->type = TYPE::PROXY;
				proxyNode->proxyByteOffset = bytesWritten;
				proxyNode->proxyByteSize = HIERARCHY_NODE_BYTESIZE * batch->chunkMap[batch->name]->nodes.size();
				
			}else{
				// if there is only one node in that batch,
				// then we flag that node as leaf in the root-batch
				auto root_batch_node = batch_root->nodeMap[batch->name];
				root_batch_node->type = TYPE::LEAF;
			}

			// fout.write(buffer->data_char, buffer->size);
			fout_potree.write(buffer->data_char, buffer->size);
			bytesWritten += buffer->size;
		}

		// close/flush file so that we can reopen it to modify beginning
		// fout.close();

		{ // update beginning of file with root chunk
			// fstream f(hierarchyFilePath, ios::ate | ios::binary | ios::out | ios::in);
			// f.seekg(0);

			auto buffer = serializeBatch(batch_root, 0);

			// f.write(buffer->data_char, buffer->size);
			// f.close();

			fout_potree.seekp(initialFilePos);
			fout_potree.write(buffer->data_char, buffer->size);
		}

		// redundant security check
		if(iEndsWith(this->path, ".hierarchyChunks")){
			fs::remove_all(this->path);
		}

		return;
	}

};
