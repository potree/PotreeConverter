
#include "Writer.h"

#include "structures.h"
#include "indexer.h"

struct MortonCode {
	uint64_t lower;
	uint64_t upper;
	uint64_t whatever;
	uint64_t index;
};

struct SoA {
	unordered_map<string, void*> buffers;
	unordered_map<string, i64> bufferSizes;
	MortonCode* mcs;
};

SoA toStructOfArrays(Node* node, Attributes& attributes, VBuffer* target) {

	auto numPoints = node->numPoints;
	uint8_t* source = node->points->data_u8;

	unordered_map<string, void*> buffers;
	unordered_map<string, i64> bufferSizes;
	
	i64 targetSize = 0;
	
	MortonCode* mcs = (MortonCode*)(target->ptr + targetSize);
	targetSize += numPoints * sizeof(MortonCode);
	target->commit(targetSize);

	for (Attribute attribute : attributes.list) {

		int64_t bytes = attribute.size * numPoints;
		auto attributeOffset = attributes.getOffset(attribute.name);

		if (attribute.name == "rgb") {

			i64 bufferSize = sizeof(u64) * numPoints;
			u64* bufferMC = (u64*)(target->ptr + targetSize);
			targetSize += bufferSize;
			target->commit(targetSize);

			for (int64_t i = 0; i < numPoints; i++) {

				int64_t pointOffset = i * attributes.bytes;


				uint16_t r, g, b;
				memcpy(&r, source + pointOffset + attributeOffset + 0, 2);
				memcpy(&g, source + pointOffset + attributeOffset + 2, 2);
				memcpy(&b, source + pointOffset + attributeOffset + 4, 2);


				auto mc = mortonEncode_magicbits(r, g, b);
				// bufferMC->write(&mc, 8);
				bufferMC[i] = mc;
			}

			buffers["rgb_morton"] = bufferMC;
			bufferSizes["rgb_morton"] = bufferSize;

		} else if (attribute.name == "position"){

			struct P {
				int32_t x, y, z;
			};

			P min;
			min.x = std::numeric_limits<int64_t>::max();
			min.y = std::numeric_limits<int64_t>::max();
			min.z = std::numeric_limits<int64_t>::max();
		
			// Compute minimum
			for (int64_t i = 0; i < numPoints; i++) {

				int64_t pointOffset = i * attributes.bytes;

				int32_t XYZ[3];
				memcpy(XYZ, source + pointOffset + attributeOffset, 12);

				min.x = std::min(min.x, XYZ[0]);
				min.y = std::min(min.y, XYZ[1]);
				min.z = std::min(min.z, XYZ[2]);
			}

			// Now generate buffer of morton codes
			for (int64_t i = 0; i < numPoints; i++) {
				
				int64_t pointOffset = i * attributes.bytes;

				int32_t XYZ[3];
				memcpy(XYZ, source + pointOffset + attributeOffset, 12);

				P p;
				p.x = XYZ[0];
				p.y = XYZ[1];
				p.z = XYZ[2];

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

				MortonCode mc;
				mc.lower = mc_l;
				mc.upper = mc_h;
				mc.whatever = mortonEncode_magicbits(mx, my, mz);
				mc.index = i;

				mcs[i] = mc;
			}

			{
				i64 bufferSize = 16 * numPoints;
				u64* bufferMc = (u64*)(target->ptr + targetSize);
				targetSize += bufferSize;
				target->commit(targetSize);
				
				for (int i = 0; i < numPoints; i++) {
					auto mc = mcs[i];

					bufferMc[2 * i + 0] = mc.upper;
					bufferMc[2 * i + 1] = mc.lower;
				}
				
				buffers["position_morton"] = bufferMc;
				bufferSizes["position_morton"] = bufferSize;
			}


		
		} 

		{

			i64 bufferSize = bytes;
			u8* buffer = (u8*)(target->ptr + targetSize);
			targetSize += bufferSize;
			target->commit(targetSize);

			for (int64_t i = 0; i < numPoints; i++) {

				int64_t pointOffset = i * attributes.bytes;

				memcpy(
					buffer + i * attribute.size,
					source + pointOffset + attributeOffset,
					attribute.size
				);
			}

			buffers[attribute.name] = buffer;
			bufferSizes[attribute.name] = bufferSize;
		}
	}

	SoA soa;
	soa.buffers = buffers;
	soa.mcs = mcs;
	soa.bufferSizes = bufferSizes;

	return soa;
}




//static int64_t totalUncompressed = 0;
//static int64_t totalCompressed = 0;
//static unordered_map<string, int64_t> uncompressedCounters;
//static unordered_map<string, int64_t> compressedCounters;
//static mutex mtx_dbg_compress;

