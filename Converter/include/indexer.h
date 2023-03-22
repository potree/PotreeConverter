
#pragma once

#include <string>
#include <filesystem>
#include <algorithm>
#include <deque>
#include <unordered_map>
#include <atomic>
#include <algorithm>
#include <functional>
#include <memory>
#include <fstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "json/json.hpp"

#include "Attributes.h"
#include "converter_utils.h"
#include "Vector3.h"
#include "unsuck/unsuck.hpp"
#include "unsuck/TaskPool.hpp"
#include "structures.h"

using json = nlohmann::json;

using std::atomic_int64_t;
using std::atomic_int64_t;
using std::deque;
using std::string;
using std::unordered_map;
using std::function;
using std::shared_ptr;
using std::make_shared;
using std::fstream;
using std::mutex;

namespace fs = std::filesystem;

namespace indexer{

	//constexpr int numSampleThreads = 10;
	//constexpr int numFlushThreads = 36;
	constexpr int maxPointsPerChunk = 10'000;

	inline int numSampleThreads() {
		return getCpuData().numProcessors;
	}

	struct Hierarchy {
		int64_t stepSize = 0;
		vector<uint8_t> buffer;
		int64_t firstChunkSize = 0;
	};

	struct Chunk {
		Vector3 min;
		Vector3 max;

		string file;
		string id;
	};

	struct Chunks {
		vector<shared_ptr<Chunk>> list;
		Vector3 min;
		Vector3 max;
		Attributes attributes;

		Chunks(vector<shared_ptr<Chunk>> list, Vector3 min, Vector3 max) {
			this->list = list;
			this->min = min;
			this->max = max;
		}

	};

	shared_ptr<Chunks> getChunks(string pathIn);

	

	struct Indexer;

	struct Writer {

		Indexer* indexer = nullptr;
		int64_t capacity = 16 * 1024 * 1024;

		// copy node data here first
		shared_ptr<Buffer> activeBuffer = nullptr;

		// backlog of buffers that reached capacity and are ready to be written to disk
		deque<shared_ptr<Buffer>> backlog;

		bool closeRequested = false;
		bool closed = false;
		std::condition_variable cvClose;

		fstream fsOctree;

		//thread tWrite;

		mutex mtx;

		Writer(Indexer* indexer);

		void writeAndUnload(Node* node);

		void launchWriterThread();

		void closeAndWait();

		int64_t backlogSizeMB();

	};

	struct HierarchyFlusher{

		struct HNode{
			string name;
			int64_t byteOffset = 0;
			int64_t byteSize = 0;
			int64_t numPoints = 0;
		};

		mutex mtx;
		string path;
		unordered_map<string, int> chunks;
		vector<HNode> buffer;

		HierarchyFlusher(string path){
			this->path = path;

			this->clear();
		}

		void clear(){
			fs::remove_all(path);

			fs::create_directories(path);
		}

		void write(Node* node, int hierarchyStepSize){
			lock_guard<mutex> lock(mtx);

			HNode hnode = {
				.name       = node->name,
				.byteOffset = node->byteOffset,
				.byteSize   = node->byteSize,
				.numPoints  = node->numPoints,
			};

			buffer.push_back(hnode);

			if(buffer.size() > 10'000){
				this->write(buffer, hierarchyStepSize);
				buffer.clear();
			}
		}

		void flush(int hierarchyStepSize){
			lock_guard<mutex> lock(mtx);
			
			this->write(buffer, hierarchyStepSize);
			buffer.clear();
		}

