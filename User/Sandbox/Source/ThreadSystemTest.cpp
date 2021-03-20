
#include "Seagull.h"

using namespace SG;

uint32_t counter = 0;

static void TaskFunction(uintptr_t arg, void* pUserdata)
{
	++*(uint32_t*)pUserdata;
	{
		SG_LOG_DEBUG("Task is running (ThreadId: %ul)", Thread::get_curr_thread_id());
	}
}

class ThreadSystemTestApp : public IApp
{
	virtual bool OnInit() override
	{
		Timer t;
		t.Reset();

		init_thread_system(&mThreadSystem, 16);

		add_thread_system_range_task(mThreadSystem, TaskFunction, &counter, 1500);

		if (is_thread_system_idle(mThreadSystem))
			SG_LOG_DEBUG("Thread system is idle");
		else
			SG_LOG_DEBUG("Thread system is not idle");

		wait_thread_system_idle(mThreadSystem);

		SG_LOG_DEBUG("Counter: %d", counter);

		if (is_thread_system_idle(mThreadSystem))
			SG_LOG_DEBUG("Thread system is idle");
		else
			SG_LOG_DEBUG("Thread system is not idle");

		exit_thread_system(mThreadSystem);

		mSettings.quit = true;
		
		t.Tick();
		SG_LOG_DEBUG("Duration: (%fs)", t.GetTotalTime());

		return true;
	}

	virtual void OnExit() override
	{
	}

	virtual bool OnLoad() override
	{
		return true;
	}

	virtual bool OnUnload() override
	{
		return true;
	}

	virtual bool OnUpdate(float deltaTime) override
	{
		//SG_LOG_INFO("Frame: %.1f", 1.0f / deltaTime);
		return true;
	}

	virtual bool OnDraw() override
	{
		return true;
	}

	virtual const char* GetName() override
	{
		return "ThreadSystemTestApp";
	}
private:
	ThreadSystem* mThreadSystem = nullptr;
};

//SG_DEFINE_APPLICATION_MAIN(ThreadSystemTestApp);