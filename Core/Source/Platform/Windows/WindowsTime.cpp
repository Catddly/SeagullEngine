#include "WindowsTime.h"

#include <time.h>
#include <windows.h>
#include <timeapi.h>

#ifdef __cplusplus
extern "C"
{
#endif

	static int64_t gHighResTimerFrequency = 0;

	void initialize_time()
	{
		LARGE_INTEGER frequency;
		if (QueryPerformanceFrequency(&frequency))
		{
			gHighResTimerFrequency = frequency.QuadPart;
		}
		else
		{
			gHighResTimerFrequency = 1000LL;
		}
	}

	int64_t get_timer_frequency()
	{
		if (gHighResTimerFrequency == 0)
			initialize_time();

		return gHighResTimerFrequency;
	}

	int64_t get_usecond()
	{
		LARGE_INTEGER counter;
		QueryPerformanceCounter(&counter);
		return counter.QuadPart * (int64_t)1e6 / get_timer_frequency();
	}

	uint64_t get_system_time()
	{
		return (uint32_t)timeGetTime();
	}

	uint64_t get_time_since_start()
	{
		return (uint32_t)time(NULL);
	}

#ifdef __cplusplus
}
#endif