#ifndef _IAPP_H_
#define _IAPP_H_

#include "IOperatingSystem.h"

namespace SG
{

	class IApp
	{
	public:
		IApp() = default;
		~IApp() = default;

		virtual bool OnInit() = 0;
		virtual void OnExit() = 0;

		virtual bool Load() = 0;
		virtual bool Unload() = 0;

		virtual bool OnUpdate(float deltaTime) = 0;
		virtual bool OnDraw() = 0;

		virtual const char* GetName() = 0;

		struct AppSettings
		{
			int32_t width = -1;
			int32_t height = -1;
			int32_t monitorIndex = -1;

			int32_t windowX = 0;
			int32_t windowY = 0;

			bool    fullScreen = false;
			bool    externalWindow = false;
			bool    enableResize = true;
			bool    borderlessWindow = false;
			bool    allowedOverSizeWindow = false;
			bool    initialized = false;
			bool    quit = false;
			bool    centered = true;
			bool    defaultVSyncEnable = false;
			bool    resetGraphic = false; /// use to recreate graphic device and resources 
			bool    forceLowDPI = false;
		} mSettings;

		WindowDescription* mWindow = nullptr;
		const char* mCommandLine = "";

		static int          argc;
		static const char** argv;
	};

}
	// entry point
#if defined(SG_PLATFORM_WINDOWS)
#define SG_DEFINE_APPLICATION_MAIN(appClass)						\
	int IApp::argc;												    \
	const char** IApp::argv;									    \
	extern int WindowUserMain(int argc, char** argv, IApp* app);    \
																	\
	int main(int argc, char** argv)									\
	{															    \
		IApp::argc = argc;										    \
		IApp::argv = (const char**)argv;						    \
		appClass app;											    \
		return WindowUserMain(argc, argv, &app);					\
	}																
#endif

#endif // _IAPP_H_