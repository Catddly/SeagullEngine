#include "Interface/IInput.h"

#include <include/EASTL/hash_map.h>

#include "Interface/ILog.h"
#include "Interface/IMemory.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace SG
{

	eastl::hash_map<uint32_t, int> gSGCodeToWinKeycodeMap;
	eastl::hash_map<uint32_t, int> gSGCodeToWinMouseMap;

	DeviceId InputListener::mMouseDeviceId;
	DeviceId InputListener::mKeyboardDeviceId;

	InputListener* InputListener::sInstance = nullptr;

	bool InputListener::Init(WindowDesc* pWindow)
	{
		sInstance = sg_placement_new<InputListener>(sg_malloc(sizeof(InputListener)));

		sInstance->mWindow = pWindow;

		gSGCodeToWinKeycodeMap[SG_KEY_A] = 65;
		gSGCodeToWinKeycodeMap[SG_KEY_B] = 66;
		gSGCodeToWinKeycodeMap[SG_KEY_C] = 67;
		gSGCodeToWinKeycodeMap[SG_KEY_D] = 68;
		gSGCodeToWinKeycodeMap[SG_KEY_E] = 69;
		gSGCodeToWinKeycodeMap[SG_KEY_F] = 70;
		gSGCodeToWinKeycodeMap[SG_KEY_G] = 71;
		gSGCodeToWinKeycodeMap[SG_KEY_H] = 72;
		gSGCodeToWinKeycodeMap[SG_KEY_I] = 73;
		gSGCodeToWinKeycodeMap[SG_KEY_J] = 74;
		gSGCodeToWinKeycodeMap[SG_KEY_K] = 75;
		gSGCodeToWinKeycodeMap[SG_KEY_L] = 76;
		gSGCodeToWinKeycodeMap[SG_KEY_M] = 77;
		gSGCodeToWinKeycodeMap[SG_KEY_N] = 78;
		gSGCodeToWinKeycodeMap[SG_KEY_O] = 79;
		gSGCodeToWinKeycodeMap[SG_KEY_P] = 80;
		gSGCodeToWinKeycodeMap[SG_KEY_Q] = 81;
		gSGCodeToWinKeycodeMap[SG_KEY_R] = 82;
		gSGCodeToWinKeycodeMap[SG_KEY_S] = 83;
		gSGCodeToWinKeycodeMap[SG_KEY_T] = 84;
		gSGCodeToWinKeycodeMap[SG_KEY_U] = 85;
		gSGCodeToWinKeycodeMap[SG_KEY_V] = 86;
		gSGCodeToWinKeycodeMap[SG_KEY_W] = 87;
		gSGCodeToWinKeycodeMap[SG_KEY_X] = 88;
		gSGCodeToWinKeycodeMap[SG_KEY_Y] = 89;
		gSGCodeToWinKeycodeMap[SG_KEY_Z] = 90;

		gSGCodeToWinKeycodeMap[SG_KEY_0] = 48;
		gSGCodeToWinKeycodeMap[SG_KEY_1] = 49;
		gSGCodeToWinKeycodeMap[SG_KEY_2] = 50;
		gSGCodeToWinKeycodeMap[SG_KEY_3] = 51;
		gSGCodeToWinKeycodeMap[SG_KEY_4] = 52;
		gSGCodeToWinKeycodeMap[SG_KEY_5] = 53;
		gSGCodeToWinKeycodeMap[SG_KEY_6] = 54;
		gSGCodeToWinKeycodeMap[SG_KEY_7] = 55;
		gSGCodeToWinKeycodeMap[SG_KEY_8] = 56;
		gSGCodeToWinKeycodeMap[SG_KEY_9] = 57;

		gSGCodeToWinKeycodeMap[SG_KEY_F1] = 112;
		gSGCodeToWinKeycodeMap[SG_KEY_F2] = 113;
		gSGCodeToWinKeycodeMap[SG_KEY_F3] = 114;
		gSGCodeToWinKeycodeMap[SG_KEY_F4] = 115;
		gSGCodeToWinKeycodeMap[SG_KEY_F5] = 116;
		gSGCodeToWinKeycodeMap[SG_KEY_F6] = 117;
		gSGCodeToWinKeycodeMap[SG_KEY_F7] = 118;
		gSGCodeToWinKeycodeMap[SG_KEY_F8] = 119;
		gSGCodeToWinKeycodeMap[SG_KEY_F9] = 120;
		gSGCodeToWinKeycodeMap[SG_KEY_F10] = 121;
		gSGCodeToWinKeycodeMap[SG_KEY_F11] = 122;
		gSGCodeToWinKeycodeMap[SG_KEY_F12] = 123;

		gSGCodeToWinKeycodeMap[SG_KEY_ESCAPE] = 27;

		gSGCodeToWinMouseMap[SG_MOUSE_LEFT] = VK_LBUTTON;
		gSGCodeToWinMouseMap[SG_MOUSE_RIGHT] = VK_RBUTTON;

		return true;
	}

	void InputListener::Exit()
	{
		sg_delete(sInstance);
	}

	bool InputListener::IsKeyPressed(KeyBindings binding)
	{
		auto state = GetKeyState(gSGCodeToWinKeycodeMap[binding]);
		state = state & (1 << 15);
		return bool(state != 0);
	}

	bool InputListener::IsKeyRelease(KeyBindings binding)
	{
		return true;
	}

	bool InputListener::IsMousePressed(MouseBindings binding)
	{
		auto state = GetKeyState(gSGCodeToWinMouseMap[binding]);
		state = state & (1 << 15);
		return bool(state != 0);
	}

	bool InputListener::IsMouseRelease(MouseBindings binding)
	{
		return true;
	}

	eastl::pair<float, float> InputListener::GetMousePosClient()
	{
		POINT point = { 0, 0 };
		auto wnd = static_cast<HWND>(sInstance->mWindow->handle.window);
		::GetCursorPos(&point);
		::ScreenToClient(wnd, &point);
		return { (float)point.x, (float)point.y };
	}

}