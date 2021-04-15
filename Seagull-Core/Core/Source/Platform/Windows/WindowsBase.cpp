#ifdef SG_PLATFORM_WINDOWS

#include "Core/CompilerConfig.h"

#include <include/EASTL/vector.h>
#include <include/EASTL/unordered_map.h>
#include <include/EASTL/algorithm.h>

#include "Interface/IOperatingSystem.h"
#include "Interface/ILog.h"
#include "Interface/ITime.h"
#include "Interface/IThread.h"
#include "Interface/IApp.h"
#include "Interface/IFileSystem.h"
#include "Interface/IMemory.h"

#define SG_WINDOW_CLASS L"Seagull Engine"
#define MAX_KEYS 256

#if !defined(XBOX)
	#include <shlwapi.h>
	#pragma comment(lib, "shlwapi.lib")
#endif

#define GETX(n) ((int)LOWORD(n))
#define GETY(n) ((int)HIWORD(n))

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

namespace SG
{

	struct MonitorInfo
	{
		unsigned index;
		WCHAR adapterName[32];
	};

	static IApp* pApp = nullptr;
	static bool  gWindowClassInitialized = false;
	static WNDCLASSEX gWindowClassEx;
	static MonitorDescription* gMonitors = nullptr;
	static uint32_t gMonitorCount = 0;
	static WindowDesc* gWindow = nullptr;
	static bool gWindowIsResizing = false;
	static bool gCursorVisible = true;
	static bool gCursorInsideRectangle = false;

	static custom_message_processor sCustomMsgProc = nullptr;

	DWORD prepare_window_style_mask(WindowDesc* pWindow);
	DWORD prepare_window_style_mask_ex(WindowDesc* pWindow);

	// helper functions
	/// update window rect from monitor
	static void update_window_fullscreen_rect(WindowDesc* pWindow)
	{
		HMONITOR currMonitor = MonitorFromWindow((HWND)pApp->mWindow->handle.window, MONITOR_DEFAULTTONEAREST);
		MONITORINFOEX info;
		info.cbSize = sizeof(MONITORINFOEX);
		bool infoRead = GetMonitorInfo(currMonitor, &info);

		pWindow->fullscreenRect.left = info.rcMonitor.left;
		pWindow->fullscreenRect.top = info.rcMonitor.top;
		pWindow->fullscreenRect.right = info.rcMonitor.right;
		pWindow->fullscreenRect.bottom = info.rcMonitor.bottom;
	}

	static void update_window_windowed_rect(WindowDesc* pWindow)
	{
		RECT windowedRect;
		HWND hwnd = (HWND)pWindow->handle.window;

		GetWindowRect(hwnd, &windowedRect);

		pWindow->windowedRect.left = windowedRect.left;
		pWindow->windowedRect.right = windowedRect.right;
		pWindow->windowedRect.top = windowedRect.top;
		pWindow->windowedRect.bottom = windowedRect.bottom;
	}

	static void update_window_client_rect(WindowDesc* pWindow)
	{
		RECT clientRect;
		HWND hwnd = (HWND)pWindow->handle.window;

		GetClientRect(hwnd, &clientRect);

		pWindow->clientRect.left = clientRect.left;
		pWindow->clientRect.right = clientRect.right;
		pWindow->clientRect.top = clientRect.top;
		pWindow->clientRect.bottom = clientRect.bottom;
	}

	static void on_resize(WindowDesc* pWindow, int32_t newSizeX, int32_t newSizeY)
	{
		if (pApp == nullptr || !pApp->mSettings.initialized) // app doesn't exist
			return;

		pApp->mSettings.fullScreen = pWindow->fullScreen;
		if (pApp->mSettings.width == newSizeX && pApp->mSettings.height == newSizeY) // it is the same before
			return;

		pApp->mSettings.width = newSizeX;
		pApp->mSettings.height = newSizeY;

		pApp->OnUnload();
		pApp->OnLoad();
	}

