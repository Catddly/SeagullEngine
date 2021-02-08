/////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifdef PROJECT_EASTL_IN_USE
#include "internal/config.h"
#else
#include <include/EASTL/internal/config.h>
#endif

#if EASTL_ALLOCATOR_SG

namespace eastl
{
	///////////////////////////////////////////////////////////////////////////////
	// allocator_sg
	//
	// Implements an EASTL allocator that uses malloc/free as opposed to
	// new/delete or PPMalloc Malloc/Free.
	//
	// Example usage:
	//      vector<int, allocator_sg> intVector;
	//
	///////////////////////////////////////////////////////////////////////////////
	class allocator_sg
	{
	public:
		allocator_sg(const char* = NULL) {}

		allocator_sg(const allocator_sg&) {}

		allocator_sg(const allocator_sg&, const char*) {}

		allocator_sg& operator=(const allocator_sg&) { return *this; }

		bool operator==(const allocator_sg&) { return true; }

		bool operator!=(const allocator_sg&) { return false; }

		void* allocate(size_t n, int /*flags*/ = 0);

		void* allocate(size_t n, size_t alignment, size_t alignmentOffset, int /*flags*/ = 0);

		void deallocate(void* p, size_t /*n*/);

		const char* get_name() const { return "allocator_sg"; }

		void set_name(const char*) {}
	};
	inline bool operator==(const allocator_sg&, const allocator_sg&) { return true; }
	inline bool operator!=(const allocator_sg&, const allocator_sg&) { return false; }

	EASTL_API allocator_sg* GetDefaultAllocatorSG();
	EASTL_API allocator_sg* SetDefaultAllocatorSG(allocator_sg* pAllocator);

} // namespace eastl

#endif
