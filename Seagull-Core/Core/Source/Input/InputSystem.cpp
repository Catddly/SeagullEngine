#include <include/EASTL/unordered_map.h>
#include <include/EASTL/unordered_set.h>
#include <include/EASTL/array.h>

#include <include/gainput/gainput.h>
#include <include/gainput/GainputInputListener.h>

#include <include/gainput/GainputInputDeviceMouse.h>
#include <include/gainput/GainputInputDeviceKeyboard.h>

#include "Interface/ILog.h"
#include "Interface/IInput.h"

#ifdef SG_PLATFORM_WINDOWS
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
		#include <windows.h>
#endif

#include "Interface/IMemory.h"

#ifdef GAINPUT_PLATFORM_GGP
namespace gainput
{
	extern void SetWindow(void* pData);
}
#endif

#define SG_MAX_DEVICES 16U

namespace SG
{

	uint32_t MAX_INPUT_ACTIONS = 128;

	struct InputSystemImpl : public gainput::InputListener
	{
		enum InputControlType
		{
			SG_CONTROL_BUTTON = 0,
			SG_CONTROL_FLOAT,
			SG_CONTROL_AXIS,
			SG_CONTROL_VIRTUAL_JOYSTICK,
			SG_CONTROL_COMPOSITE,
			SG_CONTROL_COMBO,
		};

		enum InputAreaType
		{
			SG_AREA_LEFT = 0,
			SG_AREA_RIGHT,
		};

		struct IControl
		{
			InputAction*     pAction;
			InputControlType type;
		};

		struct CompositeControl : public IControl
		{
			CompositeControl(const eastl::array<uint32_t, 4>& controls, uint8_t composite)
			{
				memset(this, 0, sizeof(*this));
				composite = composite;
				memcpy(this->controls, controls.data(), sizeof(controls));
				type = SG_CONTROL_COMPOSITE;
			}
			Vec2            value;
			uint32_t        controls[4];
			uint8_t         composite;
			uint8_t         started;
			uint8_t         performed[4];
			uint8_t         pressedVal[4];
		};

		struct FloatControl : public IControl
		{
			FloatControl(uint16_t start, uint8_t target, bool raw, bool delta)
			{
				memset(this, 0, sizeof(*this));
				startButton = start;
				target = target;
				type = SG_CONTROL_FLOAT;
				delta = (1 << (uint8_t)raw) | (uint8_t)delta;
			}
			Vec3          value;
			uint16_t      startButton;
			uint8_t       target;
			uint8_t       started;
			uint8_t       performed;
			uint8_t       delta;
		};

		struct AxisControl : public IControl
		{
			AxisControl(uint16_t start, uint8_t target, uint8_t axis)
			{
				memset(this, 0, sizeof(*this));
				startButton = start;
				target = target;
				axisCount = axis;
				type = SG_CONTROL_AXIS;
			}
			Vec3            value;
			Vec3            newValue;
			uint16_t        startButton;
			uint8_t         target;
			uint8_t         axisCount;
			uint8_t         started;
			uint8_t         performed;
		};

		//struct VirtualJoystickControl : public IControl
		//{
		//	Vec2     StartPos;
		//	Vec2     CurrPos;
		//	float    OutsideRadius;
		//	float    Deadzone;
		//	float    Scale;
		//	uint8_t  TouchIndex;
		//	uint8_t  Started;
		//	uint8_t  Performed;
		//	uint8_t  Area;
		//	uint8_t  IsPressed;
		//	uint8_t  Initialized;
		//	uint8_t  Active;
		//};

		struct ComboControl : public IControl
		{
			uint16_t pressButton;
			uint16_t triggerButton;
			uint8_t  pressed;
		};

		const eastl::unordered_map<uint32_t, gainput::MouseButton> mMouseMap =
		{
			{ SG_MOUSE_LEFT, gainput::MouseButtonLeft },
			{ SG_MOUSE_RIGHT, gainput::MouseButtonRight },
			{ SG_MOUSE_MIDDLE, gainput::MouseButtonMiddle },
			{ SG_MOUSE_SCROLL_UP, gainput::MouseButtonWheelUp },
			{ SG_MOUSE_SCROLL_DOWN, gainput::MouseButtonWheelDown },
			{ SG_MOUSE_5, gainput::MouseButton5},
			{ SG_MOUSE_6, gainput::MouseButton6},
		};