		void write(vector<HNode> nodes, int hierarchyStepSize){

			unordered_map<string, vector<HNode>> groups;

			for(auto node : nodes){


				string key = node.name.substr(0, hierarchyStepSize + 1);
				if(node.name.size() <= hierarchyStepSize + 1){
					key = "r";
				}

				if(groups.find(key) == groups.end()){
					groups[key] = vector<HNode>();
				}

				groups[key].push_back(node);

				// add batch roots to batches (in addition to root batch)
				if(node.name.size() == hierarchyStepSize + 1){
					groups[node.name].push_back(node);
				}
			}

			fs::create_directories(path);

			// this structure, but guaranteed to be packed
			// struct Record{                 size   offset
			// 	uint8_t name[31];               31        0
			// 	uint32_t numPoints;              4       31
			// 	int64_t byteOffset;              8       35
			// 	int32_t byteSize;                4       43
			// 	uint8_t end = '\n';              1       47
			// };                              ===
			//                                  48

			
			for(auto [key, groupedNodes] : groups){

				Buffer buffer(48 * groupedNodes.size());
				stringstream ss;

				for(int i = 0; i < groupedNodes.size(); i++){
					auto node = groupedNodes[i];

					auto name = node.name.c_str();
					memset(buffer.data_u8 + 48 * i, ' ', 31);
					memcpy(buffer.data_u8 + 48 * i, name, node.name.size());
					buffer.set<uint32_t>(node.numPoints,  48 * i + 31);
					buffer.set<uint64_t>(node.byteOffset, 48 * i + 35);
					buffer.set<uint32_t>(node.byteSize,   48 * i + 43);
					buffer.set<char    >('\n',             48 * i + 47);

					ss << rightPad(name, 10, ' ') 
						<< leftPad(to_string(node.numPoints), 8, ' ')
						<< leftPad(to_string(node.byteOffset), 12, ' ')
						<< leftPad(to_string(node.byteSize), 12, ' ')
						<< endl;
				}

				string filepath = path + "/" + key + ".bin";
				fstream fout(filepath, ios::app | ios::out | ios::binary);
				fout.write(buffer.data_char, buffer.size);
				fout.close();

				if(chunks.find(key) == chunks.end()){
					chunks[key] = 0;
				}

				chunks[key] += groupedNodes.size();
			}

		}

	};

	struct HierarchyChunk {
		string name = "";
		vector<Node*> nodes;
	};

	struct FlushedChunkRoot {
		shared_ptr<Node> node;
		int64_t offset = 0;
		int64_t size = 0;
	};

	struct CRNode{
		string name = "";
		Node* node;
		vector<shared_ptr<CRNode>> children;
		vector<FlushedChunkRoot> fcrs;
		int numPoints = 0;

		CRNode(){
			children.resize(8, nullptr);
		}

		void traverse(function<void(CRNode*)> callback) {
			callback(this);

			for (auto child : children) {

				if (child != nullptr) {
					child->traverse(callback);
				}

			}
		}

		void traversePost(function<void(CRNode*)> callback) {
			for (auto child : children) {

				if (child != nullptr) {
					child->traversePost(callback);
				}
			}

			callback(this);
		}

		bool isLeaf() {
			for (auto child : children) {
				if (child != nullptr) {
					return false;
				}
			}

			return true;
		}
	};

	struct Indexer{

		string targetDir = "";

		Options options;

		Attributes attributes;
		shared_ptr<Node> root;

		shared_ptr<Writer> writer;
		shared_ptr<HierarchyFlusher> hierarchyFlusher;

		vector<shared_ptr<Node>> detachedParts;

		atomic_int64_t byteOffset = 0;

		double scale = 0.001;
		double spacing = 1.0;

		atomic_int64_t dbg = 0;

		mutex mtx_depth;
		int64_t octreeDepth = 0;

		//shared_ptr<TaskPool<FlushTask>> flushPool;
		atomic_int64_t bytesInMemory = 0;
		atomic_int64_t bytesToWrite = 0;
		atomic_int64_t bytesWritten = 0;

		mutex mtx_chunkRoot;
		fstream fChunkRoots;
		vector<FlushedChunkRoot> flushedChunkRoots;

		Indexer(string targetDir) {

			this->targetDir = targetDir;

			// writer = make_shared<Writer>(this);
			hierarchyFlusher = make_shared<HierarchyFlusher>(targetDir + "/.hierarchyChunks");

			string chunkRootFile = targetDir + "/tmpChunkRoots.bin";
			fChunkRoots.open(chunkRootFile, ios::out | ios::binary);
		}

		~Indexer() {
			fChunkRoots.close();
		}

		void waitUntilWriterBacklogBelow(int maxMegabytes);

		void waitUntilMemoryBelow(int maxMegabytes);

		string createMetadata(Options options, State& state, Hierarchy hierarchy);

		string createDebugHierarchy();

		HierarchyChunk gatherChunk(Node* start, int levels);

		vector<HierarchyChunk> createHierarchyChunks(Node* root, int hierarchyStepSize);

		Hierarchy createHierarchy(string path);

		void flushChunkRoot(shared_ptr<Node> chunkRoot);

		void reloadChunkRoots();

		vector<CRNode> processChunkRoots();
	};

	class punct_facet : public std::numpunct<char> {
	protected:
		char do_decimal_point() const { return '.'; };
		char do_thousands_sep() const { return '\''; };
		string do_grouping() const { return "\3"; }
	};

	void doIndexing(string targetDir, State& state, Options& options);


}