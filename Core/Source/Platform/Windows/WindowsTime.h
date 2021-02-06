#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

	// High res timer functions
	int64_t get_usecond();
	int64_t get_timer_frequency();

	uint64_t get_system_time();
	uint64_t get_time_since_start();

#ifdef __cplusplus
}
#endif