		const eastl::unordered_map<uint32_t, gainput::Key> mKeyMap =
		{
			{ SG_KEY_ESCAPE, gainput::KeyEscape},
			{ SG_KEY_F1, gainput::KeyF1},
			{ SG_KEY_F2, gainput::KeyF2},
			{ SG_KEY_F3, gainput::KeyF3},
			{ SG_KEY_F4, gainput::KeyF4},
			{ SG_KEY_F5, gainput::KeyF5},
			{ SG_KEY_F6, gainput::KeyF6},
			{ SG_KEY_F7, gainput::KeyF7},
			{ SG_KEY_F8, gainput::KeyF8},
			{ SG_KEY_F9, gainput::KeyF9},
			{ SG_KEY_F10, gainput::KeyF10},
			{ SG_KEY_F11, gainput::KeyF11},
			{ SG_KEY_F12, gainput::KeyF12},
			{ SG_KEY_F13, gainput::KeyF13},
			{ SG_KEY_F14, gainput::KeyF14},
			{ SG_KEY_F15, gainput::KeyF15},
			{ SG_KEY_F16, gainput::KeyF16},
			{ SG_KEY_F17, gainput::KeyF17},
			{ SG_KEY_F18, gainput::KeyF18},
			{ SG_KEY_F19, gainput::KeyF19},
			{ SG_KEY_PRINT, gainput::KeyPrint},
			{ SG_KEY_SCROLLLOCK, gainput::KeyScrollLock},
			{ SG_KEY_BREAK, gainput::KeyBreak},
			{ SG_KEY_SPACE, gainput::KeySpace},
			{ SG_KEY_APOSTROPHE, gainput::KeyApostrophe},
			{ SG_KEY_COMMA, gainput::KeyComma},
			{ SG_KEY_MINUS, gainput::KeyMinus},
			{ SG_KEY_PERIOD, gainput::KeyPeriod},
			{ SG_KEY_SLASH, gainput::KeySlash},
			{ SG_KEY_0, gainput::Key0},
			{ SG_KEY_1, gainput::Key1},
			{ SG_KEY_2, gainput::Key2},
			{ SG_KEY_3, gainput::Key3},
			{ SG_KEY_4, gainput::Key4},
			{ SG_KEY_5, gainput::Key5},
			{ SG_KEY_6, gainput::Key6},
			{ SG_KEY_7, gainput::Key7},
			{ SG_KEY_8, gainput::Key8},
			{ SG_KEY_9, gainput::Key9},
			{ SG_KEY_SEMICOLON, gainput::KeySemicolon},
			{ SG_KEY_LESS, gainput::KeyLess},
			{ SG_KEY_EQUAL, gainput::KeyEqual},
			{ SG_KEY_A, gainput::KeyA},
			{ SG_KEY_B, gainput::KeyB},
			{ SG_KEY_C, gainput::KeyC},
			{ SG_KEY_D, gainput::KeyD},
			{ SG_KEY_E, gainput::KeyE},
			{ SG_KEY_F, gainput::KeyF},
			{ SG_KEY_G, gainput::KeyG},
			{ SG_KEY_H, gainput::KeyH},
			{ SG_KEY_I, gainput::KeyI},
			{ SG_KEY_J, gainput::KeyJ},
			{ SG_KEY_K, gainput::KeyK},
			{ SG_KEY_L, gainput::KeyL},
			{ SG_KEY_M, gainput::KeyM},
			{ SG_KEY_N, gainput::KeyN},
			{ SG_KEY_O, gainput::KeyO},
			{ SG_KEY_P, gainput::KeyP},
			{ SG_KEY_Q, gainput::KeyQ},
			{ SG_KEY_R, gainput::KeyR},
			{ SG_KEY_S, gainput::KeyS},
			{ SG_KEY_T, gainput::KeyT},
			{ SG_KEY_U, gainput::KeyU},
			{ SG_KEY_V, gainput::KeyV},
			{ SG_KEY_W, gainput::KeyW},
			{ SG_KEY_X, gainput::KeyX},
			{ SG_KEY_Y, gainput::KeyY},
			{ SG_KEY_Z, gainput::KeyZ},
			{ SG_KEY_BRACKETLEFT, gainput::KeyBracketLeft},
			{ SG_KEY_BACKSLASH, gainput::KeyBackslash},
			{ SG_KEY_BRACKETRIGHT, gainput::KeyBracketRight},
			{ SG_KEY_GRAVE, gainput::KeyGrave},
			{ SG_KEY_LEFT, gainput::KeyLeft},
			{ SG_KEY_RIGHT, gainput::KeyRight},
			{ SG_KEY_UP, gainput::KeyUp},
			{ SG_KEY_DOWN, gainput::KeyDown},
			{ SG_KEY_INSERT, gainput::KeyInsert},
			{ SG_KEY_HOME, gainput::KeyHome},
			{ SG_KEY_DELETE, gainput::KeyDelete},
			{ SG_KEY_END, gainput::KeyEnd},
			{ SG_KEY_PAGEUP, gainput::KeyPageUp},
			{ SG_KEY_PAGEDOWN, gainput::KeyPageDown},
			{ SG_KEY_NUMLOCK, gainput::KeyNumLock},
			{ SG_KEY_KPEQUAL, gainput::KeyKpEqual},
			{ SG_KEY_KPDIVIDE, gainput::KeyKpDivide},
			{ SG_KEY_KPMULTIPLY, gainput::KeyKpMultiply},
			{ SG_KEY_KPSUBTRACT, gainput::KeyKpSubtract},
			{ SG_KEY_KPADD, gainput::KeyKpAdd},
			{ SG_KEY_KPENTER, gainput::KeyKpEnter},
			{ SG_KEY_KPINSERT, gainput::KeyKpInsert},
			{ SG_KEY_KPEND, gainput::KeyKpEnd},
			{ SG_KEY_KPDOWN, gainput::KeyKpDown},
			{ SG_KEY_KPPAGEDOWN, gainput::KeyKpPageDown},
			{ SG_KEY_KPLEFT, gainput::KeyKpLeft},
			{ SG_KEY_KPBEGIN, gainput::KeyKpBegin},
			{ SG_KEY_KPRIGHT, gainput::KeyKpRight},
			{ SG_KEY_KPHOME, gainput::KeyKpHome},
			{ SG_KEY_KPUP, gainput::KeyKpUp},
			{ SG_KEY_KPPAGEUP, gainput::KeyKpPageUp},
			{ SG_KEY_KPDELETE, gainput::KeyKpDelete},
			{ SG_KEY_BACKSPACE, gainput::KeyBackSpace},
			{ SG_KEY_TAB, gainput::KeyTab},
			{ SG_KEY_RETURN, gainput::KeyReturn},
			{ SG_KEY_CAPSLOCK, gainput::KeyCapsLock},
			{ SG_KEY_SHIFTL, gainput::KeyShiftL},
			{ SG_KEY_CTRLL, gainput::KeyCtrlL},
			{ SG_KEY_SUPERL, gainput::KeySuperL},
			{ SG_KEY_ALTL, gainput::KeyAltL},
			{ SG_KEY_ALTR, gainput::KeyAltR},
			{ SG_KEY_SUPERR, gainput::KeySuperR},
			{ SG_KEY_MENU, gainput::KeyMenu},
			{ SG_KEY_CTRLR, gainput::KeyCtrlR},
			{ SG_KEY_SHIFTR, gainput::KeyShiftR},
			{ SG_KEY_BACK, gainput::KeyBack},
			{ SG_KEY_SOFTLEFT, gainput::KeySoftLeft},
			{ SG_KEY_SOFTRIGHT, gainput::KeySoftRight},
			{ SG_KEY_CALL, gainput::KeyCall},
			{ SG_KEY_ENDCALL, gainput::KeyEndcall},
			{ SG_KEY_STAR, gainput::KeyStar},
			{ SG_KEY_POUND, gainput::KeyPound},
			{ SG_KEY_DPADCENTER, gainput::KeyDpadCenter},
			{ SG_KEY_VOLUMEUP, gainput::KeyVolumeUp},
			{ SG_KEY_VOLUMEDOWN, gainput::KeyVolumeDown},
			{ SG_KEY_POWER, gainput::KeyPower},
			{ SG_KEY_CAMERA, gainput::KeyCamera},
			{ SG_KEY_CLEAR, gainput::KeyClear},
			{ SG_KEY_SYMBOL, gainput::KeySymbol},
			{ SG_KEY_EXPLORER, gainput::KeyExplorer},
			{ SG_KEY_ENVELOPE, gainput::KeyEnvelope},
			{ SG_KEY_EQUALS, gainput::KeyEquals},
			{ SG_KEY_AT, gainput::KeyAt},
			{ SG_KEY_HEADSETHOOK, gainput::KeyHeadsethook},
			{ SG_KEY_FOCUS, gainput::KeyFocus},
			{ SG_KEY_PLUS, gainput::KeyPlus},
			{ SG_KEY_NOTIFICATION, gainput::KeyNotification},
			{ SG_KEY_SEARCH, gainput::KeySearch},
			{ SG_KEY_MEDIAPLAYPAUSE, gainput::KeyMediaPlayPause},
			{ SG_KEY_MEDIASTOP, gainput::KeyMediaStop},
			{ SG_KEY_MEDIANEXT, gainput::KeyMediaNext},
			{ SG_KEY_MEDIAPREVIOUS, gainput::KeyMediaPrevious},
			{ SG_KEY_MEDIAREWIND, gainput::KeyMediaRewind},
			{ SG_KEY_MEDIAFASTFORWARD, gainput::KeyMediaFastForward},
			{ SG_KEY_MUTE, gainput::KeyMute},
			{ SG_KEY_PICTSYMBOLS, gainput::KeyPictsymbols},
			{ SG_KEY_SWITCHCHARSET, gainput::KeySwitchCharset},
			{ SG_KEY_FORWARD, gainput::KeyForward},
			{ SG_KEY_EXTRA1, gainput::KeyExtra1},
			{ SG_KEY_EXTRA2, gainput::KeyExtra2},
			{ SG_KEY_EXTRA3, gainput::KeyExtra3},
			{ SG_KEY_EXTRA4, gainput::KeyExtra4},
			{ SG_KEY_EXTRA5, gainput::KeyExtra5},
			{ SG_KEY_EXTRA6, gainput::KeyExtra6},
			{ SG_KEY_FN, gainput::KeyFn},
			{ SG_KEY_CIRCUMFLEX, gainput::KeyCircumflex},
			{ SG_KEY_SSHARP, gainput::KeySsharp},
			{ SG_KEY_ACUTE, gainput::KeyAcute},
			{ SG_KEY_ALTGR, gainput::KeyAltGr},
			{ SG_KEY_NUMBERSIGN, gainput::KeyNumbersign},
			{ SG_KEY_UDIAERESIS, gainput::KeyUdiaeresis},
			{ SG_KEY_ADIAERESIS, gainput::KeyAdiaeresis},
			{ SG_KEY_ODIAERESIS, gainput::KeyOdiaeresis},
			{ SG_KEY_SECTION, gainput::KeySection},
			{ SG_KEY_ARING, gainput::KeyAring},
			{ SG_KEY_DIAERESIS, gainput::KeyDiaeresis},
			{ SG_KEY_TWOSUPERIOR, gainput::KeyTwosuperior},
			{ SG_KEY_RIGHTPARENTHESIS, gainput::KeyRightParenthesis},
			{ SG_KEY_DOLLAR, gainput::KeyDollar},
			{ SG_KEY_UGRAVE, gainput::KeyUgrave},
			{ SG_KEY_ASTERISK, gainput::KeyAsterisk},
			{ SG_KEY_COLON, gainput::KeyColon},
			{ SG_KEY_EXCLAM, gainput::KeyExclam},
			{ SG_KEY_BRACELEFT, gainput::KeyBraceLeft},
			{ SG_KEY_BRACERIGHT, gainput::KeyBraceRight},
			{ SG_KEY_SYSRQ, gainput::KeySysRq},
		};

