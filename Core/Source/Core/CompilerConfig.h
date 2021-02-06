#pragma once

#include <stdint.h>

#if   INTPTR_MAX == 0x7FFFFFFFFFFFFFFFLL // 64-bit
	#define PTR_SIZE 8
#elif INTPTR_MAX == 0x7FFFFFFF           // 32-bit
	#define PTR_SIZE 4
#else
	#error unsupported platform
#endif

#ifdef __cplusplus
	#define SG_CONSTEXPR constexpr
#else
	#define SG_CONSTEXPR 
#endif

#if defined(_MSC_VER) && !defined(NX64)
	#include <BaseTsd.h>
	typedef SSIZE_T ssize_t;
#endif

//#ifdef SG_DLL_EXPORT
//	#define SG_API __declspec(dllexport)
//#else
//	#define SG_API __declspec(dllimport)
//#endif

//#ifdef SG_API
//#undef SG_API
//#define SG_API
//#endif

#define SG_COMPILE_ASSERT(exp) static_assert((exp), #exp)