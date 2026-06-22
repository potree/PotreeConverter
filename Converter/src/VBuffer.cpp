
#include "VBuffer.h"

#ifdef _WIN32
	#define NOMINMAX 
	#include "windows.h"
#elif defined(__linux__)
	#include <sys/mman.h>
	#include <unistd.h>
#endif

shared_ptr<VBuffer> VBuffer::create(i64 size){

	shared_ptr<VBuffer> buffer = make_shared<VBuffer>();

#ifdef _WIN32
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	buffer->pageSize = sysinfo.dwPageSize;
#elif defined(__linux__)
	buffer->pageSize = sysconf(_SC_PAGESIZE);
#endif
	// println("buffer.pageSize: {:L}", buffer.pageSize);
	// buffer.pageSize = 2'097'152;

	// round the reservation up to a multiple of the page size
	i64 virtualCapacity = ((size + buffer->pageSize - 1) / buffer->pageSize) * buffer->pageSize;

	void* ptr = nullptr;

#ifdef _WIN32
	ptr = VirtualAlloc(nullptr, virtualCapacity, MEM_RESERVE, PAGE_READWRITE);
#elif defined(__linux__)
	ptr = mmap(nullptr, virtualCapacity, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(ptr == MAP_FAILED) ptr = nullptr;
#endif

	if(ptr == nullptr){
		println("ERROR: VBuffer::create - failed to reserve {} bytes of virtual memory.", virtualCapacity);
		exit(4313);
	}

	buffer->ptr = (u8*)ptr;
	buffer->virtualCapacity = virtualCapacity;
	buffer->comittedCapacity = 0;

	return buffer;
}

// Ensure that at least <size> bytes of physical memory is allocated and mapped.
void VBuffer::commit(i64 size){
	
	this->size = size;

	if(size <= comittedCapacity) return;

	if(size > virtualCapacity){
		println("ERROR: VBuffer::commit - requested {} bytes exceeds reserved capacity of {} bytes.", size, virtualCapacity);
		__debugbreak();
		exit(4314);
	}

	// round the requested size up to a multiple of the page size, clamped to capacity
	i64 target = ((size + pageSize - 1) / pageSize) * pageSize;
	target = std::min(target, virtualCapacity);

#ifdef _WIN32
	// MEM_COMMIT is idempotent, so committing from the base is safe even for already-committed pages.
	void* result = VirtualAlloc(ptr, target, MEM_COMMIT, PAGE_READWRITE);
	if(result == nullptr){
		println("ERROR: VBuffer::commit - failed to commit {} bytes of physical memory.", target);
		exit(4315);
	}
#elif defined(__linux__)
	if(mprotect(ptr, target, PROT_READ | PROT_WRITE) != 0){
		println("ERROR: VBuffer::commit - failed to commit {} bytes of physical memory.", target);
		exit(4315);
	}
#endif

	comittedCapacity = target;
}

// Ensure that exactly <size> bytes (rounded up to a page) of physical memory is mapped.
// Grows like commit(), but also releases any physical pages committed beyond the target.
// void VBuffer::commitOrShrink(i64 size){

// 	if(size > virtualCapacity){
// 		println("ERROR: VBuffer::commitOrShrink - requested {} bytes exceeds reserved capacity of {} bytes.", size, virtualCapacity);
// 		__debugbreak();
// 		exit(4314);
// 	}

// 	// round the requested size up to a multiple of the page size, clamped to capacity
// 	i64 target = ((size + pageSize - 1) / pageSize) * pageSize;
// 	target = std::min(target, virtualCapacity);

// 	if(target == comittedCapacity) return;

// 	if(target > comittedCapacity){
// 		// grow: commit the additional physical memory
// 		commit(target);
// 		return;
// 	}

// 	// shrink: release the superfluous physical pages above <target>
// 	i64 freeSize = comittedCapacity - target;

// #ifdef _WIN32
// 	if(VirtualFree(ptr + target, freeSize, MEM_DECOMMIT) == 0){
// 		println("ERROR: VBuffer::commitOrShrink - failed to decommit {} bytes of physical memory.", freeSize);
// 		exit(4316);
// 	}
// #elif defined(__linux__)
// 	// MADV_DONTNEED releases the physical pages back to the OS;
// 	// PROT_NONE keeps the address range in the same reserved state as create().
// 	if(madvise(ptr + target, freeSize, MADV_DONTNEED) != 0 ||
// 		mprotect(ptr + target, freeSize, PROT_NONE) != 0){
// 		println("ERROR: VBuffer::commitOrShrink - failed to decommit {} bytes of physical memory.", freeSize);
// 		exit(4316);
// 	}
// #endif

// 	comittedCapacity = target;
// }

void VBuffer::destroy(){

	if(ptr == nullptr) return;

#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#elif defined(__linux__)
	munmap(ptr, virtualCapacity);
#endif

	ptr = nullptr;
	pageSize = 0;
	virtualCapacity = 0;
	comittedCapacity = 0;
}