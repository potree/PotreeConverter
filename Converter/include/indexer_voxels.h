
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

namespace indexer_voxels{

	//constexpr int numSampleThreads = 10;
	//constexpr int numFlushThreads = 36;
	constexpr int maxPointsPerChunk = 50'000;

	inline int numSampleThreads() {
		return getCpuData().numProcessors;
		// return 1;
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

		void writeAndUnload(Node* node, Attributes attributes);

		void launchWriterThread();

		void closeAndWait();

		int64_t backlogSizeMB();

	};

	struct HierarchyChunk {
		string name = "";
		vector<Node*> nodes;
	};

	struct FlushedChunkRoot {
		shared_ptr<Node> node;
		int64_t offset = 0;
		int64_t sizePoints = 0;
		int64_t sizeVoxels = 0;
	};

	struct Indexer{

		string targetDir = "";

		Options options;

		Attributes attributes;
		shared_ptr<Node> root;

		shared_ptr<Writer> writer;

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