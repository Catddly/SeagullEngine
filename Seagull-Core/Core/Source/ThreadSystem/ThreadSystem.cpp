#include "ThreadSystem.h"

#include <include/EASTL/algorithm.h>

#include "Interface/ILog.h"
#include "Interface/IMemory.h"

namespace SG
{

	static void task_thread_func(void* pThreadData)
	{
		ThreadSystem* ts = (ThreadSystem*)pThreadData;
		while (ts->isRunning)
		{
			ts->taskQueueMutex.Acquire();

			++(ts->numIdleLoaders);
			while (ts->isRunning && ts->begin == ts->end) // the task queue is idle
			{
				ts->idleCv.WakeAll(); // wake up all the idle queue
				ts->taskQueueCv.Wait(ts->taskQueueMutex);
			}
			--(ts->numIdleLoaders);

			if (ts->begin != ts->end)
			{
				ThreadTask& resourceTask = ts->tasks[ts->end];
				if (resourceTask.start + 1 == resourceTask.end)
				{
					++(ts->end);
					ts->end = ts->end % SG_MAX_THREAD_TASK; // back to the beginning
				}
				else
				{
					++(ts->tasks[ts->end].start);
				}
				ts->taskQueueMutex.Release();
				resourceTask.pFunc(resourceTask.start, resourceTask.pUser);
			}
			else
			{
				ts->taskQueueMutex.Release();
			}
		}

		{
			MutexLock lck(ts->taskQueueMutex);
			++(ts->numIdleLoaders);
			ts->idleCv.WakeAll();
		}
	}

	void init_thread_system(ThreadSystem** ppThreadSystem, uint32_t numRequestedThreads,
		int preferCore, bool migrateEnabled, const char* threadName)
	{
		ThreadSystem* ts = sg_new(ThreadSystem);

		uint32_t numMaxThreads = eastl::max<uint32_t>(1, Thread::get_num_CPU_cores());
		uint32_t numMaxLoaders = eastl::min<uint32_t>(numMaxThreads, eastl::min<uint32_t>(SG_MAX_LOAD_THREADS, numRequestedThreads));

		ts->taskQueueMutex.Init();
		ts->taskQueueCv.Init();
		ts->idleCv.Init();

		ts->isRunning = true;
		ts->numIdleLoaders = 0;
		ts->begin = 0;
		ts->end = 0;

		for (unsigned i = 0; i < numMaxLoaders; ++i)
		{
			ts->threadDescs[i].pFunc = task_thread_func;
			ts->threadDescs[i].pData = ts;

#if defined(NX64)
			ts->threadDescs[i].pThreadStack = aligned_alloc(THREAD_STACK_ALIGNMENT_NX, ALIGNED_THREAD_STACK_SIZE_NX);
			ts->threadDescs[i].hThread = &ts->mThreadType[i];
			ts->threadDescs[i].preferCore = preferCore;
			ts->threadDescs[i].threadName = threadName;
			ts->threadDescs[i].migrateEnabled = migrateEnabled;
#endif

			ts->handles[i] = create_thread(&ts->threadDescs[i]);
		}
		ts->numLoaders = numMaxLoaders;

		*ppThreadSystem = ts;
	}
	
	void exit_thread_system(ThreadSystem* pThreadSystem)
	{
		pThreadSystem->taskQueueMutex.Acquire();
		pThreadSystem->isRunning= false;
		pThreadSystem->taskQueueMutex.Release();

		pThreadSystem->taskQueueCv.WakeAll();
		pThreadSystem->begin = 0;
		pThreadSystem->end = 0;

		uint32_t numLoaders = pThreadSystem->numLoaders;
		for (uint32_t i = 0; i < numLoaders; ++i)
		{
			destroy_thread(pThreadSystem->handles[i]);
		}

		pThreadSystem->taskQueueCv.Destroy();
		pThreadSystem->taskQueueMutex.Destroy();
		pThreadSystem->idleCv.Destroy();
		sg_delete(pThreadSystem);
		pThreadSystem = nullptr;
	}

	void add_thread_system_range_task(ThreadSystem* pThreadSystem, TaskFunc task, void* pUser, uintptr_t count)
	{
		pThreadSystem->taskQueueMutex.Acquire();
		pThreadSystem->tasks[pThreadSystem->begin++] = ThreadTask { task, pUser, 0, count };
		pThreadSystem->begin = pThreadSystem->begin % SG_MAX_THREAD_TASK;
		if (pThreadSystem->begin == pThreadSystem->end)
			SG_LOG_ERROR("Maximum amount of thread task reached: begin(%d), end(%d), max(%d)", pThreadSystem->begin, pThreadSystem->end, SG_MAX_THREAD_TASK);
		ASSERT(pThreadSystem->begin != pThreadSystem->end);
		pThreadSystem->taskQueueMutex.Release();

		pThreadSystem->taskQueueCv.WakeAll();
	}

