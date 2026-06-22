
#pragma once

#include <stack>
#include <mutex>
#include <memory>

#include "VBuffer.h"

using std::stack;
using std::mutex;
using std::lock_guard;
using std::shared_ptr;
using std::make_shared;

// A pool that recycles VBuffer objects.
struct VBufferPool{

	static constexpr i64 DEFAULT_CAPACITY = 1'000'000'000;

	inline static stack<shared_ptr<VBuffer>> pool;
	inline static mutex mtx;

	// If there is an element in the pool, remove and return it.
	// Otherwise create a new buffer and return it.
	// The returned buffer has no physical memory committed; the caller is
	// expected to commit() the size it needs before use.
	inline static shared_ptr<VBuffer> acquire(){
		lock_guard<mutex> lock(mtx);

		if(!pool.empty()){
			shared_ptr<VBuffer> buffer = pool.top();
			pool.pop();
			return buffer;
		}
		
		return VBuffer::create(DEFAULT_CAPACITY);
	}

	// Put the buffer back into the pool for later reuse.
	inline static void release(shared_ptr<VBuffer> buffer){

		// nothing to pool for an empty/destroyed buffer
		if(buffer->ptr == nullptr) return;

		lock_guard<mutex> lock(mtx);
		pool.push(buffer);
	}

};
