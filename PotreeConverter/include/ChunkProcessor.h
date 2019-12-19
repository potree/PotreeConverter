
#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <thread>
#include <mutex>
#include <future>
#include <vector>
#include <iostream>

#include "Metadata.h"
#include "Points.h"
#include "stuff.h"

using std::atomic;
using std::future;
using std::vector;
using std::thread;
using std::mutex;
using std::unique_lock;
using std::lock_guard;
using std::string;
using std::fstream;
using std::cout;
using std::endl;

namespace fs = std::experimental::filesystem;

struct Chunk {

	string file = "";
	Points* points = nullptr;
	int index = -1;
	Vector3<int> index3D;

	Chunk() {

	}
};

class ChunkLoader {
public:
	Metadata metadata;

	vector<Chunk*> chunksToLoad;
	int chunksLoading = 0;
	vector<Chunk*> chunksLoaded;

	mutex mtx_chunksToLoad;
	mutex mtx_chunksLoading;
	mutex mtx_chunksLoaded;

	ChunkLoader(Metadata metadata, vector<Chunk*> chunksToLoad, int numThreads) {
		this->metadata = metadata;
		this->chunksToLoad = chunksToLoad;

		spawnThreads(numThreads);
	}

	future<Chunk*> nextLoadedChunk();

	void loadChunk(Chunk* chunk);

	void spawnThreads(int numThreads);

	bool isAllLoaded() {
		lock_guard<mutex> lock1(mtx_chunksToLoad);
		lock_guard<mutex> lock2(mtx_chunksLoading);

		bool allLoaded = chunksToLoad.size() == 0 && chunksLoading == 0;

		return allLoaded;
	}

	bool isDone() {
		lock_guard<mutex> lock3(mtx_chunksLoaded);

		bool allLoaded = isAllLoaded();
		bool nothingToRetrieve = chunksLoaded.size() == 0;

		bool done = allLoaded && nothingToRetrieve;

		return done;
	}
};




class ChunkIndexer {
public:

	Metadata metadata;
	ChunkLoader* loader = nullptr;

	atomic<int> numActiveThreads = 0;

	vector<Chunk*> chunksIndexed;
	int chunksIndexing = 0;

	mutex mtx_chunksIndexed;
	mutex mtx_chunksIndexing;


	ChunkIndexer(Metadata metadata, ChunkLoader* loader, int numThreads) {
		this->metadata = metadata;
		this->loader = loader;

		spawnThreads(numThreads);
	}

	void spawnThreads(int numThreads);

	void indexChunk(Chunk* chunk);

	bool isDone() {

		lock_guard<mutex> lock1(mtx_chunksIndexed);
		lock_guard<mutex> lock2(mtx_chunksIndexing);

		bool allLoaded = loader->isDone();
		bool nothingToIndex = chunksIndexing == 0;
		bool nothingToRetrieve = chunksIndexed.size() == 0;

		bool done = allLoaded && nothingToIndex && nothingToRetrieve;

		return done;
	}

	bool isAllIndexed() {

		lock_guard<mutex> lock1(mtx_chunksIndexed);
		lock_guard<mutex> lock2(mtx_chunksIndexing);

		bool allLoaded = loader->isDone();
		bool nothingIndexing = chunksIndexing == 0;

		bool done = allLoaded && nothingIndexing;

		return done;
	}

};




class ChunkWriter {
public:

	ChunkIndexer* indexer = nullptr;
	thread* t;

	ChunkWriter(ChunkIndexer* indexer) {
		this->indexer = indexer;

		t = new thread([&]() -> future<void>{
			
			while (true) {

				if (!indexer->isDone()) {
					co_return;
				}

				/*cout << "waitIndexedChunk" << endl;
				Chunk* chunk = co_await indexer->nextIndexedChunk();

				if (chunk == nullptr) {
					break;
				}

				writeChunk(chunk);*/
			}

		});

		t->detach();
	}

	void writeChunk(Chunk* chunk);

	void wait() {
		//t->join();
	}

};




class ChunkProcessor {

	vector<Chunk*> listOfChunksToProcess();

public:

	Metadata metadata;

	int numLoaderThreads = 5;
	int numIndexerThreads = 5;
	//int numWriterThreads = 3;

	ChunkLoader* loader = nullptr;
	ChunkIndexer* indexer = nullptr;
	ChunkWriter* writer = nullptr;

	vector<Chunk*> processedChunks;

	mutex mtx_processedChunks;

	ChunkProcessor(Metadata metadata) {
		this->metadata = metadata;

		initialize();
	}

	void initialize() {

		vector<Chunk*> chunksToLoad = listOfChunksToProcess();

		loader = new ChunkLoader(metadata, chunksToLoad, numLoaderThreads);
		indexer = new ChunkIndexer(metadata, loader, numIndexerThreads);
		//writer = new ChunkWriter(indexer);

		while (!indexer->isAllIndexed()) {

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		cout << "done" << endl;

	}

	void waitFinished() {
		
		//writer->wait();

	}

};