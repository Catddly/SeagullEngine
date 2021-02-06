#pragma once

#include "IOperatingSystem.h"

#ifdef SG_PLATFORM_WINDOWS
	typedef unsigned long ThreadID;
#endif

#define TIMEOUT_INFINITE UINT32_MAX

namespace SG
{

	struct Mutex
	{
		static const uint32_t sDefaultSpinCount = 1500;

		bool Init(uint32_t spinCount = sDefaultSpinCount, const char* name = NULL);
		void Destroy();

		void Acquire();
		bool TryAcquire();
		void Release();

#ifdef SG_PLATFORM_WINDOWS
		CRITICAL_SECTION mHandle;
#endif
	};

	struct MutexLock
	{
		MutexLock(Mutex& mtx) : mMutex(mtx) { mtx.Acquire(); }
		~MutexLock() { mMutex.Release(); }

		MutexLock(const MutexLock& rhs) = delete;
		MutexLock& operator=(const MutexLock& rhs) = delete;

		Mutex& mMutex;
	};

	struct ConditionVariable /// cv in std
	{
		bool Init(const char* name = NULL);
		void Destroy();

		void Wait(const Mutex& mutex, uint32_t ms = TIMEOUT_INFINITE);
		void WakeOne();
		void WakeAll();

#ifdef SG_PLATFORM_WINDOWS
		void* mHandle;
#endif
	};

	typedef void(*ThreadFunc)(void*);

	/// per element in the word queue
	struct ThreadDesciption
	{
		ThreadFunc pFunc;
		void* pData;
	};

#ifdef SG_PLATFORM_WINDOWS
	typedef void* ThreadHandle;
#endif

	ThreadHandle create_thread(ThreadDesciption* element);
	void         destroy_thread(ThreadHandle handle);
	void         join_thread(ThreadHandle handle);

	struct Thread
	{
		static ThreadID     mainThreadID;
		static void         set_main_thread();
		static ThreadID     get_curr_thread_id();
		static void         get_curr_thread_name(char* buffer, int bufferSize);
		static void         set_curr_thread_name(const char* name);
		static bool         is_main_thread();
		static void         sleep(unsigned mSec);
		static unsigned int get_num_CPU_cores();
	};

#ifndef SG_MAX_THREAD_NAME_LENGTH
#define SG_MAX_THREAD_NAME_LENGTH 31
#endif

#ifdef SG_PLATFORM_WINDOWS
	void sleep(unsigned second);
#endif

}