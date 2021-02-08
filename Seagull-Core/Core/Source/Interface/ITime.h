#pragma once

#include <include/EASTL/string.h>

namespace SG
{

	class Timer
	{
	public:
		Timer(const eastl::string_view& name);

		float GetTotalTime() const; // in seconds
		float GetDeltaTime() const; // in seconds
		double GetSecondsPerCount() const;

		void Reset(); // Call before message loop.
		void Start(); // Call when unpaused.
		void Stop();  // Call when paused.
		void Tick();  // Call every frame.
	private:
		eastl::string_view mName;

		double mDeltaTime;
		double mSecondsPerCount;

		__int64 mBaseTime;       // timer start time
		__int64 mPausedTime;     // total time we paused
		__int64 mStopTime;       // last time when we stopped
		__int64 mPrevTime;       // last frame duration
		__int64 mCurrTime;       // curr frame duration

		bool mIsStopped;
	};

}
