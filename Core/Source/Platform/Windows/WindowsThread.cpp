#ifdef SG_PLATFORM_WINDOWS

#include "Interface/IThread.h"
#include "Interface/IMemory.h"

#include <include/EASTL/EAAssert/eaassert.h>
#include <stdio.h>

namespace SG
{

	__declspec(selectany) const uint32_t Mutex::sDefaultSpinCount;

	bool Mutex::Init(uint32_t spinCount, const char* name)
	{
		return InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION*)&mHandle,
			(DWORD)spinCount);
	}

	void Mutex::Destroy()
	{
		auto* criticalSection = (CRITICAL_SECTION*)&mHandle;
		DeleteCriticalSection(criticalSection);
		mHandle = {};
	}

	void Mutex::Acquire()
	{
		EnterCriticalSection((CRITICAL_SECTION*)&mHandle);
	}

	bool Mutex::TryAcquire()
	{
		return TryEnterCriticalSection((CRITICAL_SECTION*)&mHandle);
	}

	void Mutex::Release()
	{
		LeaveCriticalSection((CRITICAL_SECTION*)&mHandle);
	}

	bool ConditionVariable::Init(const char* name /*= NULL*/)
	{
		mHandle = (CONDITION_VARIABLE*)sg_calloc(1, sizeof(CONDITION_VARIABLE));
		InitializeConditionVariable((PCONDITION_VARIABLE)&mHandle);
		return true;
	}

	void ConditionVariable::Destroy()
	{
		sg_free(mHandle);
	}

	void ConditionVariable::Wait(const Mutex& mutex, uint32_t ms)
	{
		SleepConditionVariableCS((PCONDITION_VARIABLE)&mHandle,
			(PCRITICAL_SECTION)&mutex.mHandle, ms);
	}

	void ConditionVariable::WakeOne()
	{
		WakeConditionVariable((PCONDITION_VARIABLE)&mHandle);
	}

	void ConditionVariable::WakeAll()
	{
		WakeAllConditionVariable((PCONDITION_VARIABLE)&mHandle);
	}

	DWORD WINAPI ThreadFunctionStatic(void* data)
	{
		auto* pDesc = (ThreadDesciption*)data;
		pDesc->pFunc(pDesc->pData);
		return 0;
	}

	ThreadHandle create_thread(ThreadDesciption* element)
	{
		ThreadHandle handle = CreateThread(0, 0, ThreadFunctionStatic,
			element, 0, 0);
		EA_ASSERT(handle != NULL);
		return handle;
	}

	void destroy_thread(ThreadHandle handle)
	{
		EA_ASSERT(handle != NULL);
		WaitForSingleObject((HANDLE)handle, INFINITE);
		CloseHandle((HANDLE)handle);
		handle = nullptr;
	}

	void join_thread(ThreadHandle handle)
	{
		WaitForSingleObject((HANDLE)handle, INFINITE);
	}

	void sleep(unsigned second)
	{
		::Sleep((DWORD)second);
	}

	__declspec(selectany) ThreadID Thread::mainThreadID;

	void Thread::set_main_thread()
	{
		mainThreadID = get_curr_thread_id();
	}

	ThreadID Thread::get_curr_thread_id()
	{
		return GetCurrentThreadId();
	}

	char* thread_name()
	{
		// thread local variable
		__declspec(thread) static char name[SG_MAX_THREAD_NAME_LENGTH + 1];
		return name;
	}

	void Thread::get_curr_thread_name(char* buffer, int bufferSize)
	{
		if (const char* name = thread_name())
			snprintf(buffer, (size_t)bufferSize, "%s", name);
		else
			buffer[0] = 0;
	}

	void Thread::set_curr_thread_name(const char* name)
	{
		strcpy_s(thread_name(), SG_MAX_THREAD_NAME_LENGTH + 1, name);
	}

	bool Thread::is_main_thread()
	{
		return get_curr_thread_id() == mainThreadID;
	}

	void Thread::sleep(unsigned mSec)
	{
		::Sleep(mSec);
	}

	unsigned int Thread::get_num_CPU_cores()
	{
		_SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);
		return systemInfo.dwNumberOfProcessors;
	}

}
#endif // #ifdef SG_PLATFORM_WINDOWS