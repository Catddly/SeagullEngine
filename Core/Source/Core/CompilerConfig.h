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

#define ASSERT(x) if(!(x)) __debugbreak();

#define SG_COMPILE_ASSERT(exp) static_assert((exp), #exp)