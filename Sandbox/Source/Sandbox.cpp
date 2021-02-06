// interfaces

#include <iostream>

#include "Seagull.h"

using namespace SG;

class ExampleApp : public IApp
{
public:

	virtual bool OnInit() override
	{
		std::cout << "OnInit()\n";
		// SG_LOG_DEBUG("Example app::OnInit()");
		return true;
	}

	virtual void OnExit() override
	{
		std::cout << "OnExit()\n";
	}

	virtual bool Load() override
	{
		std::cout << "OnLoad()\n";
		return true;
	}

	virtual bool Unload() override
	{
		std::cout << "OnUnload()\n";
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
	const char* mName = "Example";
};

SG_DEFINE_APPLICATION_MAIN(ExampleApp)