
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

		mutex mtx;
		string path;

		HierarchyFlusher(string path){
			this->path = path;

			this->clear();
		}

		void clear(){
			fs::remove_all(path);

			fs::create_directories(path);
		}

		void write(vector<Node*> nodes, int hierarchyStepSize){

			unordered_map<string, vector<Node*>> groups;

			for(auto node : nodes){
				string key = node->name.substr(0, hierarchyStepSize + 1);
				if(node->name.size() <= hierarchyStepSize + 1){
					key = "r";
				}

				if(groups.find(key) == groups.end()){
					groups[key] = vector<Node*>();
				}

				groups[key].push_back(node);
			}

			lock_guard<mutex> lock(mtx);

			fs::create_directories(path);

			for(auto [key, groupedNodes] : groups){

				stringstream ss;

				for(auto node : groupedNodes){
					ss << node->name << " " 
					<< node->numPoints << " " 
					<< node->byteOffset << " " 
					<< node->byteSize << endl;
				}

				string filepath = path + "/" + key + ".txt";
				fstream fout(filepath, ios::app | ios::out);
				fout << ss.str();
				fout.close();
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

			writer = make_shared<Writer>(this);
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
	};

	class punct_facet : public std::numpunct<char> {
	protected:
		char do_decimal_point() const { return '.'; };
		char do_thousands_sep() const { return '\''; };
		string do_grouping() const { return "\3"; }
	};

	void doIndexing(string targetDir, State& state, Options& options, Sampler& sampler);


}