#pragma once

#include "Core/CompilerConfig.h"
#include <stdint.h>

namespace SG
{

	/// low res timer
	class Timer
	{
	public:
		Timer();

		uint32_t  get_millisecond(bool reset);
		void      reset();
	private:
		uint32_t  mStartTime = 0;
	};

	// high res timer
	class HiresTimer
	{
	public:
		HiresTimer();

		int64_t get_usecond(bool reset);
		int64_t get_usecond_average();

		float   get_seconds(bool reset);
		float   get_seconds_average();

		void    reset();
	private:
		int64_t mStartTime;

		static const uint32_t LENGTH_OF_HISTORY = 60;
		int64_t               mHistory[LENGTH_OF_HISTORY];
		uint32_t              mHistoryIndex;
	};

}