	static void adjust_window(WindowDesc* pWindow)
	{
		HWND hwnd = (HWND)pWindow->handle.window;

		if (pWindow->fullScreen)
		{
			RECT windowedRect = {};
			// save the old window rect so we can restore it when exiting fullscreen mode.
			GetWindowRect(hwnd, &windowedRect);
			pWindow->windowedRect = { (int)windowedRect.left, (int)windowedRect.top, (int)windowedRect.right, (int)windowedRect.bottom };

			// make the window borderless so that the client area can fill the screen.
			SetWindowLong(hwnd, GWL_STYLE, WS_SYSMENU | WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE);

			// get the settings of the current display index. We want the app to go into
			// fullscreen mode on the display that supports independent flip.
			HMONITOR currentMonitor = MonitorFromWindow((HWND)pApp->mWindow->handle.window, MONITOR_DEFAULTTOPRIMARY);
			MONITORINFOEX info;
			info.cbSize = sizeof(MONITORINFOEX);
			bool infoRead = GetMonitorInfo(currentMonitor, &info);

			pApp->mSettings.windowX = info.rcMonitor.left;
			pApp->mSettings.windowY = info.rcMonitor.top;

			// set the window position to the center of the screen
			SetWindowPos(hwnd, HWND_NOTOPMOST,
				info.rcMonitor.left, info.rcMonitor.top,
				info.rcMonitor.right - info.rcMonitor.left,
				info.rcMonitor.bottom - info.rcMonitor.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE);
			ShowWindow(hwnd, SW_MAXIMIZE);

			on_resize(pWindow, info.rcMonitor.right - info.rcMonitor.left,
				info.rcMonitor.bottom - info.rcMonitor.top);
		}
		else
		{
			{
				DWORD windowStyle = prepare_window_style_mask(pWindow);
				SetWindowLong(hwnd, GWL_STYLE, windowStyle);
				DWORD windowStyleEx = prepare_window_style_mask_ex(pWindow);
				SetWindowLong(hwnd, GWL_EXSTYLE, windowStyleEx);

				SetWindowPos(hwnd, HWND_NOTOPMOST,
					pWindow->windowedRect.left,
					pWindow->windowedRect.top,
					get_rect_width(pWindow->windowedRect),
					get_rect_height(pWindow->windowedRect),
					SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

				if (pWindow->maximized)
					ShowWindow(hwnd, SW_MAXIMIZE);
				else
					ShowWindow(hwnd, SW_NORMAL);
			}
		}
	}

	static inline float counter_to_seconds_elapsed(int64_t start, int64_t end)
	{
		return (float)(end - start) / (float)1e6;
	}
	// helper functions end

	static BOOL CALLBACK monitor_callback(HMONITOR pMonitor, HDC pDeviceContext, LPRECT pRect, LPARAM pParam)
	{
		MONITORINFOEXW info;
		info.cbSize = sizeof(info);
		GetMonitorInfoW(pMonitor, &info);
		MonitorInfo* data = (MonitorInfo*)pParam;
		unsigned index = data->index;

		if (wcscmp(info.szDevice, data->adapterName) == 0)
		{
			gMonitors[index].monitorRect = { (int)info.rcMonitor.left, (int)info.rcMonitor.top, (int)info.rcMonitor.right,
												(int)info.rcMonitor.bottom };
			gMonitors[index].workRect = { (int)info.rcWork.left, (int)info.rcWork.top, (int)info.rcWork.right, (int)info.rcWork.bottom };
		}
		return TRUE;
	}

	// Window event handler - Use as less as possible
	LRESULT CALLBACK WinProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (gWindow == nullptr)
			return DefWindowProcW(hwnd, message, wParam, lParam);

		switch (message)
		{
		case WM_NCPAINT:
		case WM_WINDOWPOSCHANGED:
		case WM_STYLECHANGED:
		{
			return DefWindowProcW(hwnd, message, wParam, lParam);
		}
		case WM_DISPLAYCHANGE:
		{
			adjust_window(gWindow);
			break;
		}
		case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;

			// These sizes should be well enough to accommodate any possible 
			// window styles in full screen such that the client rectangle would
			// be equal to the fullscreen rectangle without cropping or movement.
			// These sizes were tested with 450% zoom, and should support up to
			// about 550% zoom, if such option exists.
			if (!gWindow->fullScreen)
			{
				LONG zoomOffset = 128;
				lpMMI->ptMaxPosition.x = -zoomOffset;
				lpMMI->ptMaxPosition.y = -zoomOffset;
				lpMMI->ptMinTrackSize.x = zoomOffset;
				lpMMI->ptMinTrackSize.y = zoomOffset;
				lpMMI->ptMaxTrackSize.x = gWindow->clientRect.left + get_rect_width(gWindow->clientRect) + zoomOffset;
				lpMMI->ptMaxTrackSize.y = gWindow->clientRect.top + get_rect_height(gWindow->clientRect) + zoomOffset;
			}
			break;
		}
		case WM_ERASEBKGND:
		{
			// Make sure to keep consistent background color when resizing.
			HDC hdc = (HDC)wParam;
			RECT rc;
			HBRUSH hbrWhite = CreateSolidBrush(0x00000000);
			GetClientRect(hwnd, &rc);
			FillRect(hdc, &rc, hbrWhite);
			break;
		}
		case WM_WINDOWPOSCHANGING:
		case WM_MOVE:
		{
			update_window_fullscreen_rect(gWindow);
			if (!gWindow->fullScreen)
				update_window_windowed_rect(gWindow);
			break;
		}
		case WM_STYLECHANGING:
		{
			break;
		}
		case WM_SIZE:
		{
			switch (wParam)
			{
			case SIZE_RESTORED:
			case SIZE_MAXIMIZED:
				gWindow->minimized = false;
				if (!gWindow->fullScreen && !gWindowIsResizing)
				{
					update_window_client_rect(gWindow);
					on_resize(gWindow, get_rect_width(gWindow->clientRect), get_rect_height(gWindow->clientRect));
				}
				break;
			case SIZE_MINIMIZED:
				gWindow->minimized = true;
				break;
			default:
				break;
			}

			break;
		}
		case WM_ENTERSIZEMOVE:
		{
			gWindowIsResizing = true;
			break;
		}
		case WM_EXITSIZEMOVE:
		{
			gWindowIsResizing = false;
			if (!gWindow->fullScreen)
			{
				update_window_client_rect(gWindow);
				on_resize(gWindow, get_rect_width(gWindow->clientRect), get_rect_height(gWindow->clientRect));
			}
			break;
		}
		case WM_SETCURSOR:
		{
			if (LOWORD(lParam) == HTCLIENT)
			{
				if (!gCursorInsideRectangle)
				{
					HCURSOR cursor = LoadCursor(NULL, IDC_ARROW);
					SetCursor(cursor);

					gCursorInsideRectangle = true;
				}
			}
			else
			{
				gCursorInsideRectangle = false;
				return DefWindowProcW(hwnd, message, wParam, lParam);
			}
			break;
		}
		case WM_DESTROY:
		case WM_CLOSE:
		{
			PostQuitMessage(0);
			break;
		}
		default:
		{
			if (sCustomMsgProc != nullptr)
			{
				MSG msg = {};
				msg.hwnd = hwnd;
				msg.lParam = lParam;
				msg.message = message;
				msg.wParam = wParam;

				sCustomMsgProc(gWindow, &msg);
			}

			return DefWindowProcW(hwnd, message, wParam, lParam);
		}
		}
		return 0;
	}

	DWORD prepare_window_style_mask(WindowDesc* pWindow)
	{
		DWORD windowStyle = WS_OVERLAPPEDWINDOW;
		if (pWindow->borderlessWindow)
		{
			windowStyle = WS_POPUP | WS_THICKFRAME; // WS_THICKFRAME(no window stretching)
		}
		if (pWindow->noResizeFrame)
		{
			windowStyle ^= WS_THICKFRAME | WS_MAXIMIZEBOX;
		}
		if (!pWindow->hide)
		{
			windowStyle |= WS_VISIBLE;
		}
		return windowStyle;
	}

	DWORD prepare_window_style_mask_ex(WindowDesc* pWindow)
	{
		DWORD windowStyle = WS_EX_OVERLAPPEDWINDOW;
		return windowStyle;
	}

	MonitorDescription* get_monitor(uint32_t index)
	{
		ASSERT(gMonitorCount > index);
		return &gMonitors[index];
	}

	uint32_t get_monitor_count()
	{
		return gMonitorCount;
	}

	void get_recommanded_resolution(RectDescription* pRect)
	{
		*pRect =
		{
			0, 0,
			std::min(1920, (int)(GetSystemMetrics(SM_CXSCREEN) * 0.75)),
			std::min(1080, (int)(GetSystemMetrics(SM_CYSCREEN) * 0.75))
		};
	}

	void set_resolution(const MonitorDescription* pMonitor, const Resolution* pResolution)
	{
		DEVMODEW devMode = {};
		devMode.dmSize = sizeof(DEVMODEW);
		devMode.dmPelsWidth = pResolution->width;   // 指定可见设备图面的宽度（以像素为单位）
		devMode.dmPelsHeight = pResolution->height; // 指定可见设备图面的高度（以像素为单位）
		devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
		ChangeDisplaySettingsW(&devMode, CDS_FULLSCREEN);
	}

	void offset_rect_to_display(WindowDesc* pwindow, LPRECT rect)
	{
		int32_t displayOffsetX = pwindow->fullscreenRect.left;
		int32_t displayOffsetY = pwindow->fullscreenRect.top;

		// adjust for display coordinates in absolute virtual
		// display space.
		rect->left += (LONG)displayOffsetX;
		rect->top += (LONG)displayOffsetY;
		rect->right += (LONG)displayOffsetX;
		rect->bottom += (LONG)displayOffsetY;
	}

	void center_window(WindowDesc* pWindow)
	{
		update_window_fullscreen_rect(pWindow);

		uint32_t fsHalfWidth = get_rect_width(pWindow->fullscreenRect) >> 1;
		uint32_t fsHalfHeight = get_rect_height(pWindow->fullscreenRect) >> 1;
		uint32_t windowWidth = get_rect_width(pWindow->clientRect);
		uint32_t windowHeight = get_rect_height(pWindow->clientRect);
		uint32_t windowHalfWidth = windowWidth >> 1;
		uint32_t windowHalfHeight = windowHeight >> 1;

		uint32_t X = fsHalfWidth - windowHalfWidth;
		uint32_t Y = fsHalfHeight - windowHalfHeight;

		RECT rect =
		{
			(LONG)(X),
			(LONG)(Y),
			(LONG)(X + windowWidth),
			(LONG)(Y + windowHeight)
		};

		DWORD windowStyle = prepare_window_style_mask(pWindow);
		DWORD windowStyleEx = prepare_window_style_mask_ex(pWindow);

		AdjustWindowRectEx(&rect, windowStyle, FALSE, windowStyleEx);
		offset_rect_to_display(pWindow, &rect);

		SetWindowPos((HWND)pWindow->handle.window,
			HWND_NOTOPMOST,
			rect.left, rect.top,
			rect.right - rect.left, rect.bottom - rect.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

		pWindow->windowedRect =
		{
			(int32_t)rect.left,
			(int32_t)rect.top,
			(int32_t)rect.right,
			(int32_t)rect.bottom
		};
	}

	void set_window_rect(WindowDesc* pWindow, const RectDescription& rect)
	{
		HWND hwnd = (HWND)pWindow->handle.window;

		// Adjust position to prevent the window from dancing around
		int clientWidthStart = (get_rect_width(pWindow->windowedRect) - get_rect_width(pWindow->clientRect)) >> 1;
		int clientHeightStart = get_rect_height(pWindow->windowedRect) - get_rect_height(pWindow->clientRect) - clientWidthStart;

		pWindow->clientRect = rect;

		DWORD windowStyle = prepare_window_style_mask(pWindow);
		SetWindowLong(hwnd, GWL_STYLE, windowStyle);
		DWORD windowStyleEx = prepare_window_style_mask_ex(pWindow);
		SetWindowLong(hwnd, GWL_EXSTYLE, windowStyleEx);

		if (pWindow->centered)
			center_window(pWindow);
		else
		{
			RECT clientRectStyleAdjusted =
			{
				(LONG)(rect.left + clientWidthStart),
				(LONG)(rect.top + clientHeightStart),
				(LONG)(clientRectStyleAdjusted.left + get_rect_width(rect)),
				(LONG)(clientRectStyleAdjusted.top + get_rect_height(rect))
			};
			::AdjustWindowRectEx(&clientRectStyleAdjusted, windowStyle, FALSE, windowStyleEx);

			pWindow->windowedRect =
			{
				(int32_t)clientRectStyleAdjusted.left,
				(int32_t)clientRectStyleAdjusted.top,
				(int32_t)clientRectStyleAdjusted.right,
				(int32_t)clientRectStyleAdjusted.bottom
			};
			::SetWindowPos(hwnd,
				HWND_NOTOPMOST,
				clientRectStyleAdjusted.left, clientRectStyleAdjusted.top,
				clientRectStyleAdjusted.right - clientRectStyleAdjusted.left,
				clientRectStyleAdjusted.bottom - clientRectStyleAdjusted.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
		}
	}

	void set_window_size(WindowDesc* pWindow, uint32_t width, uint32_t height)
	{
		RectDescription newClientRect =
		{
			newClientRect.left = pWindow->windowedRect.left,
			newClientRect.top = pWindow->windowedRect.top,
			newClientRect.right = newClientRect.left + (int32_t)width,
			newClientRect.bottom = newClientRect.top + (int32_t)height
		};
		set_window_rect(pWindow, newClientRect);
	}

	void toggle_borderless_window(WindowDesc* pWindow, uint32_t width, uint32_t height)
	{
		if (!pWindow->fullScreen)
		{
			pWindow->borderlessWindow = !pWindow->borderlessWindow;

			bool centered = pWindow->centered;
			pWindow->centered = false;
			set_window_size(pWindow, width, height);
			pWindow->centered = centered;
		}
	};

	void toggle_fullscreen(WindowDesc* pWindow)
	{
		pWindow->fullScreen = !pWindow->fullScreen;
		adjust_window(pWindow);
	}

	void* create_cursor(const char* path)
	{
		return LoadCursorFromFileA(path);
	}

	void set_cursor(void* pCursor)
	{
		auto windowCursor = (HCURSOR)pCursor;
		SetCursor(windowCursor);
	}

	bool is_cursor_inside_tracking_area(RectDescription* pRect)
	{
		return gCursorInsideRectangle;
	}

	Vec2 get_mouse_pos_absolutely()
	{
		POINT cursorPos = {};
		GetCursorPos(&cursorPos);
		return { cursorPos.x, cursorPos.y };
	}

	Vec2 get_mouse_pos_relative(const WindowDesc* pWindow)
	{
		POINT cursorPos = {};
		GetCursorPos(&cursorPos);
		ScreenToClient((HWND)pWindow->handle.window, &cursorPos);
		return { cursorPos.x, cursorPos.y };
	}

	void set_mouse_position_relative(const WindowDesc* pWindow, int32_t x, int32_t y)
	{
		POINT p = { (LONG)x, (LONG)y };
		ClientToScreen((HWND)pWindow->handle.window, &p); // client window coordinate to screen coordinate
		SetCursorPos(p.x, p.y);
	}

	void set_mouse_position_absolutely(int32_t x, int32_t y)
	{
		SetCursorPos(x, y);
	}

	bool handle_messages()
	{
		MSG msg{};

		bool quit = false;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (WM_CLOSE == msg.message || WM_QUIT == msg.message)
				quit = true;
		}

		return quit;
	}

	void open_window(const char* appName, WindowDesc* pWindow)
	{
		update_window_fullscreen_rect(pWindow);

		// defer borderless window settings
		bool borderless = pWindow->borderlessWindow;
		pWindow->borderlessWindow = false;

		// adjust windowed rect for windowed mode rendering.
		RECT rect =
		{
			(LONG)pWindow->clientRect.left, // top-left x
			(LONG)pWindow->clientRect.top,  // top-left y
			(LONG)pWindow->clientRect.left + (LONG)pWindow->clientRect.right, // width
			(LONG)pWindow->clientRect.top + (LONG)pWindow->clientRect.bottom  // height
		};
		auto windowStyle = prepare_window_style_mask(pWindow);
		auto windowStyleEx = prepare_window_style_mask_ex(pWindow);

		AdjustWindowRectEx(&rect, windowStyle, FALSE, windowStyleEx);
		// convert char* to wchar_t*
		WCHAR app[SG_MAX_FILEPATH] = {};
		size_t charConverted = 0;
		mbstowcs_s(&charConverted, app, appName, SG_MAX_FILEPATH);

		int windowX = rect.left;
		int windowY = rect.top;

		if (!pWindow->overrideDefaultPosition)
		{
			windowX = CW_USEDEFAULT;
			windowY = CW_USEDEFAULT;
		}

		// defer fullscreen. we create in windowed and then switch to fullscreen.
		bool fullscreen = pWindow->fullScreen;
		pWindow->fullScreen = false;

		HWND hwnd = CreateWindowEx(windowStyleEx, SG_WINDOW_CLASS, app,
			windowStyle, windowX, windowY,
			rect.right - windowX, rect.bottom - windowY,
			NULL, NULL, (HINSTANCE)GetModuleHandle(NULL), 0);

		if (hwnd != NULL)
		{
			pWindow->handle.window = hwnd;
			GetClientRect(hwnd, &rect);
			pWindow->clientRect = { (int)rect.left, (int)rect.top, (int)rect.right, (int)rect.bottom };
			GetWindowRect(hwnd, &rect);
			pWindow->windowedRect = { (int)rect.left, (int)rect.top, (int)rect.right, (int)rect.bottom };

			if (!pWindow->hide)
			{
				if (pWindow->maximized)
					ShowWindow(hwnd, SW_MAXIMIZE);
				else if (pWindow->minimized)
					ShowWindow(hwnd, SW_MINIMIZE);

				if (borderless)
					toggle_borderless_window(pWindow, get_rect_width(pWindow->clientRect), get_rect_height(pWindow->clientRect));

				if (pWindow->centered)
					center_window(pWindow);

				if (fullscreen)
					toggle_fullscreen(pWindow);
			}

			// TODO: logging(window successfully created)
		}
		else
		{
			SG_LOG_ERROR("Failed to create window class!");
		}

		set_mouse_position_relative(pWindow,
			get_rect_width(pWindow->windowedRect) >> 1,
			get_rect_height(pWindow->windowedRect) >> 1); // certer of the client window
	}

	void close_window(const WindowDesc* pWindow)
	{
		DestroyWindow((HWND)pWindow->handle.window);
		handle_messages();
	}

	void show_window(WindowDesc* pWindow)
	{
		pWindow->hide = false;
		ShowWindow((HWND)pWindow->handle.window, SW_SHOW);
	}

	void hide_window(WindowDesc* pWindow)
	{
		pWindow->hide = true;
		ShowWindow((HWND)pWindow->handle.window, SW_HIDE);
	}

	void maximize_window(WindowDesc* pWindow)
	{
		pWindow->maximized = true;
		ShowWindow((HWND)pWindow->handle.window, SW_MAXIMIZE);
	}

	void minimize_window(WindowDesc* pWindow)
	{
		pWindow->minimized = true;
		ShowWindow((HWND)pWindow->handle.window, SW_MINIMIZE);
	}

	void collect_monitors_info()
	{
		if (gMonitors != nullptr)
			return;

		DISPLAY_DEVICEW adapter;
		adapter.cb = sizeof(adapter);

		int found = 0;
		int size = 0;
		uint32_t monitorCount = 0;

		for (int adapterIndex = 0;; ++adapterIndex)
		{
			if (!EnumDisplayDevicesW(NULL, adapterIndex, &adapter, 0))
				break;

			if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
				continue;


			for (int displayIndex = 0;; displayIndex++)
			{
				DISPLAY_DEVICEW display;
				display.cb = sizeof(display);

				if (!EnumDisplayDevicesW(adapter.DeviceName, displayIndex, &display, 0))
					break;

				++monitorCount;
			}
		}

		if (monitorCount)
		{
			gMonitorCount = monitorCount;
			gMonitors = (MonitorDescription*)sg_calloc(monitorCount, sizeof(MonitorDescription));
			for (int adapterIndex = 0;; ++adapterIndex)
			{
				if (!EnumDisplayDevicesW(NULL, adapterIndex, &adapter, 0))
					break;

				if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
					continue;

				for (int displayIndex = 0;; displayIndex++)
				{
					DISPLAY_DEVICEW display;
					HDC dc;

					display.cb = sizeof(display);

					if (!EnumDisplayDevicesW(adapter.DeviceName, displayIndex, &display, 0))
						break;

					dc = CreateDCW(L"DISPLAY", adapter.DeviceName, NULL, NULL);

					MonitorDescription desc;
					desc.modesPruned = (adapter.StateFlags & DISPLAY_DEVICE_MODESPRUNED) != 0;

					wcsncpy_s(desc.adapterName, adapter.DeviceName, elementsOf(adapter.DeviceName));
					wcsncpy_s(desc.publicAdapterName, adapter.DeviceString, elementsOf(adapter.DeviceString));
					wcsncpy_s(desc.displayName, display.DeviceName, elementsOf(display.DeviceName));
					wcsncpy_s(desc.publicDisplayName, display.DeviceString, elementsOf(display.DeviceString));

					desc.physicalSizeX = GetDeviceCaps(dc, HORZSIZE);
					desc.physicalSizeY = GetDeviceCaps(dc, VERTSIZE);

					const float dpi = 96.0f;
					desc.dpiX = static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSX) / dpi);
					desc.dpiY = static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSY) / dpi);

					gMonitors[found] = (desc);
					MonitorInfo data = {};
					data.index = found;
					wcsncpy_s(data.adapterName, adapter.DeviceName, elementsOf(adapter.DeviceName));

					EnumDisplayMonitors(NULL, NULL, monitor_callback, (LPARAM)(&data));

					DeleteDC(dc);

					if ((adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) && displayIndex == 0)
					{
						MonitorDescription desc = gMonitors[0];
						gMonitors[0] = gMonitors[found];
						gMonitors[found] = desc;
					}

					found++;
				}
			}
		}
		else
		{
			//LOGF(LogLevel::eDEBUG, "FallBack Option");
			// fallback options in case enumeration fails
			// then default to the primary device 
			monitorCount = 0;
			HMONITOR  currentMonitor = NULL;
			currentMonitor = MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
			if (currentMonitor)
			{
				monitorCount = 1;
				gMonitors = (MonitorDescription*)sg_calloc(monitorCount, sizeof(MonitorDescription));

				MONITORINFOEXW info;
				info.cbSize = sizeof(MONITORINFOEXW);
				bool infoRead = GetMonitorInfoW(currentMonitor, &info);
				MonitorDescription desc = {};

				wcsncpy_s(desc.adapterName, info.szDevice, elementsOf(info.szDevice));
				wcsncpy_s(desc.publicAdapterName, info.szDevice, elementsOf(info.szDevice));
				wcsncpy_s(desc.displayName, info.szDevice, elementsOf(info.szDevice));
				wcsncpy_s(desc.publicDisplayName, info.szDevice, elementsOf(info.szDevice));
				desc.monitorRect = { (int)info.rcMonitor.left, (int)info.rcMonitor.top, (int)info.rcMonitor.right, (int)info.rcMonitor.bottom };
				desc.workRect = { (int)info.rcWork.left, (int)info.rcWork.top, (int)info.rcWork.right, (int)info.rcWork.bottom };
				gMonitors[0] = (desc);
				gMonitorCount = monitorCount;
			}
		}


		for (uint32_t monitor = 0; monitor < monitorCount; ++monitor)
		{
			MonitorDescription* pMonitor = &gMonitors[monitor];
			DEVMODEW devMode = {};
			devMode.dmSize = sizeof(DEVMODEW);
			devMode.dmFields = DM_PELSHEIGHT | DM_PELSWIDTH;

			EnumDisplaySettingsW(pMonitor->adapterName, ENUM_CURRENT_SETTINGS, &devMode);
			pMonitor->defaultResolution.height = devMode.dmPelsHeight;
			pMonitor->defaultResolution.width = devMode.dmPelsWidth;

			eastl::vector<Resolution> displays;
			DWORD current = 0;
			while (EnumDisplaySettingsW(pMonitor->adapterName, current++, &devMode))
			{
				bool duplicate = false;
				for (uint32_t i = 0; i < (uint32_t)displays.size(); ++i)
				{
					if (displays[i].width == (uint32_t)devMode.dmPelsWidth && displays[i].height == (uint32_t)devMode.dmPelsHeight)
					{
						duplicate = true;
						break;
					}
				}

				if (duplicate)
					continue;

				Resolution videoMode = {};
				videoMode.height = devMode.dmPelsHeight;
				videoMode.width = devMode.dmPelsWidth;
				displays.emplace_back(videoMode);
			}
			qsort(displays.data(), displays.size(), sizeof(Resolution), [](const void* lhs, const void* rhs) {
				Resolution* pLhs = (Resolution*)lhs;
				Resolution* pRhs = (Resolution*)rhs;
				if (pLhs->height == pRhs->height)
					return (int)(pLhs->width - pRhs->width);

				return (int)(pLhs->height - pRhs->height);
				});

			pMonitor->resolutionCount = (uint32_t)displays.size();
			pMonitor->resolutions = (Resolution*)sg_calloc(pMonitor->resolutionCount, sizeof(Resolution));
			memcpy(pMonitor->resolutions, displays.data(), pMonitor->resolutionCount * sizeof(Resolution));
		}
	}

	void set_custom_message_processor(custom_message_processor pMsgProcessor)
	{
		sCustomMsgProc = pMsgProcessor;
	}

	void on_window_class_init()
	{
		collect_monitors_info();

		if (!gWindowClassInitialized)
		{
			HINSTANCE instance = (HINSTANCE)GetModuleHandleW(NULL);
			memset(&gWindowClassEx, 0, sizeof(gWindowClassEx));
			gWindowClassEx.style = 0;
			gWindowClassEx.lpfnWndProc = WinProc;
			gWindowClassEx.hInstance = instance;
			gWindowClassEx.hIcon = LoadIcon(NULL, IDI_APPLICATION);
			gWindowClassEx.hCursor = LoadCursor(NULL, IDC_ARROW);
			gWindowClassEx.lpszClassName = SG_WINDOW_CLASS;
			gWindowClassEx.cbSize = sizeof(WNDCLASSEX);

			bool success = RegisterClassEx(&gWindowClassEx) != 0;

			if (!success)
			{
				//Get the error message, if any.
				DWORD errorMessageID = ::GetLastError();

				if (errorMessageID != ERROR_CLASS_ALREADY_EXISTS)
				{
					LPSTR messageBuffer = NULL;
					size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
						FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorMessageID,
						MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
					eastl::string message(messageBuffer, size);
					SG_LOG_ERROR("%s", message.c_str());
					return;
				}
				else
				{
					gWindowClassInitialized = success;
				}
			}
		}
	}

	void on_window_class_exit()
	{
		for (uint32_t i = 0; i < gMonitorCount; ++i)
			sg_free(gMonitors[i].resolutions);

		sg_free(gMonitors);
	}

	void request_shutdown()
	{
		PostQuitMessage(0);
	}

	bool get_resolution_support(const MonitorDescription* pMonitor, const Resolution* pResolution)
	{
		for (uint32_t i = 0; i < pMonitor->resolutionCount; ++i)
		{
			if (pMonitor->resolutions[i].width == pResolution->width && pMonitor->resolutions[i].height == pResolution->height)
				return true;
		}
		return false;
	}

	Vec2 get_dpi_scale()
	{
		HDC hdc = ::GetDC(NULL); // get the device context to get the dpi info
		Vec2 ret = {};
		const float dpi = 96.0f;
		if (hdc)
		{
			ret.x = (UINT)(::GetDeviceCaps(hdc, LOGPIXELSX)) / dpi;
			ret.y = static_cast<UINT>(::GetDeviceCaps(hdc, LOGPIXELSY)) / dpi;
			::ReleaseDC(NULL, hdc);
		}
		else
		{
#if(WINVER >= 0x0605)
			float systemDpi = ::GetDpiForSystem() / 96.0f;
			ret = { systemDpi, systemDpi };
#else
			ret = { 1.0f, 1.0f };
#endif
		}

		return ret;
	}

}

	/////////////////////////////////////////////////////////////////////////
	/// custom entry point
	/////////////////////////////////////////////////////////////////////////

	int WindowUserMain(int argc, char** argv, SG::IApp* app)
	{
		using namespace SG;

		extern bool on_memory_init(const char* appName);
		extern void on_memory_exit();

		if (!on_memory_init(app->GetName()))
			return EXIT_FAILURE;

		FileSystemInitDescription fsInitDesc{};
		fsInitDesc.appName = app->GetName();

		if (!sgfs_init_file_system(&fsInitDesc))
			return EXIT_FAILURE;
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_DEBUG, SG_RD_LOG, "Log");

		// logging init
		Logger::OnInit(app->GetName());
		
		pApp = app;
		on_window_class_init();
		
		auto* pSettings = &pApp->mSettings;
		WindowDesc myWindow{};
		gWindow = &myWindow;
		pApp->mWindow = &myWindow;

		// if the monitor index isn't correct, then we use the default monitor
		if (pSettings->monitorIndex < 0 || pSettings->monitorIndex >= (int)gMonitorCount)
			pSettings->monitorIndex = 0;

		if (pSettings->width <= 0 || pSettings->height <= 0)
		{
			RectDescription rect = {};
			get_recommanded_resolution(&rect);
			pSettings->width = get_rect_width(rect);
			pSettings->height = get_rect_height(rect);
		}

		MonitorDescription* monitor = get_monitor(pSettings->monitorIndex);
		ASSERT(monitor != nullptr);

		gWindow->clientRect =
		{
			(int)pSettings->windowX + monitor->monitorRect.left,
			(int)pSettings->windowY + monitor->monitorRect.top,
			(int)pSettings->width,
			(int)pSettings->height
		};
		gWindow->windowedRect = gWindow->clientRect;
		gWindow->fullScreen = pSettings->fullScreen;
		gWindow->maximized = false;
		gWindow->noResizeFrame = !pSettings->enableResize;
		gWindow->borderlessWindow = pSettings->borderlessWindow;
		gWindow->centered = pSettings->centered;
		gWindow->forceLowDPI = pSettings->forceLowDPI;
		gWindow->overrideDefaultPosition = true;
		if (!pSettings->externalWindow)
			open_window(pApp->GetName(), pApp->mWindow);
		gWindow->handle = pApp->mWindow->handle;

		pSettings->width = pApp->mWindow->fullScreen ? get_rect_width(pApp->mWindow->fullscreenRect) : get_rect_width(pApp->mWindow->clientRect);
		pSettings->height = pApp->mWindow->fullScreen ? get_rect_height(pApp->mWindow->fullscreenRect) : get_rect_height(pApp->mWindow->clientRect);
		
		pApp->mCommandLine = GetCommandLineA();
		// app initialization
		{
			//Timer t("Init Timer");
			if (!pApp->OnInit())
				return EXIT_FAILURE;
			//SG_LOG_INFO("App initialization: (%fms)", t.GetTotalTime());

			pSettings->initialized = true;

			if (!pApp->OnLoad())
				return EXIT_FAILURE;
			//SG_LOG_INFO("App loading: (%fms)", t.GetTotalTime());
		}

		// main loop
		bool quit = false;
		Timer GlobalTimer("AppGlobalTimer");
		GlobalTimer.Reset();
		while (!quit)
		{
			GlobalTimer.Tick();

			quit = handle_messages() || pSettings->quit;

			// If window is minimized let other processes take over
			if (gWindow->minimized)
			{
				//GlobalTimer.Stop();
				Thread::sleep(1);
				continue;
			}

			pApp->OnUpdate(GlobalTimer.GetDeltaTime());
			pApp->OnDraw();

			// Graphics reset in cases where device has to be re-created.
			if (pApp->mSettings.resetGraphic)
			{
				GlobalTimer.Stop();
				pApp->OnUnload();
				pApp->OnLoad();
				pApp->mSettings.resetGraphic = false;
				GlobalTimer.Start();
			}
		}

		pApp->mSettings.quit = true;
		pApp->OnUnload();
		pApp->OnExit();

		GlobalTimer.Stop();

		on_window_class_exit();

		// log terminate
		Logger::OnExit();

		sgfs_exit_file_system();

		on_memory_exit();

		return EXIT_SUCCESS;
	}

#endif // #ifdef SG_PLATFORM_WINDOWS