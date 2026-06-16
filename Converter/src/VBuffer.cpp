
#include "VBuffer.h"

#ifdef _WIN32
	#define NOMINMAX 
	#include "windows.h"
#elif defined(__linux__)
	#include <sys/mman.h>
	#include <unistd.h>
#endif

VBuffer VBuffer::create(i64 size){

	VBuffer buffer;

#ifdef _WIN32
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	buffer.pageSize = sysinfo.dwPageSize;
#elif defined(__linux__)
	buffer.pageSize = sysconf(_SC_PAGESIZE);
#endif

	// round the reservation up to a multiple of the page size
	i64 virtualCapacity = ((size + buffer.pageSize - 1) / buffer.pageSize) * buffer.pageSize;

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

	buffer.ptr = (u8*)ptr;
	buffer.virtualCapacity = virtualCapacity;
	buffer.comittedCapacity = 0;

	return buffer;
}

// Ensure that at least <size> bytes of physical memory is allocated and mapped.
void VBuffer::commit(i64 size){

	if(size <= comittedCapacity) return;

	if(size > virtualCapacity){
		println("ERROR: VBuffer::commit - requested {} bytes exceeds reserved capacity of {} bytes.", size, virtualCapacity);
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