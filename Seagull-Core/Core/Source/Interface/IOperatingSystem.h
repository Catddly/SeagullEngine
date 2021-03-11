#pragma once

#if defined(SG_PLATFORM_WINDOWS)
	#include <sys/stat.h>
	#include <stdlib.h>
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN 1
	#endif
	#include <windows.h>
	#undef min
	#undef max
#endif

#include "Core/CompilerConfig.h"

#include "Math/MathTypes.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// for time related functions such as getting localtime
#include <time.h>
#include <ctime>

#include <float.h>
#include <limits.h>
#include <stddef.h>

namespace SG
{

	typedef struct WindowHandle
	{
		void* window;
	} WindowHandle;

	typedef struct RectDescription
	{
		int32_t left;
		int32_t top;
		int32_t right;
		int32_t bottom;
	} RectDescription;

	static inline int32_t get_rect_width(const RectDescription& rect) { return rect.right - rect.left; }
	static inline int32_t get_rect_height(const RectDescription& rect) { return rect.bottom - rect.top; }

	typedef struct WindowDesc
	{
		WindowHandle handle;
		RectDescription windowedRect;
		RectDescription fullscreenRect;
		RectDescription clientRect;
		bool fullScreen;
		bool cursorTracked;
		bool iconified;
		bool maximized;
		bool minimized;
		bool hide;
		bool noResizeFrame;
		bool borderlessWindow;
		bool overrideDefaultPosition;
		bool centered;
		bool forceLowDPI;
	} WindowDesc;

	typedef struct Resolution
	{
		uint32_t width;
		uint32_t height;
	} Resolution;

	// monitor data
	typedef struct MonitorDescription
	{
		RectDescription   monitorRect;
		RectDescription   workRect;
		unsigned          dpiX;
		unsigned          dpiY;
		unsigned          physicalSizeX;
		unsigned          physicalSizeY;
#if defined(SG_PLATFORM_WINDOWS)
		WCHAR             adapterName[32];
		WCHAR             displayName[32];
		WCHAR             publicAdapterName[128];
		WCHAR             publicDisplayName[128];
#endif
		Resolution* resolutions;
		Resolution        defaultResolution;
		uint32_t          resolutionCount;
		bool              modesPruned;
		bool              modeChanged;
	} MonitorDescription;

	// api functions
	void request_shutdown();

	// custom processing of core pipiline messages
	typedef int32_t(*custom_message_processor)(WindowDesc* pWindow, void* pMsg);
	void set_custom_message_processor(custom_message_processor pMsgProcessor);

	// window handling
	void open_window(const char* appName, WindowDesc* pWindow);
	void close_window(const WindowDesc* pWindow);

	void set_window_rect(WindowDesc* pWindow, const RectDescription& rect);
	void set_window_size(WindowDesc* pWindow, uint32_t width, uint32_t height);

	void show_window(WindowDesc* pWindow);
	void hide_window(WindowDesc* pWindow);
	void maximize_window(WindowDesc* pWindow);
	void minimize_window(WindowDesc* pWindow);
	void center_window(WindowDesc* pWindow);

	void toggle_fullscreen(WindowDesc* pWindow);

	// mouse and cursor handling
	void* create_cursor(const char* path);
	void  set_cursor(void* pCursor);

	bool  is_cursor_inside_tracking_area(RectDescription* pRect);

	void set_mouse_position_relative(const WindowDesc* pWindow, int32_t x, int32_t y);
	void set_mouse_position_absolutely(int32_t x, int32_t y);

	void get_recommanded_resolution(RectDescription* pRect);
	void set_resolution(const MonitorDescription* pMonitor, const Resolution* pResolution);

	MonitorDescription* get_monitor(uint32_t index);
	uint32_t            get_monitor_count();
	Vec2		        get_dpi_scale();

	bool get_resolution_support(const MonitorDescription* pMonitor, const Resolution* pResolution);

	/// @param stdOutFile The file to which the output of the command should be written. May be NULL.
	int system_run(const char* command, const char** arguments, size_t argumentCount, const char* stdOutFile);
}