		/// Maps the gainput button to the Binding enum
		eastl::vector<uint32_t>                  controlMapReverse[SG_MAX_DEVICES];
		/// List of all input controls per device
		eastl::vector<eastl::vector<IControl*> > controls[SG_MAX_DEVICES];
		/// List of gestures
		eastl::vector<InputAction*>              gestureControls;
		gainput::InputMap*						 gestureInputMap;

		/// List of all text input actions
		/// These actions will be invoked everytime there is a text character typed on a physical / virtual keyboard
		eastl::vector<InputAction*>              textInputControls;
		/// List of controls which need to be canceled at the end of the frame
		eastl::unordered_set<FloatControl*>      floatDeltaControlCancelQueue;
		eastl::unordered_set<IControl*>          buttonControlPerformQueue;

		eastl::vector<InputAction>               actions;
		eastl::vector<IControl*>                 controlPool;
#if TOUCH_INPUT
		float2                                   mTouchPositions[gainput::TouchCount_ >> 2];
#else
		Vec2                                     mousePosition;
#endif

		/// Window pointer passed by the app
		/// Input capture will be performed on this window
		WindowDesc* pWindow = nullptr;

		/// Gainput Manager which lets us talk with the gainput backend
		gainput::InputManager* pInputManager = nullptr;

		InputDeviceType*                         pDeviceTypes;
		gainput::DeviceId                        mouseDeviceID;
		gainput::DeviceId                        rawMouseDeviceID;
		gainput::DeviceId                        keyboardDeviceID;

		bool                                     inputCaptured;
		bool                                     defaultCapture;

		bool OnInit(WindowDesc* window)
		{
			pWindow = window;

#ifdef GAINPUT_PLATFORM_GGP
			gainput::SetWindow(pWindow->handle.window);
#endif

			// Defaults
			defaultCapture = true;
			inputCaptured = false;

			pDeviceTypes = (InputDeviceType*)sg_calloc(4 + 3, sizeof(InputDeviceType));

			// Default device ids
			mouseDeviceID = gainput::InvalidDeviceId;
			rawMouseDeviceID = gainput::InvalidDeviceId;
			keyboardDeviceID = gainput::InvalidDeviceId;

			// create input manager
			pInputManager = sg_new(gainput::InputManager);
			ASSERT(pInputManager);

#if defined(SG_PLATFORM_WINDOWS) || defined(XBOX)
			pInputManager->SetWindowsInstance(window->handle.window);
#endif

			// create all necessary devices
			mouseDeviceID = pInputManager->CreateDevice<gainput::InputDeviceMouse>();
			rawMouseDeviceID = pInputManager->CreateDevice<gainput::InputDeviceMouse>(gainput::InputDevice::AutoIndex, gainput::InputDeviceMouse::DV_RAW);
			keyboardDeviceID = pInputManager->CreateDevice<gainput::InputDeviceKeyboard>();

			// Assign device types
			pDeviceTypes[mouseDeviceID] = SG_INPUT_DEVICE_MOUSE;
			pDeviceTypes[rawMouseDeviceID] = SG_INPUT_DEVICE_MOUSE;
			pDeviceTypes[keyboardDeviceID] = SG_INPUT_DEVICE_KEYBOARD;

			// Create control maps
			controls[keyboardDeviceID].resize(gainput::KeyCount_);
			controls[mouseDeviceID].resize(gainput::MouseButtonCount_);
			controls[rawMouseDeviceID].resize(gainput::MouseButtonCount_);

			controlMapReverse[mouseDeviceID] = eastl::vector<uint32_t>(gainput::MouseButtonCount, UINT_MAX);
			controlMapReverse[keyboardDeviceID] = eastl::vector<uint32_t>(gainput::KeyCount_, UINT_MAX);

			for (decltype(mMouseMap)::const_iterator it = mMouseMap.begin(); it != mMouseMap.end(); ++it)
				controlMapReverse[mouseDeviceID][it->second] = it->first;

			for (decltype(mKeyMap)::const_iterator it = mKeyMap.begin(); it != mKeyMap.end(); ++it)
				controlMapReverse[keyboardDeviceID][it->second] = it->first;

			actions.reserve(MAX_INPUT_ACTIONS);

			pInputManager->AddListener(this);

			gestureInputMap = sg_new(gainput::InputMap, *pInputManager, "GestureMap");

			return InitSubView();
		}

		void OnExit()
		{
			sg_delete(gestureInputMap);

			ASSERT(pInputManager);

			for (uint32_t i = 0; i < (uint32_t)controlPool.size(); ++i)
				sg_free(controlPool[i]);

			sg_free(pDeviceTypes);

			ShutdownSubView();
			sg_delete(pInputManager);
		}

