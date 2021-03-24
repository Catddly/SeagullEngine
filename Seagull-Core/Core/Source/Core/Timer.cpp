#include "Interface/ITime.h"
#include "Interface/IOperatingSystem.h"

#include <timeapi.h>

namespace SG
{

	Timer::Timer(const eastl::string_view& name)
		:mName(name), mDeltaTime(-1.0), mSecondsPerCount(0.0),
		mBaseTime(0), mPausedTime(0), mStopTime(0), mPrevTime(0), mCurrTime(0), mIsStopped(false)
	{
		uint64_t countsPerSeconds;
		QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSeconds);
		mSecondsPerCount = 1.0 / (double)countsPerSeconds;
	}

	float Timer::GetTotalTime() const
	{
		// If we are stopped, do not count the time that has passed since we stopped.
		// Moreover, if we previously already had a pause, the distance 
		// mStopTime - mBaseTime includes paused time, which we do not want to count.
		// To correct this, we can subtract the paused time from mStopTime:  
		//
		//                     |<--paused time-->|
		// ----*---------------*-----------------*------------*------------*------> time
		//  mBaseTime       mStopTime        startTime     mStopTime    mCurrTime

		if (mIsStopped)
		{
			return (float)(((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
		}

		// The distance mCurrTime - mBaseTime includes paused time,
		// which we do not want to count.  To correct this, we can subtract 
		// the paused time from mCurrTime:  
		//
		//  (mCurrTime - mPausedTime) - mBaseTime 
		//
		//                     |<--paused time-->|
		// ----*---------------*-----------------*------------*------> time
		//  mBaseTime       mStopTime        startTime     mCurrTime

		else
		{
			return (float)(((mCurrTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
		}
	}

	float Timer::GetDeltaTime() const
	{
		return (float)mDeltaTime;
	}

	double Timer::GetSecondsPerCount() const
	{
		return mSecondsPerCount;
	}

	void Timer::Reset()
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		mCurrTime = currTime;
		mBaseTime = currTime;
		mPrevTime = currTime;
		mStopTime = 0;
		mIsStopped = false;
	}

	void Timer::Start()
	{
		// Accumulate the time elapsed between stop and start pairs.
		//
		//                     |<-------d------->|
		// ----*---------------*-----------------*------------> time
		//  mBaseTime       mStopTime        startTime     
		__int64 startTime = 0;
		QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

		if (mIsStopped)
		{
			mPausedTime += (startTime - mStopTime);

			mPrevTime = startTime;
			mStopTime = 0;
			mIsStopped = false;
		}
	}

	void Timer::Stop()
	{
		if (!mIsStopped)
		{
			__int64 currTime;
			QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

			mStopTime = currTime;
			mIsStopped = true;
		}
	}

	void Timer::Tick()
	{
		if (mIsStopped) // If is stopped, we don't Tick
		{
			mDeltaTime = 0.0;
			return;
		}

		// GetCurrTime
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
		mCurrTime = currTime;

		// Time difference between this frame and the previous.
		mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;

		// Prepare for next frame.
		mPrevTime = mCurrTime;

		// force nonnegative. The DXSDK's CDXUTTimer mentions that if the 
		// processor goes into a power save mode or we get shuffled to another
		// processor, then mDeltaTime can be negative.
		if (mDeltaTime < 0.0)
			mDeltaTime = 0.0;

		// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
		if (mDeltaTime > 0.15f)
			mDeltaTime = 0.05f;
	}

}
