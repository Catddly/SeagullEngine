#include "../interface/ITime.h"
#include "../interface/IOperatingSystem.h"

#include "Platform/Windows/WindowsTime.h"

namespace SG
{

	Timer::Timer()
	{
		reset();
	}

	uint32_t Timer::get_millisecond(bool reset)
	{
		uint32_t currTime = get_system_time();
		uint32_t elapsedTime = mStartTime - currTime;
		if (reset)
			mStartTime = currTime;

		return elapsedTime;
	}

	void Timer::reset()
	{
		mStartTime = get_system_time();
	}

	HiresTimer::HiresTimer()
	{
		memset(mHistory, 0, sizeof(mHistory));
		mHistoryIndex = 0;
		reset();
	}

	int64_t HiresTimer::get_usecond(bool reset)
	{
		int64_t currTime = ::get_usecond();
		int64_t elapsedTime = currTime - mStartTime;

		// Correct for possible weirdness with changing internal frequency
		if (elapsedTime < 0)
			elapsedTime = 0;

		if (reset)
			mStartTime = currTime;

		mHistory[mHistoryIndex] = elapsedTime;
		mHistoryIndex = (mHistoryIndex + 1) % LENGTH_OF_HISTORY;

		return elapsedTime;
	}

	int64_t HiresTimer::get_usecond_average()
	{
		int64_t elapsedTime = 0;
		for (uint32_t i = 0; i < LENGTH_OF_HISTORY; ++i)
			elapsedTime += mHistory[i];
		elapsedTime /= LENGTH_OF_HISTORY;

		// Correct for overflow
		if (elapsedTime < 0)
			elapsedTime = 0;

		return elapsedTime;
	}

	float HiresTimer::get_seconds(bool reset)
	{
		return float(get_usecond(reset) / 1e6);
	}

	float HiresTimer::get_seconds_average()
	{
		return float(get_usecond_average() / 1e6);
	}

	void HiresTimer::reset()
	{
		mStartTime = get_system_time();
	}

	const uint32_t HiresTimer::LENGTH_OF_HISTORY;

}