		void OnUpdate(uint32_t width, uint32_t height)
		{
			ASSERT(pInputManager);

			for (FloatControl* pControl : floatDeltaControlCancelQueue)
			{
				pControl->started = 0;
				pControl->performed = 0;
				pControl->value = Vec3(0.0f);

				if (pControl->pAction->desc.callback)
				{
					InputActionContext ctx = {};
					ctx.pUserData = pControl->pAction->desc.pUserData;
					ctx.phase = INPUT_ACTION_PHASE_CANCELED;
					ctx.pCaptured = &defaultCapture;
					ctx.pPosition = &mousePosition;

					pControl->pAction->desc.callback(&ctx);
				}
			}
//#if TOUCH_INPUT
//			for (IControl* pControl : buttonControlPerformQueue)
//			{
//				InputActionContext ctx = {};
//				ctx.pUserData = pControl->pAction->desc.pUserData;
//				ctx.phase = INPUT_ACTION_PHASE_PERFORMED;
//				ctx.pCaptured = &defaultCapture;
//				ctx.phase = INPUT_ACTION_PHASE_PERFORMED;
//				ctx.bindings = SG_BUTTON_SOUTH;
//				ctx.pPosition = &mTouchPositions[pControl->pAction->desc.mUserId];
//				ctx.mBool = true;
//				pControl->pAction->desc.callback(&ctx);
//			}
//#endif
			buttonControlPerformQueue.clear();
			floatDeltaControlCancelQueue.clear();

			gainput::InputDeviceKeyboard* keyboard = (gainput::InputDeviceKeyboard*)pInputManager->GetDevice(keyboardDeviceID);
			if (keyboard)
			{
				uint32_t count = 0;
				wchar_t* pText = keyboard->GetTextInput(&count);
				if (count)
				{
					InputActionContext ctx = {};
					ctx.text = pText;
					ctx.phase = INPUT_ACTION_PHASE_PERFORMED;
					for (InputAction* pAction : textInputControls)
					{
						ctx.pUserData = pAction->desc.pUserData;
						if (!pAction->desc.callback(&ctx))
							break;
					}
				}
			}

			// update gainput manager
			pInputManager->SetDisplaySize(width, height);
			pInputManager->Update();

			// update all the long hold gesture
			for (auto* pAction : gestureControls)
			{
				GestureDesc* pGesture = &pAction->desc.pGesture;

				InputActionContext ctx = {};
				//if (gestureInputMap->GetBool(pGesture->triggerBinding))
				//SG_LOG_DEBUG("Trigger: %d", gestureInputMap->GetBool(pGesture->triggerBinding));

				if (gestureInputMap->GetBool(pGesture->triggerBinding))
				{
					uint32_t deviceId = pGesture->triggerBinding >= SG_KEY_COUNT ? mouseDeviceID : keyboardDeviceID;

					ctx.deviceType = pDeviceTypes[deviceId];
					ctx.pCaptured = IsPointerType(deviceId) ? &inputCaptured : &defaultCapture;

					if (IsPointerType(deviceId))
					{
						gainput::InputDeviceMouse* pMouse = (gainput::InputDeviceMouse*)pInputManager->GetDevice(mouseDeviceID);
						mousePosition[0] = pMouse->GetFloat(gainput::MouseAxisX);
						mousePosition[1] = pMouse->GetFloat(gainput::MouseAxisY);
						ctx.pPosition = &mousePosition;
						ctx.scrollValue = pMouse->GetFloat(gainput::MouseButtonMiddle);
					}

					ctx.pUserData = pAction->desc.pUserData;
					ctx.bindings = pGesture->triggerBinding;
					ctx.isPressed = true;

					ctx.phase = INPUT_ACTION_PHASE_PERFORMED;
					pAction->desc.callback(&ctx);
				}
				//else
				//{
				//	uint32_t deviceId = pGesture->triggerBinding >= SG_KEY_COUNT ? mouseDeviceID : keyboardDeviceID;

				//	ctx.deviceType = pDeviceTypes[deviceId];
				//	ctx.pCaptured = IsPointerType(deviceId) ? &inputCaptured : &defaultCapture;

				//	if (IsPointerType(deviceId))
				//	{
				//		gainput::InputDeviceMouse* pMouse = (gainput::InputDeviceMouse*)pInputManager->GetDevice(mouseDeviceID);
				//		mousePosition[0] = pMouse->GetFloat(gainput::MouseAxisX);
				//		mousePosition[1] = pMouse->GetFloat(gainput::MouseAxisY);
				//		ctx.pPosition = &mousePosition;
				//		ctx.scrollValue = pMouse->GetFloat(gainput::MouseButtonMiddle);
				//	}

				//	ctx.pUserData = pAction->desc.pUserData;
				//	ctx.bindings = pGesture->triggerBinding;
				//	ctx.isPressed = false;

				//	ctx.phase = INPUT_ACTION_PHASE_CANCELED;
				//}
			}

#if defined(__linux__) && !defined(__ANDROID__) && !defined(GAINPUT_PLATFORM_GGP)
			//this needs to be done before updating the events
			//that way current frame data will be delta after resetting mouse position
			if (inputCaptured)
			{
				ASSERT(pWindow);

				float x = 0;
				float y = 0;
				x = (pWindow->windowedRect.right - pWindow->windowedRect.left) / 2;
				y = (pWindow->windowedRect.bottom - pWindow->windowedRect.top) / 2;
				XWarpPointer(pWindow->handle.display, None, pWindow->handle.window, 0, 0, 0, 0, x, y);
				gainput::InputDevice* device = pInputManager->GetDevice(rawMouseDeviceID);
				device->WarpMouse(x, y);
				XFlush(pWindow->handle.display);
			}
#endif
		}

		template <typename T>
		T* AllocateControl()
		{
			T* pControl = (T*)sg_calloc(1, sizeof(T));
			controlPool.emplace_back(pControl);
			return pControl;
		}