// Pooling allocator for the Brotli encoder.
//
// The one-shot BrotliEncoderCompress creates a fresh BrotliEncoderState for every
// node, so all of its internal buffers (storage, ring buffer, hashers, command/literal
// buffers) are malloc'd and freed per node. The large multi-MB allocations go through
// VirtualAlloc/VirtualFree on Windows, which are slow and serialized across the parallel
// writer threads (this shows up as time spent in GetBrotliStorage).
//
// Instead we drive the streaming API with a thread_local pooling allocator: freed blocks
// are kept in a per-thread free list (bucketed by rounded-up size) and reused across nodes,
// so the system allocator is hit rarely and there is no cross-thread contention.
static size_t brotliPoolBucket(size_t size) {
	// Round up to the next power of two so similar-sized requests share a bucket.
	size_t bucket = 64;
	while (bucket < size) {
		bucket <<= 1;
	}
	return bucket;
}

// One free list per thread, shared by alloc and free. Bucket size -> cached blocks.
static std::unordered_map<size_t, std::vector<void*>>& brotliPoolFreelists() {
	thread_local std::unordered_map<size_t, std::vector<void*>> freelists;
	return freelists;
}

static void* brotliPoolAlloc(void* opaque, size_t size) {
	if (size == 0) return nullptr;

	size_t bucket = brotliPoolBucket(size);

	auto& list = brotliPoolFreelists()[bucket];
	uint8_t* block;
	if (!list.empty()) {
		block = reinterpret_cast<uint8_t*>(list.back());
		list.pop_back();
	} else {
		block = reinterpret_cast<uint8_t*>(malloc(bucket + sizeof(size_t)));
		if (block == nullptr) return nullptr;
	}

	// Store the bucket size in a header so free can return it to the right list.
	*reinterpret_cast<size_t*>(block) = bucket;

	return block + sizeof(size_t);
}

static void brotliPoolFree(void* opaque, void* address) {
	if (address == nullptr) return;

	uint8_t* block = reinterpret_cast<uint8_t*>(address) - sizeof(size_t);
	size_t bucket = *reinterpret_cast<size_t*>(block);

	brotliPoolFreelists()[bucket].push_back(block);
}

