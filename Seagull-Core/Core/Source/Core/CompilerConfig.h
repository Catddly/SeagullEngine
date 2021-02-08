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

#ifdef __cplusplus
	#ifdef SG_PLATFORM_WINDOWS
		#define SG_ALIGN(def, a) __declspec(align(a)) def
	#endif
#endif

#if defined(_MSC_VER) && !defined(NX64)
	#include <BaseTsd.h>
	typedef SSIZE_T ssize_t;
#endif

#if defined(SG_PLATFORM_WINDOWS)
	#define SG_CALLCONV __cdecl
#else
	#define SG_CALLCONV
#endif

#define ASSERT(x) if(!(x)) __debugbreak();

#define SG_COMPILE_ASSERT(exp) static_assert((exp), #exp)