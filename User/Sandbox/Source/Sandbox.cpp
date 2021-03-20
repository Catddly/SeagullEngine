
#include "Seagull.h"

using namespace SG;

int gCounter = 0;
Mutex mMutex;
ConditionVariable cv;

bool ready = false;

void ThreadTestFunc(void* data)
{
	MutexLock lck(mMutex);
	while (!ready)
		cv.Wait(mMutex);

	char num[4];
	itoa(gCounter, num, 10);
	char msg[64] = "WorkerThread-";
	strncat(msg, num, 1);
	Thread::set_curr_thread_name(msg);
	SG_LOG_DEBUG("ThreadID:%-06lu (print:%d)", Thread::get_curr_thread_id(), gCounter);
	++gCounter;
}

class ExampleApp : public IApp
{
public:

	virtual bool OnInit() override
	{
		//Logger::SetQuiet(true);

		SG_LOG_INFO("OnInit()");

		SG_LOG_INFO("|*************************  Begin Renderer Test  ************************|");
		RendererTest();
		SG_LOG_INFO("|*************************  End   Renderer Test  ************************|");

		SG_LOG_INFO("|*************************  Begin Memory Test  ************************|");
		MemoryTest();						  
		SG_LOG_INFO("|*************************  End   Memory Test  ************************|");

		SG_LOG_INFO("|*************************  Begin Thread Test  ************************|");
		ThreadTest();						  
		SG_LOG_INFO("|*************************  End   Thread Test  ************************|");

		return true;
	}

	virtual void OnExit() override
	{
		mMutex.Destroy();
		cv.Destroy();

		remove_renderer(mRenderer);

		for (int i = 0; i < 6; i++)
		{
			sg_free(nums[i]);
		}
		for (int i = 0; i < 6; i++)
		{
			destroy_thread(mThreads[i]);
		}

		SG_LOG_INFO("OnExit()");
	}

	virtual bool OnLoad() override
	{
		SG_LOG_INFO("OnLoad()");
		return true;
	}

	virtual bool OnUnload() override
	{
		SG_LOG_INFO("OnUnload()");
		return true;
	}

	virtual bool OnUpdate(float deltaTime) override
	{
		//std::cout << "OnUpdate(" << deltaTime << ")\n";
		//mSettings.quit = true;
		return true;
	}

	virtual bool OnDraw() override
	{
		//std::cout << "OnDraw()\n";
		return true;
	}

	virtual const char* GetName() override
	{
		return mName;
	}

private:
	void MemoryTest()
	{
		Timer t("MemoryTestTimer");
		t.Reset();
		auto* ptr = (int*)sg_malloc(sizeof(int));
		*ptr = 54;
		SG_LOG_DEBUG("dynamic malloc ptr = %d", *ptr);
		sg_free(ptr);
		t.Tick();
		SG_LOG_DEBUG("Memory test duration: (%.4f ms)", t.GetTotalTime() * 1000.0f);
	}

	void ThreadTest()
	{
		Timer t("ThreadTestTimer");
		t.Reset();
		mMutex.Init();
		cv.Init();
		Thread::set_main_thread();
		Thread::set_curr_thread_name("MainThread");
		SG_LOG_DEBUG("Available threads count : %d", Thread::get_num_CPU_cores());

		for (int i = 0; i < 6; i++)
		{
			nums[i] = (int*)sg_malloc(sizeof(int));
			*nums[i] = i;
			ThreadDesc desc;
			desc.pFunc = ThreadTestFunc;
			desc.pData = nums[i];
			mThreads[i] = create_thread(&desc);
		}

		SG_LOG_DEBUG("6 threads are ready~!");
		ready = true;
		cv.WakeAll();

		for (int i = 0; i < 6; i++)
		{
			join_thread(mThreads[i]);
		}
		char threadName[SG_MAX_THREAD_NAME_LENGTH];
		Thread::get_curr_thread_name(threadName, SG_MAX_THREAD_NAME_LENGTH);
		SG_LOG_DEBUG("Thread: %s", threadName);
		SG_LOG_DEBUG("%d", gCounter);
		t.Tick();
		SG_LOG_DEBUG("Thread test duration: (%.4f ms)", t.GetTotalTime() * 1000.0f);
	}

	void RendererTest()
	{
		RendererCreateDesc desc{};
		desc.api = SG_RENDERER_API_VULKAN;
		desc.gpuMode = SG_GPU_MODE_SINGLE;
		desc.pLogFn = CustomLogFunc;
		desc.shaderTarget = SG_SHADER_TARGET_6_0;
		
		desc.ppInstanceLayers = nullptr;
		desc.instanceLayerCount = 0;

		desc.ppDeviceExtensions = const_cast<const char**>(mRequiredDeviceExtensions.data());
		desc.deviceExtensionCount = static_cast<uint32_t>(mRequiredDeviceExtensions.size());

		desc.ppInstanceExtensions = nullptr;
		desc.instanceExtensionCount = 0;

		desc.requestAllAvailableQueues = false;

		init_renderer("Vulkan Test", &desc, &mRenderer);

		SG_LOG_INFO("Renderer Iniialized successfully!");
	}

	static void CustomLogFunc(LogType type, const char* t, const char* msg)
	{
		SG_LOG(type, "%s :(%s)", t, msg);
	}
private:
	const char* mName = "Example";
	int* nums[6];
	ThreadHandle mThreads[6];

	Renderer* mRenderer = nullptr;

	const eastl::vector<const char*> mRequiredDeviceExtensions = {
		"VK_KHR_swapchain"
	};
};

class DummyApp : public IApp
{
	virtual bool OnInit() override
	{
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
		return "DummyApp";
	}
};

//SG_DEFINE_APPLICATION_MAIN(ExampleApp)