//#include "Interface/IMemory.h"

#include "Core/CompilerConfig.h"

#include <include/mimalloc.h>

//#include <eastl/EABase/eabase.h>
#include <memory.h>

#define ALIGN_TO(size, alignment) (size + alignment - 1) & ~(alignment - 1)
//#define MIN_ALLOC_ALIGNMENT EA_PLATFORM_MIN_MALLOC_ALIGNMENT

#if defined(SG_USE_MEMORY_TRACKING)
	static_assert(0, "Memory tracking unsopported!");
#define _CRT_SECURE_NO_WARNINGS 1
#else

// TODO: memory management on init and exit
bool on_memory_init(const char* appName)
{
	return true;
}

void on_memory_exit()
{

}

void* operator new[](size_t size, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
	return mi_new(size);
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
	return mi_new(size);
}


namespace SG
{

	void* sg_malloc(size_t size)
	{
		void* ptr = mi_malloc(size);
		return ptr;
	}

	void* sg_calloc(size_t count, size_t size)
	{
		void* ptr = mi_calloc(count, size);
		memset(ptr, 0, size * count);
		return ptr;
	}

	void* sg_memalign(size_t align, size_t size)
	{
		align = align > sizeof(void*) ? align : sizeof(void*);
		void* ptr = mi_aligned_alloc(align, size);
		return ptr;
	}

	void* sg_calloc_memalign(size_t count, size_t align, size_t size)
	{
		align = align > sizeof(void*) ? align : sizeof(void*);
		void* ptr = mi_calloc_aligned(count, size, align);
		memset(ptr, 0, count * size);
		return ptr;
	}

	void* sg_realloc(void* ptr, size_t size)
	{
		void* p = mi_realloc(ptr, size);
		return p;
	}

	void sg_free(void* ptr)
	{
		mi_free(ptr);
	}

	void* sg_malloc_internal(size_t size, const char* file, int line, const char* srcFunc) { return sg_malloc(size); }
	void* sg_memory_align_internal(size_t align, size_t size, const char* file, int line, const char* srcFunc) { return sg_memalign(align, size); }
	void* sg_calloc_internal(size_t count, size_t size, const char* file, int line, const char* srcFunc) { return sg_calloc(count, size); }
	void* sg_calloc_memory_align_internal(size_t count, size_t align, size_t size, const char* file, int line, const char* srcFunc) { return sg_calloc_memalign(count, align, size); }
	void* sg_realloc_internal(void* ptr, size_t size, const char* file, int line, const char* srcFunc) { return sg_realloc(ptr, size); }
	void  sg_free_internal(void* ptr, const char* file, int line, const char* srcFunc) { sg_free(ptr); }

}

#endif
