
#pragma once

#include <algorithm>

#include "unsuck/unsuck.hpp"

// A growable buffer backed by virtual memory.
// A large range of virtual address space is reserved up front (cheap, no physical
// memory is touched), and physical memory is only committed on demand via commit().
// This keeps the data contiguous and pointer-stable while it grows, without paying
// for the full capacity until it is actually used.
struct VBuffer{

	u8* ptr = nullptr;
	i64 pageSize = 0;
	i64 virtualCapacity = 0;
	i64 comittedCapacity = 0;

	// Reserve <size> bytes of virtual memory without committing physical memory yet.
	static VBuffer create(i64 size);

	// Ensure that at least <size> bytes of physical memory is allocated and mapped.
	void commit(i64 size);

	// Release the reserved virtual memory and any committed physical memory.
	void destroy();

};