		InputAction* AddInputAction(const InputActionDesc* pDesc)
		{
			ASSERT(pDesc);

			actions.emplace_back(InputAction{});
			InputAction* pAction = &actions.back();
			ASSERT(pAction);

			pAction->desc = *pDesc;
#if defined(TARGET_IOS)
			if (pGainputView && GESTURE_BINDINGS_BEGIN <= pDesc->bindings && GESTURE_BINDINGS_END >= pDesc->bindings)
			{
				const GestureDesc* pGesture = pDesc->pGesture;
				ASSERT(pGesture);

				GainputView* view = (__bridge GainputView*)pGainputView;
				uint32_t gestureId = (uint32_t)gestureControls.size();
				gainput::GestureConfig gestureConfig = {};
				gestureConfig.mType = (gainput::GestureType)(pDesc->bindings - GESTURE_BINDINGS_BEGIN);
				gestureConfig.mMaxNumberOfTouches = pGesture->mMaxNumberOfTouches;
				gestureConfig.mMinNumberOfTouches = pGesture->mMinNumberOfTouches;
				gestureConfig.mMinimumPressDuration = pGesture->mMinimumPressDuration;
				gestureConfig.mNumberOfTapsRequired = pGesture->mNumberOfTapsRequired;
				[view addGestureMapping : gestureId withConfig : gestureConfig] ;
				gestureControls.emplace_back(pAction);
				return pAction;
			}
#endif

			if (pDesc->bindings == SG_TEXT)
			{
				ASSERT(pDesc->callback);
				textInputControls.emplace_back(pAction);
				return pAction;
			}

			const uint32_t control = pDesc->bindings;

			if (SG_BUTTON_ANY == control)
			{
				IControl* pControl = AllocateControl<IControl>();
				ASSERT(pControl);

				pControl->type = SG_CONTROL_BUTTON;
				pControl->pAction = pAction;

				for (decltype(mKeyMap)::const_iterator it = mKeyMap.begin(); it != mKeyMap.end(); ++it)
					controls[keyboardDeviceID][it->second].emplace_back(pControl);

				for (decltype(mMouseMap)::const_iterator it = mMouseMap.begin(); it != mMouseMap.end(); ++it)
					controls[mouseDeviceID][it->second].emplace_back(pControl);

				return pAction;
			}
			else if (SG_KEY_FULLSCREEN == control)
			{
				ComboControl* pControl = AllocateControl<ComboControl>();
				ASSERT(pControl);

				pControl->type = SG_CONTROL_COMBO;
				pControl->pAction = pAction;
				pControl->pressButton = gainput::KeyAltL;
				pControl->triggerButton = gainput::KeyReturn;
				controls[keyboardDeviceID][gainput::KeyReturn].emplace_back(pControl);
				controls[keyboardDeviceID][gainput::KeyAltL].emplace_back(pControl);
			}
			else if (SG_KEY_DUMP == control)
			{
				//ComboControl* pGamePadControl = AllocateControl<ComboControl>();
				//ASSERT(pGamePadControl);

				//pGamePadControl->type = SG_CONTROL_COMBO;
				//pGamePadControl->pAction = pAction;
				//pGamePadControl->pressButton = gainput::PadButtonStart;
				//pGamePadControl->triggerButton = gainput::PadButtonB;
				//controls[gamepadDeviceId][pGamePadControl->mTriggerButton].emplace_back(pGamePadControl);
				//controls[gamepadDeviceId][pGamePadControl->mPressButton].emplace_back(pGamePadControl);

				ComboControl* pControl = AllocateControl<ComboControl>();
				ASSERT(pControl);
				pControl->type = SG_CONTROL_BUTTON;
				pControl->pAction = pAction;
				decltype(mKeyMap)::const_iterator keyIt = mKeyMap.find(control);
				if (keyIt != mKeyMap.end())
					controls[keyboardDeviceID][keyIt->second].emplace_back(pControl);
			}
			else if (0 <= control && SG_BUTTON_ANY >= control)
			{
				IControl* pControl = AllocateControl<IControl>();
				ASSERT(pControl);

				pControl->type = SG_CONTROL_BUTTON;
				pControl->pAction = pAction;

				//decltype(mGamepadMap)::const_iterator gamepadIt = mGamepadMap.find(control);
				//if (gamepadIt != mGamepadMap.end())
				//	controls[gamepadDeviceId][gamepadIt->second].emplace_back(pControl);

				decltype(mKeyMap)::const_iterator keyIt = mKeyMap.find(control);
				if (keyIt != mKeyMap.end())
					controls[keyboardDeviceID][keyIt->second].emplace_back(pControl);

				decltype(mMouseMap)::const_iterator mouseIt = mMouseMap.find(control);
				if (mouseIt != mMouseMap.end())
					controls[mouseDeviceID][mouseIt->second].emplace_back(pControl);
			}
			/*else if (FLOAT_BINDINGS_BEGIN <= control && FLOAT_BINDINGS_END >= control)
			{
				if (FLOAT_DPAD == control)
				{
					CompositeControl* pControl = AllocateControl<CompositeControl>();
					ASSERT(pControl);

					pControl->mType = CONTROL_COMPOSITE;
					pControl->pAction = pAction;
					pControl->composite = 4;
					pControl->mControls[0] = gainput::PadButtonRight;
					pControl->mControls[1] = gainput::PadButtonLeft;
					pControl->mControls[2] = gainput::PadButtonUp;
					pControl->mControls[3] = gainput::PadButtonDown;
					for (uint32_t i = 0; i < pControl->composite; ++i)
						controls[gamepadDeviceId][pControl->mControls[i]].emplace_back(pControl);
				}
				else
				{
					uint32_t axisCount = 0;
					decltype(mGamepadAxisMap)::const_iterator gamepadIt = mGamepadAxisMap.find(control);
					ASSERT(gamepadIt != mGamepadAxisMap.end());
					if (gamepadIt != mGamepadAxisMap.end())
					{
						AxisControl* pControl = AllocateControl<AxisControl>();
						ASSERT(pControl);

						*pControl = gamepadIt->second;
						pControl->pAction = pAction;
						for (uint32_t i = 0; i < pControl->mAxisCount; ++i)
							controls[gamepadDeviceId][pControl->mStartButton + i].emplace_back(pControl);

						axisCount = pControl->mAxisCount;
					}
#if TOUCH_INPUT
					if ((FLOAT_LEFTSTICK == control || FLOAT_RIGHTSTICK == control) && (pDesc->mOutsideRadius && pDesc->mScale))
					{
						VirtualJoystickControl* pControl = AllocateControl<VirtualJoystickControl>();
						ASSERT(pControl);

						pControl->mType = CONTROL_VIRTUAL_JOYSTICK;
						pControl->pAction = pAction;
						pControl->mOutsideRadius = pDesc->mOutsideRadius;
						pControl->mDeadzone = pDesc->mDeadzone;
						pControl->mScale = pDesc->mScale;
						pControl->mTouchIndex = 0xFF;
						pControl->mArea = FLOAT_LEFTSTICK == control ? AREA_LEFT : AREA_RIGHT;
						controls[mTouchDeviceID][gainput::Touch0Down].emplace_back(pControl);
						controls[mTouchDeviceID][gainput::Touch0X].emplace_back(pControl);
						controls[mTouchDeviceID][gainput::Touch0Y].emplace_back(pControl);
						controls[mTouchDeviceID][gainput::Touch1Down].emplace_back(pControl);
						controls[mTouchDeviceID][gainput::Touch1X].emplace_back(pControl);
						controls[mTouchDeviceID][gainput::Touch1Y].emplace_back(pControl);
					}
#else

#ifndef NO_DEFAULT_BINDINGS
					decltype(mGamepadCompositeMap)::const_iterator keyIt = mGamepadCompositeMap.find(control);
					if (keyIt != mGamepadCompositeMap.end())
					{
						CompositeControl* pControl = AllocateControl<CompositeControl>();
						ASSERT(pControl);

						*pControl = keyIt->second;
						pControl->pAction = pAction;
						for (uint32_t i = 0; i < pControl->composite; ++i)
							controls[keyboardDeviceID][pControl->mControls[i]].emplace_back(pControl);
					}
#endif

					decltype(mGamepadFloatMap)::const_iterator floatIt = mGamepadFloatMap.find(control);
					if (floatIt != mGamepadFloatMap.end())
					{
						FloatControl* pControl = AllocateControl<FloatControl>();
						ASSERT(pControl);

						*pControl = floatIt->second;
						pControl->pAction = pAction;

						gainput::DeviceId deviceId = ((pControl->mDelta >> 1) & 0x1) ? rawMouseDeviceID : mouseDeviceID;

						for (uint32_t i = 0; i < axisCount; ++i)
							controls[deviceId][pControl->mStartButton + i].emplace_back(pControl);
					}
#endif
				}
			}*/

			// long pressed gesture
			if (control == SG_GESTURE_LONG_PRESS)
			{
				IControl* pControl = AllocateControl<IControl>();
				ASSERT(pControl);

				pControl->type = SG_CONTROL_BUTTON;
				pControl->pAction = pAction;

				decltype(mMouseMap)::const_iterator mouseIt = mMouseMap.find(pDesc->pGesture.triggerBinding);
				if (mouseIt != mMouseMap.end())
				{
					gestureControls.emplace_back(pAction);

					gainput::HoldGesture* hg = pInputManager->CreateAndGetDevice<gainput::HoldGesture>();
					ASSERT(hg);

					//SG_LOG_DEBUG("%d", mouseIt->second);
					hg->Initialize(mouseDeviceID, mouseIt->second,
						mouseDeviceID, gainput::MouseAxisX, 1.f,
						mouseDeviceID, gainput::MouseAxisY, 1.f,
						false, pDesc->pGesture.minimumPressDurationMs);

					gestureInputMap->MapBool(pDesc->pGesture.triggerBinding, hg->GetDeviceId(), gainput::HoldTriggered);
				}

				decltype(mKeyMap)::const_iterator keyIt = mKeyMap.find(pDesc->pGesture.triggerBinding);
				if (keyIt != mKeyMap.end())
				{
					gestureControls.emplace_back(pAction);

					gainput::HoldGesture* hg = pInputManager->CreateAndGetDevice<gainput::HoldGesture>();
					ASSERT(hg);

					//SG_LOG_DEBUG("%d", mouseIt->second);
					hg->Initialize(keyboardDeviceID, keyIt->second,
						mouseDeviceID, gainput::MouseAxisX, 1.f,
						mouseDeviceID, gainput::MouseAxisY, 1.f,
						false, pDesc->pGesture.minimumPressDurationMs);

					gestureInputMap->MapBool(pDesc->pGesture.triggerBinding, hg->GetDeviceId(), gainput::HoldTriggered);
				}
			}

			return pAction;
		}

		void RemoveInputAction(InputAction* pAction)
		{
			ASSERT(pAction);

			decltype(gestureControls)::const_iterator it = eastl::find(gestureControls.begin(), gestureControls.end(), pAction);
			if (it != gestureControls.end())
				gestureControls.erase(it);

			sg_free(pAction);
		}

		bool InitSubView()
		{
#ifdef __APPLE__
			if (pWindow)
			{
				void* view = pWindow->handle.window;
				if (!view)
					return false;

#ifdef TARGET_IOS
				UIView* mainView = (UIView*)CFBridgingRelease(view);
				GainputView* newView = [[GainputView alloc]initWithFrame:mainView.bounds inputManager : *pInputManager];
				//we want everything to resize with main view.
				[newView setAutoresizingMask : (UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleTopMargin |
					UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin |
					UIViewAutoresizingFlexibleBottomMargin)] ;
#else
				NSView* mainView = (__bridge NSView*)view;
				float retinScale = ((CAMetalLayer*)(mainView.layer)).drawableSize.width / mainView.frame.size.width;
				GainputMacInputView* newView = [[GainputMacInputView alloc]initWithFrame:mainView.bounds
					window : mainView.window
					retinaScale : retinScale
					inputManager : *pInputManager];
				newView.nextKeyView = mainView;
				[newView setAutoresizingMask : NSViewWidthSizable | NSViewHeightSizable] ;
#endif

				[mainView addSubview : newView];

#ifdef TARGET_IOS
#else
				NSWindow* window = [newView window];
				BOOL madeFirstResponder = [window makeFirstResponder : newView];
				if (!madeFirstResponder)
					return false;
#endif

				pGainputView = (__bridge void*)newView;
			}
#endif

			return true;
		}

