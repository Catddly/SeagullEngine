#pragma once

#include "CompilerConfig.h"

#if defined(_MSC_VER) && !defined(NX64)
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <intrin.h>  // intrinsic functions

namespace SG
{

	typedef volatile SG_ALIGNAS(4) uint32_t sg_atomic32_t;
	typedef volatile SG_ALIGNAS(8) uint64_t sg_atomic64_t;
	typedef volatile SG_ALIGNAS(PTR_SIZE) uintptr_t sg_atomicptr_t;

#define sg_memorybarrier_acquire() _ReadWriteBarrier()
#define sg_memorybarrier_release() _ReadWriteBarrier()

#define sg_atomic32_load_relaxed(pVar) (*(pVar))
#define sg_atomic32_store_relaxed(dst, val) _InterlockedExchange( (volatile long*)(dst), val )
#define sg_atomic32_add_relaxed(dst, val) _InterlockedExchangeAdd( (volatile long*)(dst), (val) )
#define sg_atomic32_cas_relaxed(dst, cmp_val, new_val) _InterlockedCompareExchange( (volatile long*)(dst), (new_val), (cmp_val) )

#define sg_atomic64_load_relaxed(pVar) (*(pVar))
#define sg_atomic64_store_relaxed(dst, val) _InterlockedExchange64( (volatile LONG64*)(dst), val )
#define sg_atomic64_add_relaxed(dst, val) _InterlockedExchangeAdd64( (volatile LONG64*)(dst), (val) )
#define sg_atomic64_cas_relaxed(dst, cmp_val, new_val) _InterlockedCompareExchange64( (volatile LONG64*)(dst), (new_val), (cmp_val) )
#else
#define sg_memorybarrier_acquire() __asm__ __volatile__("": : :"memory")
#define sg_memorybarrier_release() __asm__ __volatile__("": : :"memory")

#define sg_atomic32_load_relaxed(pVar) (*(pVar))
#define sg_atomic32_store_relaxed(dst, val) __sync_lock_test_and_set ( (volatile int32_t*)(dst), val )
#define sg_atomic32_add_relaxed(dst, val) __sync_fetch_and_add( (volatile int32_t*)(dst), (val) )
#define sg_atomic32_cas_relaxed(dst, cmp_val, new_val) __sync_val_compare_and_swap( (volatile int32_t*)(dst), (cmp_val), (new_val) )

#define sg_atomic64_load_relaxed(pVar) (*(pVar))
#define sg_atomic64_store_relaxed(dst, val) __sync_lock_test_and_set ((volatile int64_t*) (dst), val )
#define sg_atomic64_add_relaxed(dst, val) __sync_fetch_and_add( (volatile int64_t*)(dst), (val) )
#define sg_atomic64_cas_relaxed(dst, cmp_val, new_val) __sync_val_compare_and_swap( (volatile int64_t*)(dst), (cmp_val), (new_val) )
#endif

	static inline uint32_t sg_atomic32_load_acquire(sg_atomic32_t* pVar)
	{
		uint32_t value = sg_atomic32_load_relaxed(pVar);
		sg_memorybarrier_acquire();
		return value;
	}

	static inline uint32_t sg_atomic32_store_release(sg_atomic32_t* pVar, uint32_t val)
	{
		sg_memorybarrier_release();
		return sg_atomic32_store_relaxed(pVar, val);
	}

	static inline uint32_t sg_atomic32_max_relaxed(sg_atomic32_t* dst, uint32_t val)
	{
		uint32_t prevVal = val;
		do { prevVal = sg_atomic32_cas_relaxed(dst, prevVal, val); } while (prevVal < val);
		return prevVal;
	}

	static inline uint64_t sg_atomic64_load_acquire(sg_atomic64_t* pVar)
	{
		uint64_t value = sg_atomic64_load_relaxed(pVar);
		sg_memorybarrier_acquire();
		return value;
	}

	static inline uint64_t sg_atomic64_store_release(sg_atomic64_t* pVar, uint64_t val)
	{
		sg_memorybarrier_release();
		return sg_atomic64_store_relaxed(pVar, val);
	}

	static inline uint64_t sg_atomic64_max_relaxed(sg_atomic64_t* dst, uint64_t val)
	{
		uint64_t prev_val = val;
		do { prev_val = sg_atomic64_cas_relaxed(dst, prev_val, val); } while (prev_val < val);
		return prev_val;
	}

}

#if PTR_SIZE == 4
#define sg_atomicptr_load_relaxed  sg_atomic32_load_relaxed
#define sg_atomicptr_load_acquire  sg_atomic32_load_acquire
#define sg_atomicptr_store_relaxed sg_atomic32_store_relaxed
#define sg_atomicptr_store_release sg_atomic32_store_release
#define sg_atomicptr_add_relaxed   sg_atomic32_add_relaxed
#define sg_atomicptr_cas_relaxed   sg_atomic32_cas_relaxed
#define sg_atomicptr_max_relaxed   sg_atomic32_max_relaxed
#elif PTR_SIZE == 8
#define sg_atomicptr_load_relaxed  sg_atomic64_load_relaxed
#define sg_atomicptr_load_acquire  sg_atomic64_load_acquire
#define sg_atomicptr_store_relaxed sg_atomic64_store_relaxed
#define sg_atomicptr_store_release sg_atomic64_store_release
#define sg_atomicptr_add_relaxed   sg_atomic64_add_relaxed
#define sg_atomicptr_cas_relaxed   sg_atomic64_cas_relaxed
#define sg_atomicptr_max_relaxed   sg_atomic64_max_relaxed
#endif