void compress(Node* node, Attributes& attributes, VBuffer* encoded, i64* out_encodedSize) {
	
	thread_local VBuffer soa_buffer = VBuffer::create(100'000'000);

	auto numPoints = node->numPoints;
	auto soa = toStructOfArrays(node, attributes, &soa_buffer);

	std::sort(soa.mcs, soa.mcs + numPoints, [](MortonCode& a, MortonCode& b) {

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

		bufferSize += soa.bufferSizes[name];
	}
	
	// Allocating virtual memory with lots of capacity.
	// Note: A single octree node should never need that much capacity.
	// If it is, something is wrong and crashing is expected.
	thread_local VBuffer bufferMerged = VBuffer::create(100'000'000ll);
	bufferMerged.commit(bufferSize);

	i64 targetOffset = 0;
	for (Attribute& attribute : attributes.list) {

		string name = mapName(attribute.name);

		u8* buffer = (u8*)soa.buffers[name];
		i64 bufferSize = soa.bufferSizes[name];

		int64_t bufferAttributeSize = bufferSize / numPoints;

		for (int i = 0; i < numPoints; i++) {
			int sourceIndex = soa.mcs[i].index;

			memcpy(
				bufferMerged.ptr + targetOffset,
				buffer + sourceIndex * bufferAttributeSize,
				bufferAttributeSize
			);
			targetOffset += bufferAttributeSize;
		}
	}

	{

		int quality = 6;
		int lgwin = BROTLI_DEFAULT_WINDOW;
		auto mode = BROTLI_DEFAULT_MODE;
		uint8_t* input_buffer = bufferMerged.ptr;
		size_t input_size = targetOffset;

		size_t encoded_size = input_size * 1.5 + 1'000;
		encoded->commit(encoded_size);

		bool success = false;

		// Use the streaming API (which BrotliEncoderCompress itself wraps) so we can feed
		// it a thread_local pooling allocator that reuses the encoder's internal buffers
		// across nodes instead of malloc/free-ing them every call.
		for (int i = 0; i < 5; i++) {
			BrotliEncoderState* s = BrotliEncoderCreateInstance(brotliPoolAlloc, brotliPoolFree, nullptr);
			BrotliEncoderSetParameter(s, BROTLI_PARAM_QUALITY, (uint32_t)quality);
			BrotliEncoderSetParameter(s, BROTLI_PARAM_LGWIN, (uint32_t)lgwin);
			BrotliEncoderSetParameter(s, BROTLI_PARAM_MODE, (uint32_t)mode);
			BrotliEncoderSetParameter(s, BROTLI_PARAM_SIZE_HINT, (uint32_t)input_size);

			size_t available_in = input_size;
			const uint8_t* next_in = input_buffer;
			size_t available_out = encoded_size;
			uint8_t* next_out = encoded->ptr;
			size_t total_out = 0;

			BROTLI_BOOL result = BrotliEncoderCompressStream(s, BROTLI_OPERATION_FINISH,
				&available_in, &next_in, &available_out, &next_out, &total_out);

			success = (result == BROTLI_TRUE) && BrotliEncoderIsFinished(s);
			BrotliEncoderDestroyInstance(s);

			if (success) {
				encoded_size = total_out;
				break;
			} else {
				encoded_size = (encoded_size + 1024) * 1.5;

				logger::WARN("reserved encoded_buffer size was too small. Trying again with size " + formatNumber(encoded_size) + ".");
			}
		}

		if (!success) {
			stringstream ss;
			ss << "failed to compress node " << node->name << ". aborting conversion." ;
			logger::ERROR(ss.str());

			exit(123);
		}

		*out_encodedSize = encoded_size;
	}

}



Writer::Writer(indexer::Indexer* indexer){
	this->indexer = indexer;
	
	string octreePath = indexer->targetDir + "/octree.bin";
	fsOctree.open(octreePath, ios::out | ios::binary);
	
	ringBuffer = VBuffer::create(capacity);
	ringBuffer.commit(capacity);
	
	launchWriterThread();
}

void Writer::writeAndUnload(Node* node){

	if(node->numPoints == 0) return;

	auto attributes = indexer->attributes;
	string encoding = indexer->options.encoding;

	void* sourceBuffer = nullptr;
	i64 sourceBufferSize = 0;

	if (encoding == "BROTLI") {
		thread_local VBuffer vbuffer = VBuffer::create(100'000'000);
		i64 outSize = 0;
		compress(node, attributes, &vbuffer, &outSize);
		sourceBuffer = vbuffer.ptr;
		sourceBufferSize = outSize;
	} else {
		sourceBuffer = node->points->data;
		sourceBufferSize = node->points->size;
	}
	
	node->byteSize = sourceBufferSize;
	node->byteOffset = write(sourceBuffer, sourceBufferSize);
	node->points = nullptr;
}

i64 Writer::write(void* buffer, i64 size){

	if(size <= 0) return writePos;

	if(size > capacity){
		println("ERROR: Writer::write - write of {} bytes exceeds ring buffer capacity of {} bytes.", size, capacity);
		exit(4320);
	}

	u8* source = (u8*)buffer;

	std::unique_lock<std::mutex> lock(mtx);

	// Wait until enough space is free. Free space is the capacity minus the bytes
	// that have been accepted but not yet written to file. This guarantees we never
	// overwrite a region that is still pending a flush.
	cvSpace.wait(lock, [&]{ return (capacity - (writePos - flushPos)) >= size; });

	// Place the data contiguously, wrapping around to the start of the ring when it
	// would run past the end.
	i64 offset = writePos % capacity;
	i64 firstPart = std::min(size, capacity - offset);
	i64 secondPart = size - firstPart;

	memcpy(ringBuffer.ptr + offset, source, firstPart);
	if(secondPart > 0){
		memcpy(ringBuffer.ptr, source + firstPart, secondPart);
	}
	
	i64 byteOffset = writePos;

	writePos += size;

	lock.unlock();
	cvData.notify_one();
	
	return byteOffset;
}

void Writer::launchWriterThread(){
	// Launch a single thread that writes incoming data to fsOctree.
	writerThread = std::thread([this]{
		
		i64 bytesToFlush = 1'000'000'000;

		while(true){

			i64 offset = 0;
			i64 length = 0;

			{
				std::unique_lock<std::mutex> lock(mtx);

				cvData.wait(lock, [&]{ return (writePos > flushPos) || closeRequested; });

				i64 available = writePos - flushPos;
				if(available == 0 && closeRequested){
					// no pending data and shutdown requested -> done
					break;
				}

				offset = flushPos % capacity;
				// Write at most up to the end of the ring in one go; any wrapped
				// remainder is picked up on the next iteration.
				length = std::min(available, capacity - offset);
			}

			// Disk I/O happens outside the lock so producers can keep filling the
			// ring buffer. The region [flushPos, writePos) is never touched by write()
			// until we advance flushPos below, so reading it here is safe.
			fsOctree.write((char*)(ringBuffer.ptr + offset), length);
			
			// Flush every now and then so that we can better observe the current size of the file. 
			bytesToFlush -= length;
			if(bytesToFlush <= 0){
				fsOctree.flush();
				bytesToFlush = 1'000'000'000;
			}
			
			indexer->bytesWritten += length;
			indexer->bytesToWrite -= length;

			{
				std::lock_guard<std::mutex> lock(mtx);
				flushPos += length;
			}
			cvSpace.notify_all();
		}
	});
}

void Writer::closeAndWait(){
	// Wait until all pending writes have been done.
	{
		std::lock_guard<std::mutex> lock(mtx);
		if(closed) return;
		closeRequested = true;
	}
	cvData.notify_all();

	if(writerThread.joinable()){
		writerThread.join();
	}

	fsOctree.flush();
	fsOctree.close();

	closed = true;
}

i64 Writer::backlogSizeMB(){
	// Bytes accepted via write() but not yet flushed to disk by the writer thread.
	std::lock_guard<std::mutex> lock(mtx);
	i64 backlogBytes = writePos - flushPos;
	return backlogBytes / (1024 * 1024);
}