		void ShutdownSubView()
		{
#ifdef __APPLE__
			if (!pGainputView)
				return;

			//automatic reference counting
			//it will get deallocated.
			if (pGainputView)
			{
#ifndef TARGET_IOS
				GainputMacInputView* view = (GainputMacInputView*)CFBridgingRelease(pGainputView);
#else
				GainputView* view = (GainputView*)CFBridgingRelease(pGainputView);
#endif
				[view removeFromSuperview];
				pGainputView = NULL;
			}
#endif
		}

		bool SetEnableCaptureInput(bool enable)
		{
			ASSERT(pWindow);

#if defined(SG_PLATFORM_WINDOWS) && !defined(XBOX)
			//static int32_t lastCursorPosX = 0;
			//static int32_t lastCursorPosY = 0;

			if (enable != inputCaptured)
			{
				if (enable)
				{
					//POINT lastCursorPoint;
					//GetCursorPos(&lastCursorPoint);
					//lastCursorPosX = lastCursorPoint.x;
					//lastCursorPosY = lastCursorPoint.y;

					HWND handle = (HWND)pWindow->handle.window;
					SetCapture(handle);

					RECT clientRect;
					GetClientRect(handle, &clientRect);
					//convert screen rect to client coordinates.
					POINT ptClientUL = { clientRect.left, clientRect.top };
					// Add one to the right and bottom sides, because the
					// coordinates retrieved by GetClientRect do not
					// include the far left and lowermost pixels.
					POINT ptClientLR = { clientRect.right + 1, clientRect.bottom + 1 };
					ClientToScreen(handle, &ptClientUL);
					ClientToScreen(handle, &ptClientLR);

					// Copy the client coordinates of the client area
					// to the rcClient structure. Confine the mouse cursor
					// to the client area by passing the rcClient structure
					// to the ClipCursor function.
					SetRect(&clientRect, ptClientUL.x, ptClientUL.y, ptClientLR.x, ptClientLR.y);
					ClipCursor(&clientRect);
					ShowCursor(FALSE);
				}
				else
				{
					ClipCursor(NULL);
					ShowCursor(TRUE);
					ReleaseCapture();
					//SetCursorPos(lastCursorPosX, lastCursorPosY);
				}

				inputCaptured = enable;
				return true;
			}
#elif defined(__APPLE__)
			if (mInputCaptured != enable)
			{
#if !defined(TARGET_IOS)
				if (enable)
				{
					CGDisplayHideCursor(kCGDirectMainDisplay);
					CGAssociateMouseAndMouseCursorPosition(false);
				}
				else
				{
					CGDisplayShowCursor(kCGDirectMainDisplay);
					CGAssociateMouseAndMouseCursorPosition(true);
				}
#endif
				mInputCaptured = enable;

				return true;
			}
#elif defined(__linux__) && !defined(__ANDROID__) && !defined(GAINPUT_PLATFORM_GGP)
			if (mInputCaptured != enable)
			{
				if (enable)
				{
					// Create invisible cursor that will be used when mouse is captured
					Cursor      invisibleCursor = {};
					Pixmap      bitmapEmpty = {};
					XColor      emptyColor = {};
					static char emptyData[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
					emptyColor.red = emptyColor.green = emptyColor.blue = 0;
					bitmapEmpty = XCreateBitmapFromData(pWindow->handle.display, pWindow->handle.window, emptyData, 8, 8);
					invisibleCursor = XCreatePixmapCursor(pWindow->handle.display, bitmapEmpty, bitmapEmpty, &emptyColor, &emptyColor, 0, 0);
					// Capture mouse
					unsigned int masks = PointerMotionMask |    //Mouse movement
						ButtonPressMask |      //Mouse click
						ButtonReleaseMask;     // Mouse release
					int XRes = XGrabPointer(
						pWindow->handle.display, pWindow->handle.window, 1 /*reports with respect to the grab window*/, masks, GrabModeAsync, GrabModeAsync, None,
						invisibleCursor, CurrentTime);
				}
				else
				{
					XUngrabPointer(pWindow->handle.display, CurrentTime);
				}

				mInputCaptured = enable;

				return true;
			}
#elif defined(__ANDROID__)
			if (mInputCaptured != enable)
			{
				mInputCaptured = enable;
				return true;
			}
#endif

			return false;
		}

		bool SetEnableCaptureInputCustom(bool enable, const Vec4& windowRectRelative)
		{
			ASSERT(pWindow);

#if defined(SG_PLATFORM_WINDOWS) && !defined(XBOX)
			if (enable != inputCaptured)
			{
				if (enable)
				{
					HWND handle = (HWND)pWindow->handle.window;
					SetCapture(handle);

					RECT clientRect;
					GetClientRect(handle, &clientRect);

					POINT tl = { (LONG)windowRectRelative.x, (LONG)windowRectRelative.y };
					POINT br = { (LONG)windowRectRelative.z, (LONG)windowRectRelative.w };
					ClientToScreen(handle, &tl);
					ClientToScreen(handle, &br);

					SetRect(&clientRect, tl.x, tl.y, br.x, br.y);
					ClipCursor(&clientRect);
					ShowCursor(FALSE);
				}
				else
				{
					HWND handle = (HWND)pWindow->handle.window;
					RECT clientRect;
					GetClientRect(handle, &clientRect);

					POINT ptClientUL = { clientRect.left, clientRect.top };
					POINT ptClientLR = { clientRect.right + 1, clientRect.bottom + 1 };
					ClientToScreen(handle, &ptClientUL);
					ClientToScreen(handle, &ptClientLR);

					SetRect(&clientRect, ptClientUL.x, ptClientUL.y, ptClientLR.x, ptClientLR.y);

					ClipCursor(NULL);
					ShowCursor(TRUE);
					ReleaseCapture();
				}

				inputCaptured = enable;
				return true;
			}
#endif
			return false;
		}

		void SetVirtualKeyboard(uint32_t type)
		{
#ifdef TARGET_IOS
			if (!pGainputView)
				return;

			if ((type > 0) != mVirtualKeyboardActive)
				mVirtualKeyboardActive = (type > 0);
			else
				return;

			GainputView* view = (__bridge GainputView*)(pGainputView);
			[view setVirtualKeyboard : type] ;
#elif defined(__ANDROID__)		
			if ((type > 0) != mVirtualKeyboardActive)
			{
				mVirtualKeyboardActive = (type > 0);

				/* Note: native activity's API for soft input (ANativeActivity_showSoftInput & ANativeActivity_hideSoftInput) do not work.
				 *       So we do it manually using JNI.
				 */

				ANativeActivity* activity = pWindow->handle.activity;
				JNIEnv* jni;
				jint result = activity->vm->AttachCurrentThread(&jni, NULL);
				if (result == JNI_ERR)
				{
					ASSERT(0);
					return;
				}

				jclass cls = jni->GetObjectClass(activity->clazz);
				jmethodID methodID = jni->GetMethodID(cls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
				jstring serviceName = jni->NewStringUTF("input_method");
				jobject inputService = jni->CallObjectMethod(activity->clazz, methodID, serviceName);

				jclass inputServiceCls = jni->GetObjectClass(inputService);
				methodID = jni->GetMethodID(inputServiceCls, "toggleSoftInput", "(II)V");
				jni->CallVoidMethod(inputService, methodID, 0, 0);

				jni->DeleteLocalRef(serviceName);
				activity->vm->DetachCurrentThread();

			}
			else
				return;
#endif
		}

		inline constexpr bool IsPointerType(gainput::DeviceId device) const
		{
#if TOUCH_INPUT
			return false;
#else
			return (device == mouseDeviceID || device == rawMouseDeviceID);
#endif
		}

		bool OnDeviceButtonBool(gainput::DeviceId device, gainput::DeviceButtonId deviceButton, bool oldValue, bool newValue) override
		{
			if (oldValue == newValue)
				return false;

			if (controls[device].size())
			{
				InputActionContext ctx = {};
				ctx.deviceType = pDeviceTypes[device];
				ctx.pCaptured = IsPointerType(device) ? &inputCaptured : &defaultCapture;
#if TOUCH_INPUT
				uint32_t touchIndex = 0;
				if (device == mTouchDeviceID)
				{
					touchIndex = TOUCH_USER(deviceButton);
					gainput::InputDeviceTouch* pTouch = (gainput::InputDeviceTouch*)pInputManager->GetDevice(mTouchDeviceID);
					mTouchPositions[touchIndex][0] = pTouch->GetFloat(TOUCH_X(touchIndex));
					mTouchPositions[touchIndex][1] = pTouch->GetFloat(TOUCH_Y(touchIndex));
					ctx.pPosition = &mTouchPositions[touchIndex];
				}
#else
				if (IsPointerType(device))
				{
					gainput::InputDeviceMouse* pMouse = (gainput::InputDeviceMouse*)pInputManager->GetDevice(mouseDeviceID);
					mousePosition[0] = pMouse->GetFloat(gainput::MouseAxisX);
					mousePosition[1] = pMouse->GetFloat(gainput::MouseAxisY);
					ctx.pPosition = &mousePosition;
					ctx.scrollValue = pMouse->GetFloat(gainput::MouseButtonMiddle);
				}
#endif
				bool executeNext = true;

				const eastl::vector<IControl*>& controls = this->controls[device][deviceButton];
				for (IControl* control : controls)
				{
					if (!executeNext)
						return true;

					const InputControlType type = control->type;
					const InputActionDesc* pDesc = &control->pAction->desc;
					ctx.pUserData = pDesc->pUserData;
					ctx.bindings = controlMapReverse[device][deviceButton];

					switch (type)
					{
					case SG_CONTROL_BUTTON:
					{
						if (pDesc->callback)
						{
							ctx.isPressed = newValue;
							if (newValue && !oldValue)
							{
								ctx.phase = INPUT_ACTION_PHASE_STARTED;
								executeNext = pDesc->callback(&ctx) && executeNext;
#if TOUCH_INPUT
								mButtonControlPerformQueue.insert(control);
#else
								ctx.phase = INPUT_ACTION_PHASE_PERFORMED;
								executeNext = pDesc->callback(&ctx) && executeNext;
#endif
							}
							else if (oldValue && !newValue)
							{
								ctx.phase = INPUT_ACTION_PHASE_CANCELED;
								executeNext = pDesc->callback(&ctx) && executeNext;
							}
						}
						break;
					}
					case SG_CONTROL_COMPOSITE:
					{
						CompositeControl* pControl = (CompositeControl*)control;
						uint32_t index = 0;
						for (; index < pControl->composite; ++index)
							if (deviceButton == pControl->controls[index])
								break;

						const uint32_t axis = (index > 1);
						if (newValue)
						{
							pControl->pressedVal[index] = 1;
							pControl->value[axis] = (float)pControl->pressedVal[axis * 2 + 0] - (float)pControl->pressedVal[axis * 2 + 1];
						}

						if (pControl->composite == 2)
						{
							ctx.value = pControl->value[axis];
						}
						else
						{
							if (!pControl->value[0] && !pControl->value[1])
								ctx.value2 = Vec2(0.0f);
							else
								ctx.value2 = pControl->value;
						}

						// Action Started
						if (!pControl->started && !oldValue && newValue)
						{
							pControl->started = 1;
							ctx.phase = INPUT_ACTION_PHASE_STARTED;
							if (pDesc->callback)
								executeNext = pDesc->callback(&ctx) && executeNext;
						}
						// Action Performed
						if (pControl->started && newValue && !pControl->performed[index])
						{
							pControl->performed[index] = 1;
							ctx.phase = INPUT_ACTION_PHASE_PERFORMED;
							if (pDesc->callback)
								executeNext = pDesc->callback(&ctx) && executeNext;
						}
						// Action Canceled
						if (oldValue && !newValue)
						{
							pControl->performed[index] = 0;
							pControl->pressedVal[index] = 0;
							bool allReleased = true;
							for (uint8_t i = 0; i < pControl->composite; ++i)
							{
								if (pControl->performed[i])
								{
									allReleased = false;
									break;
								}
							}
							if (allReleased)
							{
								pControl->value = Vec2(0.0f);
								pControl->started = 0;
								ctx.value2 = pControl->value;
								ctx.phase = INPUT_ACTION_PHASE_CANCELED;
								if (pDesc->callback)
									executeNext = pDesc->callback(&ctx) && executeNext;
							}
							else if (pDesc->callback)
							{
								ctx.phase = INPUT_ACTION_PHASE_PERFORMED;
								pControl->value[axis] = (float)pControl->pressedVal[axis * 2 + 0] - (float)pControl->pressedVal[axis * 2 + 1];
								ctx.value2 = pControl->value;
								executeNext = pDesc->callback(&ctx) && executeNext;
							}
						}

						break;
					}
					// Mouse scroll is using OnDeviceButtonBool
					case SG_CONTROL_FLOAT:
					{
						if (!oldValue && newValue)
						{
							ASSERT(deviceButton == gainput::MouseButtonWheelUp || deviceButton == gainput::MouseButtonWheelDown);

							FloatControl* pControl = (FloatControl*)control;
							ctx.value2[1] = deviceButton == gainput::MouseButtonWheelUp ? 1.0f : -1.0f;
							if (pDesc->callback)
							{
								ctx.phase = INPUT_ACTION_PHASE_PERFORMED;
								executeNext = pDesc->callback(&ctx) && executeNext;
							}

							floatDeltaControlCancelQueue.insert(pControl);
						}
					}
#if TOUCH_INPUT
					case CONTROL_VIRTUAL_JOYSTICK:
					{
						VirtualJoystickControl* pControl = (VirtualJoystickControl*)control;

						if (!oldValue && newValue && !pControl->started)
						{
							pControl->mStartPos = mTouchPositions[touchIndex];
							if ((AREA_LEFT == pControl->mArea && pControl->mStartPos[0] <= pInputManager->GetDisplayWidth() * 0.5f) ||
								(AREA_RIGHT == pControl->mArea && pControl->mStartPos[0] > pInputManager->GetDisplayWidth() * 0.5f))
							{
								pControl->started = 0x3;
								pControl->mTouchIndex = touchIndex;
								pControl->mCurrPos = pControl->mStartPos;

								if (pDesc->callback)
								{
									ctx.phase = INPUT_ACTION_PHASE_STARTED;
									ctx.value2 = float2(0.0f);
									ctx.pPosition = &pControl->mCurrPos;
									executeNext = pDesc->callback(&ctx) && executeNext;
								}
							}
							else
							{
								pControl->started = 0;
								pControl->mTouchIndex = 0xFF;
							}
						}
						else if (oldValue && !newValue)
						{
							if (pControl->mTouchIndex == touchIndex)
							{
								pControl->mIsPressed = 0;
								pControl->mTouchIndex = 0xFF;
								pControl->started = 0;
								pControl->performed = 0;
								if (pDesc->callback)
								{
									ctx.value2 = float2(0.0f);
									ctx.phase = INPUT_ACTION_PHASE_CANCELED;
									executeNext = pDesc->callback(&ctx) && executeNext;
								}
							}
						}
						break;
					}
#endif
					case SG_CONTROL_COMBO:
					{
						ComboControl* pControl = (ComboControl*)control;
						if (deviceButton == pControl->pressButton)
						{
							pControl->pressed = (uint8_t)newValue;
						}
						else if (pControl->pressed && oldValue && !newValue && pDesc->callback)
						{
							ctx.isPressed = true;
							ctx.phase = INPUT_ACTION_PHASE_PERFORMED;
							pDesc->callback(&ctx);
						}
						break;
					}
					default:
						break;
					}
				}
			}

			return true;
		}

		bool OnDeviceButtonFloat(gainput::DeviceId device, gainput::DeviceButtonId deviceButton, float oldValue, float newValue) override
		{
#if TOUCH_INPUT
			if (mTouchDeviceID == device)
			{
				const uint32_t touchIndex = TOUCH_USER(deviceButton);
				gainput::InputDeviceTouch* pTouch = (gainput::InputDeviceTouch*)pInputManager->GetDevice(mTouchDeviceID);
				mTouchPositions[touchIndex][0] = pTouch->GetFloat(TOUCH_X(touchIndex));
				mTouchPositions[touchIndex][1] = pTouch->GetFloat(TOUCH_Y(touchIndex));
			}
#else
			if (mouseDeviceID == device)
			{
				gainput::InputDeviceMouse* pMouse = (gainput::InputDeviceMouse*)pInputManager->GetDevice(mouseDeviceID);
				mousePosition[0] = pMouse->GetFloat(gainput::MouseAxisX);
				mousePosition[1] = pMouse->GetFloat(gainput::MouseAxisY);
			}
#endif

			if (this->controls[device].size())
			{
				bool executeNext = true;

				const eastl::vector<IControl*>& controls = this->controls[device][deviceButton];
				for (IControl* control : controls)
				{
					if (!executeNext)
						return true;

					const InputControlType type = control->type;
					const InputActionDesc* pDesc = &control->pAction->desc;
					InputActionContext ctx = {};
					ctx.deviceType = pDeviceTypes[device];
					ctx.pUserData = pDesc->pUserData;
					ctx.pCaptured = IsPointerType(device) ? &inputCaptured : &defaultCapture;

					switch (type)
					{
					case SG_CONTROL_FLOAT:
					{
						FloatControl* pControl = (FloatControl*)control;
						const InputActionDesc* pDesc = &pControl->pAction->desc;
						const uint32_t axis = (deviceButton - pControl->startButton);

						if (pControl->delta & 0x1)
						{
							pControl->value[axis] += (axis > 0 ? -1.0f : 1.0f) * (newValue - oldValue);
							ctx.value3 = pControl->value;

							if (((pControl->started >> axis) & 0x1) == 0)
							{
								pControl->started |= (1 << axis);
								if (pControl->started == pControl->target && pDesc->callback)
								{
									ctx.phase = INPUT_ACTION_PHASE_STARTED;
									executeNext = pDesc->callback(&ctx) && executeNext;
								}

								floatDeltaControlCancelQueue.insert(pControl);
							}

							pControl->performed |= (1 << axis);

							if (pControl->performed == pControl->target)
							{
								pControl->performed = 0;
								ctx.phase = INPUT_ACTION_PHASE_PERFORMED;
								if (pDesc->callback)
									executeNext = pDesc->callback(&ctx) && executeNext;
							}
						}
						else if (pDesc->callback)
						{
							pControl->performed |= (1 << axis);
							pControl->value[axis] = newValue;
							if (pControl->performed == pControl->target)
							{
								ctx.phase = INPUT_ACTION_PHASE_PERFORMED;
								pControl->performed = 0;
								ctx.value3 = pControl->value;
								executeNext = pDesc->callback(&ctx) && executeNext;
							}
						}
						break;
					}
					case SG_CONTROL_AXIS:
					{
						AxisControl* pControl = (AxisControl*)control;
						const InputActionDesc* pDesc = &pControl->pAction->desc;

						const uint32_t axis = (deviceButton - pControl->startButton);

						pControl->newValue[axis] = newValue;
						bool equal = true;
						for (uint32_t i = 0; i < pControl->axisCount; ++i)
							equal = equal && (pControl->value[i] == pControl->newValue[i]);

						pControl->performed |= (1 << axis);
						pControl->value[axis] = pControl->newValue[axis];

						if (pControl->performed == pControl->target && pDesc->callback)
						{
							ctx.phase = INPUT_ACTION_PHASE_PERFORMED;
							ctx.value3 = pControl->value;
							executeNext = pDesc->callback(&ctx) && executeNext;
						}

						if (pControl->performed != pControl->target)
							continue;

						pControl->performed = 0;
						break;
					}
#if TOUCH_INPUT
					case CONTROL_VIRTUAL_JOYSTICK:
					{
						VirtualJoystickControl* pControl = (VirtualJoystickControl*)control;

						const uint32_t axis = TOUCH_AXIS(deviceButton);

						if (!pControl->started || TOUCH_USER(deviceButton) != pControl->mTouchIndex)
							continue;

						pControl->performed |= (1 << axis);
						pControl->mCurrPos[axis] = newValue;
						if (pControl->performed == 0x3)
						{
							// Calculate the new joystick positions
							vec2 delta = f2Tov2(pControl->mCurrPos - pControl->mStartPos);
							float halfRad = (pControl->mOutsideRadius * 0.5f) - pControl->mDeadzone;
							if (length(delta) > halfRad)
								pControl->mCurrPos = pControl->mStartPos + halfRad * v2ToF2(normalize(delta));

							ctx.phase = INPUT_ACTION_PHASE_PERFORMED;
							float2 dir = ((pControl->mCurrPos - pControl->mStartPos) / halfRad) * pControl->mScale;
							ctx.value2 = float2(dir[0], -dir[1]);
							ctx.pPosition = &pControl->mCurrPos;
							if (pDesc->callback)
								executeNext = pDesc->callback(&ctx) && executeNext;
						}
						break;
					}
#endif
					default:
						break;
					}
				}
			}

			return true;
		}

		int  GetPriority() const
		{
			return 0;
		}

	};

	static InputSystemImpl* pInputSystem = nullptr;

#if defined(SG_PLATFORM_WINDOWS) && !defined(XBOX)
	static void ResetInputStates()
	{
		pInputSystem->pInputManager->ClearAllStates(pInputSystem->mouseDeviceID);
		pInputSystem->pInputManager->ClearAllStates(pInputSystem->keyboardDeviceID);
	}
#endif

	int32_t InputSystemHandleMessage(WindowDesc* pWindow, void* msg)
	{
		if (pInputSystem == nullptr)
		{
			return 0;
		}
#if defined(SG_PLATFORM_WINDOWS) && !defined(XBOX)
		pInputSystem->pInputManager->HandleMessage(*(MSG*)msg);
		if ((*(MSG*)msg).message == WM_ACTIVATEAPP && (*(MSG*)msg).wParam == WA_INACTIVE)
		{
			ResetInputStates();
		}
#elif defined(__ANDROID__)
		return pInputSystem->pInputManager->HandleInput((AInputEvent*)msg, pWindow->handle.activity);
#elif defined(__linux__) && !defined(GAINPUT_PLATFORM_GGP)
		pInputSystem->pInputManager->HandleEvent(*(XEvent*)msg);
#endif

		return 0;
	}

	bool init_input_system(WindowDesc* pWindow)
	{
		pInputSystem = sg_new(InputSystemImpl);

		set_custom_message_processor(InputSystemHandleMessage);

		return pInputSystem->OnInit(pWindow);
	}

	void exit_input_system()
	{
		ASSERT(pInputSystem);

		set_custom_message_processor(nullptr);

		pInputSystem->OnExit();
		sg_delete(pInputSystem);
		pInputSystem = nullptr;
	}

	void update_input_system(uint32_t width, uint32_t height)
	{
		ASSERT(pInputSystem);

		pInputSystem->OnUpdate(width, height);
	}

	InputAction* add_input_action(const InputActionDesc* pDesc)
	{
		ASSERT(pInputSystem);

		return pInputSystem->AddInputAction(pDesc);
	}

	void remove_input_action(InputAction* pAction)
	{
		ASSERT(pInputSystem);

		pInputSystem->RemoveInputAction(pAction);
	}

	bool set_enable_capture_input(bool enable)
	{
		ASSERT(pInputSystem);

		return pInputSystem->SetEnableCaptureInput(enable);
	}

	bool set_enable_capture_input_custom(bool enable, const Vec4& windowRectRelative)
	{
		ASSERT(pInputSystem);

		return pInputSystem->SetEnableCaptureInputCustom(enable, windowRectRelative);
	}

}