	void add_thread_system_range_task(ThreadSystem* pThreadSystem, TaskFunc task, void* pUser, uintptr_t start, uintptr_t end)
	{
		pThreadSystem->taskQueueMutex.Acquire();
		pThreadSystem->tasks[pThreadSystem->begin++] = ThreadTask{ task, pUser, start, end };
		pThreadSystem->begin = pThreadSystem->begin % SG_MAX_THREAD_TASK;
		if (pThreadSystem->begin == pThreadSystem->end)
			SG_LOG_ERROR("Maximum amount of thread task reached: begin(%d), end(%d), max(%d)", pThreadSystem->begin, pThreadSystem->end, SG_MAX_THREAD_TASK);
		ASSERT(pThreadSystem->begin != pThreadSystem->end);
		pThreadSystem->taskQueueMutex.Release();

		pThreadSystem->taskQueueCv.WakeAll();
	}

	void add_thread_system_task(ThreadSystem* pThreadSystem, TaskFunc task, void* pUser, uintptr_t index)
	{
		pThreadSystem->taskQueueMutex.Acquire();
		pThreadSystem->tasks[pThreadSystem->begin++] = ThreadTask{ task, pUser, index, index + 1 };
		pThreadSystem->begin = pThreadSystem->begin % SG_MAX_THREAD_TASK;
		if (pThreadSystem->begin == pThreadSystem->end)
			SG_LOG_ERROR("Maximum amount of thread task reached: begin(%d), end(%d), max(%d)", pThreadSystem->begin, pThreadSystem->end, SG_MAX_THREAD_TASK);
		ASSERT(pThreadSystem->begin != pThreadSystem->end);
		pThreadSystem->taskQueueMutex.Release();

		pThreadSystem->taskQueueCv.WakeAll();
	}

	uint32_t get_thread_system_thread_count(ThreadSystem* pThreadSystem)
	{
		return pThreadSystem->numLoaders;
	}

	bool assist_thread_system_tasks(ThreadSystem* pThreadSystem, uint32_t* pIds, size_t count)
	{
		pThreadSystem->taskQueueMutex.Acquire();
		if (pThreadSystem->begin == pThreadSystem->end) // no tasks
		{
			pThreadSystem->taskQueueMutex.Release();
			return false;
		}

		uint32_t taskSize = pThreadSystem->begin >= pThreadSystem->end ? pThreadSystem->begin - pThreadSystem->end 
			: SG_MAX_THREAD_TASK - (pThreadSystem->end - pThreadSystem->begin);
		ThreadTask resourceTask;

		bool found = false;
		for (uint32_t i = 0; i < taskSize; ++i)
		{
			uint32_t index = (pThreadSystem->end + i) % SG_MAX_THREAD_TASK;
			resourceTask = pThreadSystem->tasks[index];

			for (size_t j = 0; j < count; ++j)
			{
				if (pIds[j] == resourceTask.start)
				{
					found = true;
					break;
				}
			}

			if (found)
			{
				if (resourceTask.start + 1 == resourceTask.end)
				{
					pThreadSystem->tasks[index] = pThreadSystem->tasks[pThreadSystem->end];
					++pThreadSystem->end;
					pThreadSystem->end = pThreadSystem->end % SG_MAX_THREAD_TASK;
				}
				else
				{
					++pThreadSystem->tasks[index].start;
				}
				break;
			}
		}

		pThreadSystem->taskQueueMutex.Release();

		if (!found)
		{
			return false;
		}

		resourceTask.pFunc(resourceTask.start, resourceTask.pUser);
		return true;
	}

	bool assist_thread_system(ThreadSystem* pThreadSystem)
	{
		pThreadSystem->taskQueueMutex.Acquire();
		if (pThreadSystem->begin != pThreadSystem->end)
		{
			ThreadTask resourceTask = pThreadSystem->tasks[pThreadSystem->end];

			if (resourceTask.start + 1 == resourceTask.end)
			{
				++pThreadSystem->end;
				pThreadSystem->end = pThreadSystem->end % SG_MAX_THREAD_TASK;
			}
			else
			{
				++pThreadSystem->tasks[pThreadSystem->end].start;
			}
			pThreadSystem->taskQueueMutex.Release();
			resourceTask.pFunc(resourceTask.start, resourceTask.pUser);

			return true;
		}
		else
		{
			pThreadSystem->taskQueueMutex.Release();
			return false;
		}
	}

	bool is_thread_system_idle(ThreadSystem* pThreadSystem)
	{
		pThreadSystem->taskQueueMutex.Acquire();
		bool idle = (pThreadSystem->begin == pThreadSystem->end && pThreadSystem->numIdleLoaders == pThreadSystem->numLoaders) || !pThreadSystem->isRunning;
		pThreadSystem->taskQueueMutex.Release();

		return idle;
	}

	void wait_thread_system_idle(ThreadSystem* pThreadSystem)
	{
		pThreadSystem->taskQueueMutex.Acquire();
		// when the task queue is not idle
		while ((pThreadSystem->begin != pThreadSystem->end || pThreadSystem->numIdleLoaders < pThreadSystem->numLoaders) && pThreadSystem->isRunning)
			pThreadSystem->idleCv.Wait(pThreadSystem->taskQueueMutex);
		pThreadSystem->taskQueueMutex.Release();
	}

}