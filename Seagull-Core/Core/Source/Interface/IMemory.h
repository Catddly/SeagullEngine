#ifndef IMEMORY_H
#define IMEMORY_H

#include "Core/CompilerConfig.h"
#include <new>

namespace SG
{

	void* sg_malloc_internal(size_t size, const char* file, int line, const char* srcFunc);
	void* sg_memory_align_internal(size_t align, size_t size, const char* file, int line, const char* srcFunc);
	/// calloc will zero the memory
	void* sg_calloc_internal(size_t count, size_t size, const char* file, int line, const char* srcFunc);
	void* sg_calloc_memory_align_internal(size_t count, size_t align, size_t size, const char* file, int line, const char* srcFunc);
	void* sg_realloc_internal(void* ptr, size_t size, const char* file, int line, const char* srcFunc);
	void  sg_free_internal(void* ptr, const char* file, int line, const char* srcFunc);

	/// type* memory = sg_placement_new(ptr)(constructor)
	template <typename T, typename... Args>
	static T* sg_placement_new(void* ptr, Args&&... args)
	{
		return new(ptr) T(eastl::forward<Args>(args)...);
	}

	template <typename T, typename... Args>
	static T* sg_new_internal(const char* file, int line, const char* srcFunc, Args&&... args)
	{
		/// force to use align memory to allocate
		T* ptr = (T*)sg_memory_align_internal(alignof(T), sizeof(T), file, line, srcFunc);
		return sg_placement_new<T>(ptr, eastl::forward<Args>(args)...);
	}

	template <typename T>
	static void sg_delete_internal(T* ptr, const char* file, int line, const char* srcFunc)
	{
		if (ptr)
		{
			ptr->~T();
			sg_free_internal(ptr, file, line, srcFunc);
		}
	}

#ifndef sg_malloc
#define sg_malloc(size) sg_malloc_internal(size, __FILE__, __LINE__, __FUNCTION__)
#endif	
#ifndef sg_memalign
#define sg_memalign(align,size) sg_memory_align_internal(align, size, __FILE__, __LINE__, __FUNCTION__)
#endif	
#ifndef sg_calloc
#define sg_calloc(count,size) sg_calloc_internal(count, size, __FILE__, __LINE__, __FUNCTION__)
#endif	
#ifndef sg_calloc_memalign
#define sg_calloc_memalign(count,align,size) sg_calloc_memory_align_internal(count, align, size, __FILE__, __LINE__, __FUNCTION__)
#endif	
#ifndef sg_realloc
#define sg_realloc(ptr,size) sg_realloc_internal(ptr, size, __FILE__, __LINE__, __FUNCTION__)
#endif	
#ifndef sg_free
#define sg_free(ptr) sg_free_internal(ptr,  __FILE__, __LINE__, __FUNCTION__)
#endif	
#ifndef sg_new
#define sg_new(ObjectType, ...) sg_new_internal<ObjectType>(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#endif	
#ifndef sg_delete
#define sg_delete(ptr) sg_delete_internal(ptr,  __FILE__, __LINE__, __FUNCTION__)
#endif

}

#endif // IMEMORY_H

#ifndef SG_MEMORY_MANAGEMENT
#ifndef malloc
#define malloc(size) static_assert(false, "Please use sg_malloc");
#endif
#ifndef calloc
#define calloc(count, size) static_assert(false, "Please use sg_calloc");
#endif
#ifndef memalign
#define memalign(align, size) static_assert(false, "Please use sg_memalign");
#endif
#ifndef realloc
#define realloc(ptr, size) static_assert(false, "Please use sg_realloc");
#endif
#ifndef free
#define free(ptr) static_assert(false, "Please use sg_free");
#endif
#ifndef new
#define new static_assert(false, "Please use sg_placement_new");
#endif
#ifndef delete
#define delete static_assert(false, "Please use sg_free with explicit destructor call");
#endif
#endif

#ifdef SG_MEMORY_MANAGEMENT
#undef SG_MEMORY_MANAGEMENT
#endif