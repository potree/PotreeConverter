#pragma once

#include <mutex>
#include <condition_variable>
#include <print>

#include "brotli/encode.h"
#include "brotli/decode.h"

#include "unsuck/unsuck.hpp"
#include "VBuffer.h"
#include "logger.h"

using std::println;
using std::format;

namespace indexer {
	struct Indexer;
}

struct Node;

struct Writer{

	indexer::Indexer* indexer = nullptr;
	i64 capacity = 1024 * 1024 * 1024;
	VBuffer ringBuffer;
	fstream fsOctree;   // File to which we write

	// Ring buffer bookkeeping, expressed as monotonically increasing absolute byte
	// counters. The physical slot for a counter is (counter % capacity).
	// - writePos: total bytes accepted via write()
	// - flushPos: total bytes already written to fsOctree by the writer thread
	// Bytes in [flushPos, writePos) are pending and must not be overwritten.
	i64 writePos = 0;
	i64 flushPos = 0;

	bool closeRequested = false;
	bool closed = false;

	std::mutex mtx;
	std::condition_variable cvData;   // notified when new data is available to flush
	std::condition_variable cvSpace;  // notified when flushed data frees up space
	std::thread writerThread;

	Writer(indexer::Indexer* indexer);

	void writeAndUnload(Node* node);
	
	i64 write(void* buffer, i64 size);

	void launchWriterThread();

	void closeAndWait();
	
	i64 backlogSizeMB();

};
