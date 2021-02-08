#include "allocator_seagull.h""

#if EASTL_ALLOCATOR_SG
#include "Interface/IMemory.h"

namespace eastl
{
	using namespace SG;

	void* allocator_sg::allocate(size_t n, int /*flags*/)
	{
		return sg_malloc(n);
	}

	void* allocator_sg::allocate(size_t n, size_t alignment, size_t alignmentOffset, int /*flags*/)
	{
		if ((alignmentOffset % alignment) == 0) // We check for (offset % alignmnent == 0) instead of (offset == 0) because any block which is
												// aligned on e.g. 64 also is aligned at an offset of 64 by definition.
			return sg_memalign(alignment, n);
		return NULL;
	}

	void allocator_sg::deallocate(void* p, size_t /*n*/)
	{
		sg_free(p);
	}

	/// gDefaultAllocator
	/// Default global allocator_sg instance. 
	EASTL_API allocator_sg  gDefaultAllocatorSG;
	EASTL_API allocator_sg* gpDefaultAllocatorSG = &gDefaultAllocatorSG;

	EASTL_API allocator_sg* GetDefaultAllocatorSG()
	{
		return gpDefaultAllocatorSG;
	}

	EASTL_API allocator_sg* SetDefaultAllocatorSG(allocator_sg* pAllocator)
	{
		allocator_sg* const pPrevAllocator = gpDefaultAllocatorSG;
		gpDefaultAllocatorSG = pAllocator;
		return pPrevAllocator;
	}

} // namespace eastl

#endif // EASTL_USER_DEFINED_ALLOCATOR











