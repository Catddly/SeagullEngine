#pragma once

#include "Interface/IThread.h"

namespace SG
{

#define SG_MAX_LOAD_THREADS 16
#define SG_MAX_THREAD_TASK 128

	typedef void (*TaskFunc)(uintptr_t arg, void* pUserdata);

	template <class T, void (T::*callback)(size_t)>
	static void MemTaskFunction(size_t arg, void* pUserdata)
	{
		T* ptr = static_cast<T*>(pUserdata);
		(ptr->*callback)(arg);
	}

	struct ThreadTask
	{
		TaskFunc pFunc;
		void* pUser;
		uintptr_t start;
		uintptr_t end;
	};

	struct ThreadSystem
	{
		ThreadDesc threadDescs[SG_MAX_LOAD_THREADS];
		ThreadHandle handles[SG_MAX_LOAD_THREADS];
		ThreadTask tasks[SG_MAX_THREAD_TASK];
		
		uint32_t begin;
		uint32_t end;

		ConditionVariable taskQueueCv;
		Mutex taskQueueMutex;
		ConditionVariable idleCv;

		uint32_t numLoaders;
		uint32_t numIdleLoaders;

		volatile bool isRunning;
	};

	void init_thread_system(ThreadSystem** ppThreadSystem, uint32_t numRequestedThreads = SG_MAX_LOAD_THREADS, int preferCore = 0, bool migrateEnabled = true, const char* threadName = "");
	void exit_thread_system(ThreadSystem* pThreadSystem);

	void add_thread_system_range_task(ThreadSystem* pThreadSystem, TaskFunc task, void* pUser, uintptr_t count);
	void add_thread_system_range_task(ThreadSystem* pThreadSystem, TaskFunc task, void* pUser, uintptr_t start, uintptr_t end);
	void add_thread_system_task(ThreadSystem* pThreadSystem, TaskFunc task, void* pUser, uintptr_t index = 0);

	uint32_t get_thread_system_thread_count(ThreadSystem* pThreadSystem);

	bool assist_thread_system_tasks(ThreadSystem* pThreadSystem, uint32_t* pIds, size_t count);
	bool assist_thread_system(ThreadSystem* pThreadSystem);

	bool is_thread_system_idle(ThreadSystem* pThreadSystem);
	void wait_thread_system_idle(ThreadSystem* pThreadSystem);

}