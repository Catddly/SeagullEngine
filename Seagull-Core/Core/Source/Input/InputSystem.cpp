#include "InputListener.h"
#include "InputManager.h"

#include <include/EASTL/array.h>
#include <include/EASTL/unordered_map.h>
#include <include/EASTL/unordered_set.h>

#include "Interface/ILog.h"
#include "Interface/IOperatingSystem.h"
#include "Interface/IFileSystem.h"
#include "Interface/IInput.h"
#include "Interface/IMemory.h"

//#ifdef GAINPUT_PLATFORM_GGP
//namespace gainput
//{
//	extern void SetWindow(void* pData);
//}
//#endif

#define SG_MAX_INPUT_DEVICES 16U

namespace SG
{
	//uint32_t MAX_INPUT_GAMEPADS = 4;
	//uint32_t MAX_INPUT_MULTI_TOUCHES = 4;
	uint32_t MAX_INPUT_ACTIONS = 128;

	enum ButtonBindings
	{
		SG_BUTTON_ANY = SG_MOUSE_AXIS_Y + SG_BUTTON_KEY_COUNT
	};

	struct InputAction
	{
		InputActionDesc desc;
	};

	// we create a listener, listen for the input message
	struct InputSystemImpl : public InputListener
	{
		enum InputControlType
		{
			SG_CONTROL_TYPE_BUTTON = 0,
			SG_CONTROL_TYPE_FLOAT,
			SG_CONTROL_TYPE_AXIS,
			SG_CONTROL_TYPE_VIRTUAL_JOYSTICK,
			SG_CONTROL_TYPE_COMPOSITE,
			SG_CONTROL_TYPE_COMBO,
		};

		enum InputAreaType
		{
			SG_AREA_LEFT = 0,
			SG_AREA_RIGHT,
		};

		struct IControl
		{
			InputAction* pAction;
			InputControlType type;
		};

		struct CompositeControl : public IControl
		{
			CompositeControl(const eastl::array<uint32_t, 4>& controls, uint8_t composite)
			{
				memset(this, 0, sizeof(*this));
				this->composite = composite;
				memcpy(this->controls, controls.data(), sizeof(mControls));
				type = SG_CONTROL_TYPE_COMPOSITE;
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
				this->target = target;
				type = SG_CONTROL_TYPE_FLOAT;
				delta = (1 << (uint8_t)raw) | (uint8_t)delta;
			}
			Vec3            value;
			uint16_t        startButton;
			uint8_t         target;
			uint8_t         started;
			uint8_t         performed;
			uint8_t         delta;
		};

		struct AxisControl : public IControl
		{
			AxisControl(uint16_t start, uint8_t target, uint8_t axis)
			{
				memset(this, 0, sizeof(*this));
				startButton = start;
				this->target = target;
				axisCount = axis;
				type = SG_CONTROL_TYPE_AXIS;
			}
			Vec3            value;
			Vec3            newValue;
			uint16_t        startButton;
			uint8_t         target;
			uint8_t         axisCount;
			uint8_t         started;
			uint8_t         performed;
		};

		struct VirtualJoystickControl : public IControl
		{
			Vec2     startPos;
			Vec2     currPos;
			float    outsideRadius;
			float    deadzone;
			float    scale;
			uint8_t  touchIndex;
			uint8_t  started;
			uint8_t  performed;
			uint8_t  area;
			uint8_t  isPressed;
			uint8_t  initialized;
			uint8_t  active;
		};

		struct ComboControl : public IControl
		{
			uint16_t pressButton;
			uint16_t triggerButton;
			uint8_t  pressed;
		};

//		const eastl::unordered_map<uint32_t, gainput::MouseButton> mMouseMap =
//		{
//	#ifndef SG_NO_DEFAULT_KEY_BINDINGS
//			{ InputBindings::SG_BUTTON_SOUTH, gainput::MouseButtonLeft },
//			// Following are for UI windows
//			{ InputBindings::SG_BUTTON_MOUSE_RIGHT, gainput::MouseButtonRight },
//			{ InputBindings::SG_BUTTON_MOUSE_MIDDLE, gainput::MouseButtonMiddle },
//			{ InputBindings::SG_BUTTON_MOUSE_SCROLL_UP, gainput::MouseButtonWheelUp },
//			{ InputBindings::SG_BUTTON_MOUSE_SCROLL_DOWN, gainput::MouseButtonWheelDown },
//	#else
//
//			{ InputBindings::SG_BUTTON_MOUSE_LEFT, gainput::MouseButtonLeft },
//			{ InputBindings::SG_BUTTON_MOUSE_RIGHT, gainput::MouseButtonRight },
//			{ InputBindings::SG_BUTTON_MOUSE_MIDDLE, gainput::MouseButtonMiddle },
//			{ InputBindings::SG_BUTTON_MOUSE_SCROLL_UP, gainput::MouseButtonWheelUp },
//			{ InputBindings::SG_BUTTON_MOUSE_SCROLL_DOWN, gainput::MouseButtonWheelDown },
//			{ InputBindings::SG_BUTTON_MOUSE_5, gainput::MouseButton5},
//			{ InputBindings::SG_BUTTON_MOUSE_6, gainput::MouseButton6},
//	#endif
//		};
//
//		const eastl::unordered_map<uint32_t, gainput::Key> mKeyMap =
//		{
//	#ifndef SG_NO_DEFAULT_KEY_BINDINGS
//			{ InputBindings::SG_BUTTON_EXIT, gainput::KeyEscape },
//			{ InputBindings::SG_BUTTON_BACK, gainput::KeyBackSpace },
//			{ InputBindings::SG_BUTTON_NORTH, gainput::KeySpace },
//			{ InputBindings::SG_BUTTON_R3, gainput::KeyF1 },
//			{ InputBindings::SG_BUTTON_L3, gainput::KeyF2 },
//			{ InputBindings::SG_BUTTON_DUMP, gainput::KeyF3 },
//			// Following are for UI text inputs
//			{ InputBindings::SG_BUTTON_KEY_LEFT, gainput::KeyLeft},
//			{ InputBindings::SG_BUTTON_KEY_RIGHT, gainput::KeyRight},
//			{ InputBindings::SG_BUTTON_KEY_SHIFTL, gainput::KeyShiftL},
//			{ InputBindings::SG_BUTTON_KEY_SHIFTR, gainput::KeyShiftR},
//			{ InputBindings::SG_BUTTON_KEY_HOME, gainput::KeyHome},
//			{ InputBindings::SG_BUTTON_KEY_END, gainput::KeyEnd},
//			{ InputBindings::SG_BUTTON_KEY_DELETE, gainput::KeyDelete},
//	#else
//			{ InputBindings::SG_BUTTON_KEY_ESCAPE, gainput::KeyEscape},
//			{ InputBindings::SG_BUTTON_KEY_F1, gainput::KeyF1},
//			{ InputBindings::SG_BUTTON_KEY_F2, gainput::KeyF2},
//			{ InputBindings::SG_BUTTON_KEY_F3, gainput::KeyF3},
//			{ InputBindings::SG_BUTTON_KEY_F4, gainput::KeyF4},
//			{ InputBindings::SG_BUTTON_KEY_F5, gainput::KeyF5},
//			{ InputBindings::SG_BUTTON_KEY_F6, gainput::KeyF6},
//			{ InputBindings::SG_BUTTON_KEY_F7, gainput::KeyF7},
//			{ InputBindings::SG_BUTTON_KEY_F8, gainput::KeyF8},
//			{ InputBindings::SG_BUTTON_KEY_F9, gainput::KeyF9},
//			{ InputBindings::SG_BUTTON_KEY_F10, gainput::KeyF10},
//			{ InputBindings::SG_BUTTON_KEY_F11, gainput::KeyF11},
//			{ InputBindings::SG_BUTTON_KEY_F12, gainput::KeyF12},
//			{ InputBindings::SG_BUTTON_KEY_F13, gainput::KeyF13},
//			{ InputBindings::SG_BUTTON_KEY_F14, gainput::KeyF14},
//			{ InputBindings::SG_BUTTON_KEY_F15, gainput::KeyF15},
//			{ InputBindings::SG_BUTTON_KEY_F16, gainput::KeyF16},
//			{ InputBindings::SG_BUTTON_KEY_F17, gainput::KeyF17},
//			{ InputBindings::SG_BUTTON_KEY_F18, gainput::KeyF18},
//			{ InputBindings::SG_BUTTON_KEY_F19, gainput::KeyF19},
//			{ InputBindings::SG_BUTTON_KEY_PRINT, gainput::KeyPrint},
//			{ InputBindings::SG_BUTTON_KEY_SCROLLLOCK, gainput::KeyScrollLock},
//			{ InputBindings::SG_BUTTON_KEY_BREAK, gainput::KeyBreak},
//			{ InputBindings::SG_BUTTON_KEY_SPACE, gainput::KeySpace},
//			{ InputBindings::SG_BUTTON_KEY_APOSTROPHE, gainput::KeyApostrophe},
//			{ InputBindings::SG_BUTTON_KEY_COMMA, gainput::KeyComma},
//			{ InputBindings::SG_BUTTON_KEY_MINUS, gainput::KeyMinus},
//			{ InputBindings::SG_BUTTON_KEY_PERIOD, gainput::KeyPeriod},
//			{ InputBindings::SG_BUTTON_KEY_SLASH, gainput::KeySlash},
//			{ InputBindings::SG_BUTTON_KEY_0, gainput::Key0},
//			{ InputBindings::SG_BUTTON_KEY_1, gainput::Key1},
//			{ InputBindings::SG_BUTTON_KEY_2, gainput::Key2},
//			{ InputBindings::SG_BUTTON_KEY_3, gainput::Key3},
//			{ InputBindings::SG_BUTTON_KEY_4, gainput::Key4},
//			{ InputBindings::SG_BUTTON_KEY_5, gainput::Key5},
//			{ InputBindings::SG_BUTTON_KEY_6, gainput::Key6},
//			{ InputBindings::SG_BUTTON_KEY_7, gainput::Key7},
//			{ InputBindings::SG_BUTTON_KEY_8, gainput::Key8},
//			{ InputBindings::SG_BUTTON_KEY_9, gainput::Key9},
//			{ InputBindings::SG_BUTTON_KEY_SEMICOLON, gainput::KeySemicolon},
//			{ InputBindings::SG_BUTTON_KEY_LESS, gainput::KeyLess},
//			{ InputBindings::SG_BUTTON_KEY_EQUAL, gainput::KeyEqual},
//			{ InputBindings::SG_BUTTON_KEY_A, gainput::KeyA},
//			{ InputBindings::SG_BUTTON_KEY_B, gainput::KeyB},
//			{ InputBindings::SG_BUTTON_KEY_C, gainput::KeyC},
//			{ InputBindings::SG_BUTTON_KEY_D, gainput::KeyD},
//			{ InputBindings::SG_BUTTON_KEY_E, gainput::KeyE},
//			{ InputBindings::SG_BUTTON_KEY_F, gainput::KeyF},
//			{ InputBindings::SG_BUTTON_KEY_G, gainput::KeyG},
//			{ InputBindings::SG_BUTTON_KEY_H, gainput::KeyH},
//			{ InputBindings::SG_BUTTON_KEY_I, gainput::KeyI},
//			{ InputBindings::SG_BUTTON_KEY_J, gainput::KeyJ},
//			{ InputBindings::SG_BUTTON_KEY_K, gainput::KeyK},
//			{ InputBindings::SG_BUTTON_KEY_L, gainput::KeyL},
//			{ InputBindings::SG_BUTTON_KEY_M, gainput::KeyM},
//			{ InputBindings::SG_BUTTON_KEY_N, gainput::KeyN},
//			{ InputBindings::SG_BUTTON_KEY_O, gainput::KeyO},
//			{ InputBindings::SG_BUTTON_KEY_P, gainput::KeyP},
//			{ InputBindings::SG_BUTTON_KEY_Q, gainput::KeyQ},
//			{ InputBindings::SG_BUTTON_KEY_R, gainput::KeyR},
//			{ InputBindings::SG_BUTTON_KEY_S, gainput::KeyS},
//			{ InputBindings::SG_BUTTON_KEY_T, gainput::KeyT},
//			{ InputBindings::SG_BUTTON_KEY_U, gainput::KeyU},
//			{ InputBindings::SG_BUTTON_KEY_V, gainput::KeyV},
//			{ InputBindings::SG_BUTTON_KEY_W, gainput::KeyW},
//			{ InputBindings::SG_BUTTON_KEY_X, gainput::KeyX},
//			{ InputBindings::SG_BUTTON_KEY_Y, gainput::KeyY},
//			{ InputBindings::SG_BUTTON_KEY_Z, gainput::KeyZ},
//			{ InputBindings::SG_BUTTON_KEY_BRACKETLEFT, gainput::KeyBracketLeft},
//			{ InputBindings::SG_BUTTON_KEY_BACKSLASH, gainput::KeyBackslash},
//			{ InputBindings::SG_BUTTON_KEY_BRACKETRIGHT, gainput::KeyBracketRight},
//			{ InputBindings::SG_BUTTON_KEY_GRAVE, gainput::KeyGrave},
//			{ InputBindings::SG_BUTTON_KEY_LEFT, gainput::KeyLeft},
//			{ InputBindings::SG_BUTTON_KEY_RIGHT, gainput::KeyRight},
//			{ InputBindings::SG_BUTTON_KEY_UP, gainput::KeyUp},
//			{ InputBindings::SG_BUTTON_KEY_DOWN, gainput::KeyDown},
//			{ InputBindings::SG_BUTTON_KEY_INSERT, gainput::KeyInsert},
//			{ InputBindings::SG_BUTTON_KEY_HOME, gainput::KeyHome},
//			{ InputBindings::SG_BUTTON_KEY_DELETE, gainput::KeyDelete},
//			{ InputBindings::SG_BUTTON_KEY_END, gainput::KeyEnd},
//			{ InputBindings::SG_BUTTON_KEY_PAGEUP, gainput::KeyPageUp},
//			{ InputBindings::SG_BUTTON_KEY_PAGEDOWN, gainput::KeyPageDown},
//			{ InputBindings::SG_BUTTON_KEY_NUMLOCK, gainput::KeyNumLock},
//			{ InputBindings::SG_BUTTON_KEY_KPEQUAL, gainput::KeyKpEqual},
//			{ InputBindings::SG_BUTTON_KEY_KPDIVIDE, gainput::KeyKpDivide},
//			{ InputBindings::SG_BUTTON_KEY_KPMULTIPLY, gainput::KeyKpMultiply},
//			{ InputBindings::SG_BUTTON_KEY_KPSUBTRACT, gainput::KeyKpSubtract},
//			{ InputBindings::SG_BUTTON_KEY_KPADD, gainput::KeyKpAdd},
//			{ InputBindings::SG_BUTTON_KEY_KPENTER, gainput::KeyKpEnter},
//			{ InputBindings::SG_BUTTON_KEY_KPINSERT, gainput::KeyKpInsert},
//			{ InputBindings::SG_BUTTON_KEY_KPEND, gainput::KeyKpEnd},
//			{ InputBindings::SG_BUTTON_KEY_KPDOWN, gainput::KeyKpDown},
//			{ InputBindings::SG_BUTTON_KEY_KPPAGEDOWN, gainput::KeyKpPageDown},
//			{ InputBindings::SG_BUTTON_KEY_KPLEFT, gainput::KeyKpLeft},
//			{ InputBindings::SG_BUTTON_KEY_KPBEGIN, gainput::KeyKpBegin},
//			{ InputBindings::SG_BUTTON_KEY_KPRIGHT, gainput::KeyKpRight},
//			{ InputBindings::SG_BUTTON_KEY_KPHOME, gainput::KeyKpHome},
//			{ InputBindings::SG_BUTTON_KEY_KPUP, gainput::KeyKpUp},
//			{ InputBindings::SG_BUTTON_KEY_KPPAGEUP, gainput::KeyKpPageUp},
//			{ InputBindings::SG_BUTTON_KEY_KPDELETE, gainput::KeyKpDelete},
//			{ InputBindings::SG_BUTTON_KEY_BACKSPACE, gainput::KeyBackSpace},
//			{ InputBindings::SG_BUTTON_KEY_TAB, gainput::KeyTab},
//			{ InputBindings::SG_BUTTON_KEY_RETURN, gainput::KeyReturn},
//			{ InputBindings::SG_BUTTON_KEY_CAPSLOCK, gainput::KeyCapsLock},
//			{ InputBindings::SG_BUTTON_KEY_SHIFTL, gainput::KeyShiftL},
//			{ InputBindings::SG_BUTTON_KEY_CTRLL, gainput::KeyCtrlL},
//			{ InputBindings::SG_BUTTON_KEY_SUPERL, gainput::KeySuperL},
//			{ InputBindings::SG_BUTTON_KEY_ALTL, gainput::KeyAltL},
//			{ InputBindings::SG_BUTTON_KEY_ALTR, gainput::KeyAltR},
//			{ InputBindings::SG_BUTTON_KEY_SUPERR, gainput::KeySuperR},
//			{ InputBindings::SG_BUTTON_KEY_MENU, gainput::KeyMenu},
//			{ InputBindings::SG_BUTTON_KEY_CTRLR, gainput::KeyCtrlR},
//			{ InputBindings::SG_BUTTON_KEY_SHIFTR, gainput::KeyShiftR},
//			{ InputBindings::SG_BUTTON_KEY_BACK, gainput::KeyBack},
//			{ InputBindings::SG_BUTTON_KEY_SOFTLEFT, gainput::KeySoftLeft},
//			{ InputBindings::SG_BUTTON_KEY_SOFTRIGHT, gainput::KeySoftRight},
//			{ InputBindings::SG_BUTTON_KEY_CALL, gainput::KeyCall},
//			{ InputBindings::SG_BUTTON_KEY_ENDCALL, gainput::KeyEndcall},
//			{ InputBindings::SG_BUTTON_KEY_STAR, gainput::KeyStar},
//			{ InputBindings::SG_BUTTON_KEY_POUND, gainput::KeyPound},
//			{ InputBindings::SG_BUTTON_KEY_DPADCENTER, gainput::KeyDpadCenter},
//			{ InputBindings::SG_BUTTON_KEY_VOLUMEUP, gainput::KeyVolumeUp},
//			{ InputBindings::SG_BUTTON_KEY_VOLUMEDOWN, gainput::KeyVolumeDown},
//			{ InputBindings::SG_BUTTON_KEY_POWER, gainput::KeyPower},
//			{ InputBindings::SG_BUTTON_KEY_CAMERA, gainput::KeyCamera},
//			{ InputBindings::SG_BUTTON_KEY_CLEAR, gainput::KeyClear},
//			{ InputBindings::SG_BUTTON_KEY_SYMBOL, gainput::KeySymbol},
//			{ InputBindings::SG_BUTTON_KEY_EXPLORER, gainput::KeyExplorer},
//			{ InputBindings::SG_BUTTON_KEY_ENVELOPE, gainput::KeyEnvelope},
//			{ InputBindings::SG_BUTTON_KEY_EQUALS, gainput::KeyEquals},
//			{ InputBindings::SG_BUTTON_KEY_AT, gainput::KeyAt},
//			{ InputBindings::SG_BUTTON_KEY_HEADSETHOOK, gainput::KeyHeadsethook},
//			{ InputBindings::SG_BUTTON_KEY_FOCUS, gainput::KeyFocus},
//			{ InputBindings::SG_BUTTON_KEY_PLUS, gainput::KeyPlus},
//			{ InputBindings::SG_BUTTON_KEY_NOTIFICATION, gainput::KeyNotification},
//			{ InputBindings::SG_BUTTON_KEY_SEARCH, gainput::KeySearch},
//			{ InputBindings::SG_BUTTON_KEY_MEDIAPLAYPAUSE, gainput::KeyMediaPlayPause},
//			{ InputBindings::SG_BUTTON_KEY_MEDIASTOP, gainput::KeyMediaStop},
//			{ InputBindings::SG_BUTTON_KEY_MEDIANEXT, gainput::KeyMediaNext},
//			{ InputBindings::SG_BUTTON_KEY_MEDIAPREVIOUS, gainput::KeyMediaPrevious},
//			{ InputBindings::SG_BUTTON_KEY_MEDIAREWIND, gainput::KeyMediaRewind},
//			{ InputBindings::SG_BUTTON_KEY_MEDIAFASTFORWARD, gainput::KeyMediaFastForward},
//			{ InputBindings::SG_BUTTON_KEY_MUTE, gainput::KeyMute},
//			{ InputBindings::SG_BUTTON_KEY_PICTSYMBOLS, gainput::KeyPictsymbols},
//			{ InputBindings::SG_BUTTON_KEY_SWITCHCHARSET, gainput::KeySwitchCharset},
//			{ InputBindings::SG_BUTTON_KEY_FORWARD, gainput::KeyForward},
//			{ InputBindings::SG_BUTTON_KEY_EXTRA1, gainput::KeyExtra1},
//			{ InputBindings::SG_BUTTON_KEY_EXTRA2, gainput::KeyExtra2},
//			{ InputBindings::SG_BUTTON_KEY_EXTRA3, gainput::KeyExtra3},
//			{ InputBindings::SG_BUTTON_KEY_EXTRA4, gainput::KeyExtra4},
//			{ InputBindings::SG_BUTTON_KEY_EXTRA5, gainput::KeyExtra5},
//			{ InputBindings::SG_BUTTON_KEY_EXTRA6, gainput::KeyExtra6},
//			{ InputBindings::SG_BUTTON_KEY_FN, gainput::KeyFn},
//			{ InputBindings::SG_BUTTON_KEY_CIRCUMFLEX, gainput::KeyCircumflex},
//			{ InputBindings::SG_BUTTON_KEY_SSHARP, gainput::KeySsharp},
//			{ InputBindings::SG_BUTTON_KEY_ACUTE, gainput::KeyAcute},
//			{ InputBindings::SG_BUTTON_KEY_ALTGR, gainput::KeyAltGr},
//			{ InputBindings::SG_BUTTON_KEY_NUMBERSIGN, gainput::KeyNumbersign},
//			{ InputBindings::SG_BUTTON_KEY_UDIAERESIS, gainput::KeyUdiaeresis},
//			{ InputBindings::SG_BUTTON_KEY_ADIAERESIS, gainput::KeyAdiaeresis},
//			{ InputBindings::SG_BUTTON_KEY_ODIAERESIS, gainput::KeyOdiaeresis},
//			{ InputBindings::SG_BUTTON_KEY_SECTION, gainput::KeySection},
//			{ InputBindings::SG_BUTTON_KEY_ARING, gainput::KeyAring},
//			{ InputBindings::SG_BUTTON_KEY_DIAERESIS, gainput::KeyDiaeresis},
//			{ InputBindings::SG_BUTTON_KEY_TWOSUPERIOR, gainput::KeyTwosuperior},
//			{ InputBindings::SG_BUTTON_KEY_RIGHTPARENTHESIS, gainput::KeyRightParenthesis},
//			{ InputBindings::SG_BUTTON_KEY_DOLLAR, gainput::KeyDollar},
//			{ InputBindings::SG_BUTTON_KEY_UGRAVE, gainput::KeyUgrave},
//			{ InputBindings::SG_BUTTON_KEY_ASTERISK, gainput::KeyAsterisk},
//			{ InputBindings::SG_BUTTON_KEY_COLON, gainput::KeyColon},
//			{ InputBindings::SG_BUTTON_KEY_EXCLAM, gainput::KeyExclam},
//			{ InputBindings::SG_BUTTON_KEY_BRACELEFT, gainput::KeyBraceLeft},
//			{ InputBindings::SG_BUTTON_KEY_BRACERIGHT, gainput::KeyBraceRight},
//			{ InputBindings::SG_BUTTON_KEY_SYSRQ, gainput::KeySysRq},
//
//	#endif
//		};
//
//		const eastl::unordered_map<uint32_t, gainput::PadButton> mGamepadMap =
//		{
//			{ InputBindings::SG_BUTTON_DPAD_LEFT, gainput::PadButtonLeft },
//			{ InputBindings::SG_BUTTON_DPAD_RIGHT, gainput::PadButtonRight },
//			{ InputBindings::SG_BUTTON_DPAD_UP, gainput::PadButtonUp },
//			{ InputBindings::SG_BUTTON_DPAD_DOWN, gainput::PadButtonDown },
//			{ InputBindings::SG_BUTTON_SOUTH, gainput::PadButtonA }, // A/CROSS
//			{ InputBindings::SG_BUTTON_EAST, gainput::PadButtonB }, // B/CIRCLE
//			{ InputBindings::SG_BUTTON_WEST, gainput::PadButtonX }, // X/SQUARE
//			{ InputBindings::SG_BUTTON_NORTH, gainput::PadButtonY }, // Y/TRIANGLE
//			{ InputBindings::SG_BUTTON_L1, gainput::PadButtonL1 },
//			{ InputBindings::SG_BUTTON_R1, gainput::PadButtonR1 },
//			{ InputBindings::SG_BUTTON_L2, gainput::PadButtonL2 },
//			{ InputBindings::SG_BUTTON_R2, gainput::PadButtonR2 },
//			{ InputBindings::SG_BUTTON_L3, gainput::PadButtonL3 }, // LEFT THUMB
//			{ InputBindings::SG_BUTTON_R3, gainput::PadButtonR3 }, // RIGHT THUMB
//			{ InputBindings::SG_BUTTON_START, gainput::PadButtonStart },
//			{ InputBindings::SG_BUTTON_SELECT, gainput::PadButtonSelect },
//			{ InputBindings::SG_BUTTON_TOUCH, gainput::PadButton17 }, // PS BUTTON
//			{ InputBindings::SG_BUTTON_HOME, gainput::PadButtonHome}, // PS BUTTON
//		};
//
//		const eastl::unordered_map<uint32_t, AxisControl> mGamepadAxisMap =
//		{
//			{ InputBindings::SG_FLOAT_L2, { (uint16_t)gainput::PadButtonAxis4, 1, 1 } },
//			{ InputBindings::SG_FLOAT_R2, { (uint16_t)gainput::PadButtonAxis5, 1, 1 } },
//			{ InputBindings::SG_FLOAT_LEFTSTICK, { (uint16_t)gainput::PadButtonLeftStickX, (1 << 1) | 1, 2 } },
//			{ InputBindings::SG_FLOAT_RIGHTSTICK, { (uint16_t)gainput::PadButtonRightStickX, (1 << 1) | 1, 2 } },
//		};
//
//#ifndef SG_NO_DEFAULT_KEY_BINDINGS
//		const eastl::unordered_map<uint32_t, CompositeControl> mGamepadCompositeMap
//		{
//			{ InputBindings::SG_FLOAT_LEFTSTICK, { { gainput::KeyD, gainput::KeyA, gainput::KeyW, gainput::KeyS }, 4 } },
//		};
//#endif
//
//		const eastl::unordered_map<uint32_t, FloatControl> mGamepadFloatMap =
//		{
//			{ InputBindings::SG_FLOAT_RIGHTSTICK, { (uint16_t)gainput::MouseAxisX, (1 << 1) | 1, true, true } },
//		};

		/// Maps the gainput button to the InputBindings::KeyBindings enum
		//eastl::vector<uint32_t>                mControlMapReverse[SG_MAX_INPUT_DEVICES];
		/// List of all input controls per device
		eastl::vector<eastl::vector<IControl*> > mControls[SG_MAX_INPUT_DEVICES];
		/// List of gestures
		//eastl::vector<InputAction*>            mGestureControls;
		/// List of all text input actions
		/// These actions will be invoked everytime there is a text character typed on a physical / virtual keyboard
		eastl::vector<InputAction*>              mTextInputControls;
		/// List of controls which need to be canceled at the end of the frame
		eastl::unordered_set<FloatControl*>      mFloatDeltaControlCancelQueue;
		eastl::unordered_set<IControl*>          mButtonControlPerformQueue;

		eastl::vector<InputAction>               mActions;
		eastl::vector<IControl*>                 mControlPool;
#if SG_TOUCH_INPUT
		Vec2                                     mTouchPositions[TouchCount_ >> 2];
#else										     
		Vec2                                     mMousePosition;
#endif

		/// Window pointer passed by the app
		/// Input capture will be performed on this window
		WindowDesc* pWindow = nullptr;

		/// Gainput Manager which lets us talk with the gainput backend
		InputManager* pInputManager = nullptr;

		InputDeviceType* pDeviceTypes;
		//DeviceId* pGamepadDeviceIDs;
		DeviceId  mouseDeviceID;
		//DeviceId  rawMouseDeviceID;
		DeviceId  keyboardDeviceID;
		//DeviceId  touchDeviceID;

		bool isVirtualKeyboardActive;
		bool isInputCaptured;
		bool useDefaultCapture;

		bool init(WindowDesc* window)
		{
			pWindow = window;

			// clear up defaults
			isVirtualKeyboardActive = false;
			useDefaultCapture = true;
			isInputCaptured = false;

			//pGamepadDeviceIDs = (DeviceId*)sg_calloc(MAX_INPUT_GAMEPADS, sizeof(DeviceId));
			pDeviceTypes = (InputDeviceType*)sg_calloc(MAX_INPUT_GAMEPADS + 4, sizeof(InputDeviceType));

			// default device ids
			mouseDeviceID = InvalidDeviceId;
			//rawMouseDeviceID = InvalidDeviceId;
			keyboardDeviceID = InvalidDeviceId;
			//for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
			//	pGamepadDeviceIDs[i] = InvalidDeviceId;
			//touchDeviceID = InvalidDeviceId;

			// create input manager
			pInputManager = sg_new(InputManager);
			ASSERT(pInputManager);

#if defined(SG_PLATFORM_WINDOWS) || defined(XBOX)
			//pInputManager->SetWindowsInstance(window->handle.window);
#endif
			// create all necessary devices
			mouseDeviceID = pInputManager->create_device<InputDeviceMouse>();
			//rawMouseDeviceID = pInputManager->CreateDevice<InputDeviceMouse>();
			keyboardDeviceID = pInputManager->create_device<InputDeviceKeyboard>();
			/*touchDeviceID = pInputManager->CreateDevice<InputDeviceTouch>();
			for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
				pGamepadDeviceIDs[i] = pInputManager->CreateDevice<gainput::InputDevicePad>();*/

			// assign device types
			pDeviceTypes[mouseDeviceID] = InputDeviceType::SG_INPUT_DEVICE_MOUSE;
			//pDeviceTypes[rawMouseDeviceID] = InputDeviceType::SG_INPUT_DEVICE_MOUSE;
			pDeviceTypes[keyboardDeviceID] = InputDeviceType::SG_INPUT_DEVICE_KEYBOARD;
			//pDeviceTypes[touchDeviceID] = InputDeviceType::SG_INPUT_DEVICE_TOUCH;

			// create control maps
			mControls[keyboardDeviceID].resize(SG_BUTTON_KEY_COUNT);
			mControls[mouseDeviceID].resize(SG_BUTTON_MOUSE_COUNT);
			//mControls[rawMouseDeviceID].resize(gainput::MouseButtonCount_);
			//mControls[touchDeviceID].resize(gainput::TouchCount_);

			//mControlMapReverse[mouseDeviceID] = eastl::vector<uint32_t>(gainput::MouseButtonCount, UINT_MAX);
			//mControlMapReverse[keyboardDeviceID] = eastl::vector<uint32_t>(gainput::KeyCount_, UINT_MAX);
			//mControlMapReverse[touchDeviceID] = eastl::vector<uint32_t>(gainput::TouchCount_, UINT_MAX);

			// first is gainput's bindings, second is our KeyBindings,
			//for (decltype(mMouseMap)::const_iterator it = mMouseMap.begin(); it != mMouseMap.end(); ++it)
			//	mControlMapReverse[mouseDeviceID][it->second] = it->first;

			//for (decltype(mKeyMap)::const_iterator it = mKeyMap.begin(); it != mKeyMap.end(); ++it)
			//	mControlMapReverse[keyboardDeviceID][it->second] = it->first;

			// every touch can map to the same button
			//for (uint32_t i = 0; i < gainput::TouchCount_; ++i)
			//	mControlMapReverse[touchDeviceID][i] = InputBindings::SG_BUTTON_SOUTH;

			//for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
			//{
			//	gainput::DeviceId deviceId = pGamepadDeviceIDs[i];

			//	pDeviceTypes[deviceId] = InputDeviceType::SG_INPUT_DEVICE_GAMEPAD;
			//	mControls[deviceId].resize(gainput::PadButtonMax_);
			//	mControlMapReverse[deviceId] = eastl::vector<uint32_t>(gainput::PadButtonMax_, UINT_MAX);

			//	for (decltype(mGamepadMap)::const_iterator it = mGamepadMap.begin(); it != mGamepadMap.end(); ++it)
			//		mControlMapReverse[deviceId][it->second] = it->first;
			//}

			mActions.reserve(MAX_INPUT_ACTIONS);

			pInputManager->add_listener(this);

			return true;
		}

		void exit()
		{
			ASSERT(pInputManager);

			for (uint32_t i = 0; i < (uint32_t)mControlPool.size(); ++i)
				sg_free(mControlPool[i]);

			//sg_free(pGamepadDeviceIDs);
			sg_free(pDeviceTypes);

			sg_delete(pInputManager);
		}

		void update(uint32_t width, uint32_t height)
		{
			ASSERT(pInputManager);

			for (FloatControl* pControl : mFloatDeltaControlCancelQueue)
			{
				pControl->started = 0;
				pControl->performed = 0;
				pControl->value = Vec3(0.0f);

				if (pControl->pAction->desc.pFunction)
				{
					InputActionContext ctx = {};
					ctx.pUserData = pControl->pAction->desc.pUserData;
					ctx.phase = SG_INPUT_ACTION_PHASE_CANCELED;
					ctx.pCaptured = &useDefaultCapture;
#if SG_TOUCH_INPUT
					ctx.pPosition = &mTouchPositions[pControl->pAction->desc.userId];
#else
					ctx.pPosition = &mMousePosition;
#endif
					pControl->pAction->desc.pFunction(&ctx);
				}
			}

//#if SG_TOUCH_INPUT
//			for (IControl* pControl : mButtonControlPerformQueue)
//			{
//				InputActionContext ctx = {};
//				ctx.pUserData = pControl->pAction->desc.pUserData;
//				ctx.phase = SG_INPUT_ACTION_PHASE_PERFORMED;
//				ctx.pCaptured = &useDefaultCapture;
//				ctx.phase = SG_INPUT_ACTION_PHASE_PERFORMED;
//				ctx.binding = InputBindings::SG_BUTTON_SOUTH;
//				ctx.pPosition = &mTouchPositions[pControl->pAction->desc.userId];
//				ctx.bool1 = true;
//				pControl->pAction->desc.pFunction(&ctx);
//			}
//#endif

			// finishing the update, we clear the control queue
			mButtonControlPerformQueue.clear();
			mFloatDeltaControlCancelQueue.clear();

			//auto* keyboard = (InputDeviceKeyboard*)pInputManager->get_device(keyboardDeviceID);
			//if (keyboard)
			//{
			//	char pText = keyboard->GetNextCharacter();
			//	if (pText)
			//	{
			//		InputActionContext ctx = {};
			//		ctx.text = pText;
			//		ctx.phase = SG_INPUT_ACTION_PHASE_PERFORMED;
			//		for (InputAction* pAction : mTextInputControls)
			//		{
			//			ctx.pUserData = pAction->desc.pUserData;
			//			if (!pAction->desc.pFunction(&ctx))
			//				break;
			//		}
			//	}
			//}

			pInputManager->SetDisplaySize(width, height);
			pInputManager->update();

#if defined(__linux__) && !defined(__ANDROID__) && !defined(GAINPUT_PLATFORM_GGP)
			//this needs to be done before updating the events
			//that way current frame data will be delta after resetting mouse position
			if (mInputCaptured)
			{
				ASSERT(pWindow);

				float x = 0;
				float y = 0;
				x = (pWindow->windowedRect.right - pWindow->windowedRect.left) / 2;
				y = (pWindow->windowedRect.bottom - pWindow->windowedRect.top) / 2;
				XWarpPointer(pWindow->handle.display, None, pWindow->handle.window, 0, 0, 0, 0, x, y);
				gainput::InputDevice* device = pInputManager->GetDevice(mRawMouseDeviceID);
				device->WarpMouse(x, y);
				XFlush(pWindow->handle.display);
			}
#endif
		}

		template <typename T>
		T* allocate_control()
		{
			T* pControl = (T*)sg_calloc(1, sizeof(T));
			mControlPool.emplace_back(pControl);
			return pControl;
		}

		InputAction* register_input_action(const InputActionDesc* pDesc)
		{
			ASSERT(pDesc);

			mActions.emplace_back(InputAction{});
			InputAction* pAction = &mActions.back();
			ASSERT(pAction);

			pAction->desc = *pDesc;
#if defined(TARGET_IOS)
			if (pGainputView && InputBindings::GESTURE_BINDINGS_BEGIN <= pDesc->mBinding && InputBindings::GESTURE_BINDINGS_END >= pDesc->mBinding)
			{
				const InputBindings::GestureDesc* pGesture = pDesc->pGesture;
				ASSERT(pGesture);

				GainputView* view = (__bridge GainputView*)pGainputView;
				uint32_t gestureId = (uint32_t)mGestureControls.size();
				gainput::GestureConfig gestureConfig = {};
				gestureConfig.mType = (gainput::GestureType)(pDesc->mBinding - InputBindings::GESTURE_BINDINGS_BEGIN);
				gestureConfig.maxNumberOfTouches = pGesture->maxNumberOfTouches;
				gestureConfig.minNumberOfTouches = pGesture->minNumberOfTouches;
				gestureConfig.minimumPressDuration = pGesture->minimumPressDuration;
				gestureConfig.numberOfTapsRequired = pGesture->numberOfTapsRequired;
				[view addGestureMapping : gestureId withConfig : gestureConfig] ;
				mGestureControls.emplace_back(pAction);
				return pAction;
			}
#endif

			//if (pDesc->binding == SG_TEXT)
			//{
			//	ASSERT(pDesc->pFunction);
			//	mTextInputControls.emplace_back(pAction);
			//	return pAction;
			//}

			const uint32_t control = pDesc->binding;
			//const DeviceId gamepadDeviceId = pGamepadDeviceIDs[pDesc->userId];

			if (SG_BUTTON_ANY == control)
			{
				IControl* pControl = allocate_control<IControl>();
				ASSERT(pControl);

				pControl->type = SG_CONTROL_TYPE_BUTTON;
				pControl->pAction = pAction;

				for (uint32_t i = SG_BUTTON_KEY_BEGIN; i <= SG_BUTTON_KEY_END; i++)
					mControls[keyboardDeviceID][pDesc->binding].emplace_back(pControl);
#if SG_TOUCH_INPUT
				mControls[touchDeviceID][TOUCH_DOWN(pDesc->userId)].emplace_back(pControl);
#else
				for (uint32_t i = SG_BUTTON_MOUSE_BEGIN; i <= SG_BUTTON_MOUSE_END; i++)
					mControls[mouseDeviceID][pDesc->binding].emplace_back(pControl);
#endif
				return pAction;
			}
			else if (SG_BUTTON_KEY_FULLSCREEN == control)
			{
				ComboControl* pControl = allocate_control<ComboControl>();
				ASSERT(pControl);

				pControl->type = SG_CONTROL_TYPE_COMBO;
				pControl->pAction = pAction;
				pControl->pressButton = SG_BUTTON_KEY_ALTL;
				pControl->triggerButton = SG_BUTTON_KEY_RETURN;
				mControls[keyboardDeviceID][SG_BUTTON_KEY_RETURN].emplace_back(pControl);
				mControls[keyboardDeviceID][SG_BUTTON_KEY_ALTL].emplace_back(pControl);
			}
			//else if (SG_BUTTON_KEY_DUMP == control)
			//{
			//	ComboControl* pGamePadControl = allocate_control<ComboControl>();
			//	ASSERT(pGamePadControl);

			//	pGamePadControl->type = SG_CONTROL_TYPE_COMBO;
			//	pGamePadControl->pAction = pAction;
			//	pGamePadControl->pressButton = gainput::PadButtonStart;
			//	pGamePadControl->triggerButton = gainput::PadButtonB;
			//	mControls[gamepadDeviceId][pGamePadControl->triggerButton].emplace_back(pGamePadControl);
			//	mControls[gamepadDeviceId][pGamePadControl->pressButton].emplace_back(pGamePadControl);

			//	ComboControl* pControl = allocate_control<ComboControl>();
			//	ASSERT(pControl);
			//	pControl->type = SG_CONTROL_TYPE_BUTTON;
			//	pControl->pAction = pAction;
			//	decltype(mKeyMap)::const_iterator keyIt = mKeyMap.find(control);
			//	if (keyIt != mKeyMap.end())
			//		mControls[keyboardDeviceID][keyIt->second].emplace_back(pControl);
			//}
			else if (SG_BUTTON_KEY_BEGIN <= control && SG_BUTTON_MOUSE_END >= control)
			{
				IControl* pControl = allocate_control<IControl>();
				ASSERT(pControl);

				pControl->type = SG_CONTROL_TYPE_BUTTON;
				pControl->pAction = pAction;

				//decltype(mGamepadMap)::const_iterator gamepadIt = mGamepadMap.find(control);
				//if (gamepadIt != mGamepadMap.end())
				//	mControls[gamepadDeviceId][gamepadIt->second].emplace_back(pControl);
#if SG_TOUCH_INPUT
				if (InputBindings::SG_BUTTON_SOUTH == control)
					mControls[touchDeviceID][TOUCH_DOWN(pDesc->userId)].emplace_back(pControl);
#else
				mControls[keyboardDeviceID][pDesc->binding].emplace_back(pControl);
				mControls[mouseDeviceID][pDesc->binding].emplace_back(pControl);
#endif
			}
//			else if (InputBindings::SG_FLOAT_BINDINGS_BEGIN <= control && InputBindings::SG_FLOAT_BINDINGS_END >= control)
//			{
//				if (InputBindings::SG_FLOAT_DPAD == control)
//				{
//					CompositeControl* pControl = allocate_control<CompositeControl>();
//					ASSERT(pControl);
//
//					pControl->type = SG_CONTROL_TYPE_COMPOSITE;
//					pControl->pAction = pAction;
//					pControl->composite = 4;
//					pControl->controls[0] = gainput::PadButtonRight;
//					pControl->controls[1] = gainput::PadButtonLeft;
//					pControl->controls[2] = gainput::PadButtonUp;
//					pControl->controls[3] = gainput::PadButtonDown;
//					for (uint32_t i = 0; i < pControl->composite; ++i)
//						mControls[gamepadDeviceId][pControl->controls[i]].emplace_back(pControl);
//				}
//				else
//				{
//					uint32_t axisCount = 0;
//					decltype(mGamepadAxisMap)::const_iterator gamepadIt = mGamepadAxisMap.find(control);
//					ASSERT(gamepadIt != mGamepadAxisMap.end());
//					if (gamepadIt != mGamepadAxisMap.end())
//					{
//						AxisControl* pControl = allocate_control<AxisControl>();
//						ASSERT(pControl);
//
//						*pControl = gamepadIt->second;
//						pControl->pAction = pAction;
//						for (uint32_t i = 0; i < pControl->axisCount; ++i)
//							mControls[gamepadDeviceId][pControl->startButton + i].emplace_back(pControl);
//
//						axisCount = pControl->axisCount;
//					}
//#if SG_TOUCH_INPUT
//					if ((InputBindings::FLOAT_LEFTSTICK == control || InputBindings::FLOAT_RIGHTSTICK == control) && (pDesc->mOutsideRadius && pDesc->mScale))
//					{
//						VirtualJoystickControl* pControl = AllocateControl<VirtualJoystickControl>();
//						ASSERT(pControl);
//
//						pControl->mType = CONTROL_VIRTUAL_JOYSTICK;
//						pControl->pAction = pAction;
//						pControl->mOutsideRadius = pDesc->mOutsideRadius;
//						pControl->mDeadzone = pDesc->mDeadzone;
//						pControl->mScale = pDesc->mScale;
//						pControl->mTouchIndex = 0xFF;
//						pControl->mArea = InputBindings::FLOAT_LEFTSTICK == control ? AREA_LEFT : AREA_RIGHT;
//						mControls[mTouchDeviceID][gainput::Touch0Down].emplace_back(pControl);
//						mControls[mTouchDeviceID][gainput::Touch0X].emplace_back(pControl);
//						mControls[mTouchDeviceID][gainput::Touch0Y].emplace_back(pControl);
//						mControls[mTouchDeviceID][gainput::Touch1Down].emplace_back(pControl);
//						mControls[mTouchDeviceID][gainput::Touch1X].emplace_back(pControl);
//						mControls[mTouchDeviceID][gainput::Touch1Y].emplace_back(pControl);
//					}
//#else
//
//#ifndef SG_NO_DEFAULT_KEY_BINDINGS
//					decltype(mGamepadCompositeMap)::const_iterator keyIt = mGamepadCompositeMap.find(control);
//					if (keyIt != mGamepadCompositeMap.end())
//					{
//						CompositeControl* pControl = allocate_control<CompositeControl>();
//						ASSERT(pControl);
//
//						*pControl = keyIt->second;
//						pControl->pAction = pAction;
//						for (uint32_t i = 0; i < pControl->composite; ++i)
//							mControls[keyboardDeviceID][pControl->controls[i]].emplace_back(pControl);
//					}
//#endif
//
//					decltype(mGamepadFloatMap)::const_iterator floatIt = mGamepadFloatMap.find(control);
//					if (floatIt != mGamepadFloatMap.end())
//					{
//						FloatControl* pControl = allocate_control<FloatControl>();
//						ASSERT(pControl);
//
//						*pControl = floatIt->second;
//						pControl->pAction = pAction;
//
//						gainput::DeviceId deviceId = ((pControl->delta >> 1) & 0x1) ? rawMouseDeviceID : mouseDeviceID;
//
//						for (uint32_t i = 0; i < axisCount; ++i)
//							mControls[deviceId][pControl->startButton + i].emplace_back(pControl);
//					}
//#endif
//				}
//			}

			return pAction;
		}

		void remove_input_action(InputAction* pAction)
		{
			ASSERT(pAction);

			//decltype(mGestureControls)::const_iterator it = eastl::find(mGestureControls.begin(), mGestureControls.end(), pAction);
			//if (it != mGestureControls.end())
			//	mGestureControls.erase(it);

			sg_free(pAction);
		}

		bool set_enable_capture_input(bool enable)
		{
			ASSERT(pWindow);

#if defined(SG_PLATFORM_WINDOWS) && !defined(XBOX)
			static int32_t lastCursorPosX = 0;
			static int32_t lastCursorPosY = 0;

			if (enable != isInputCaptured)
			{
				if (enable)
				{
					POINT lastCursorPoint;
					GetCursorPos(&lastCursorPoint);
					lastCursorPosX = lastCursorPoint.x;
					lastCursorPosY = lastCursorPoint.y;

					HWND handle = (HWND)pWindow->handle.window;
					SetCapture(handle); // capture this window to avoid the miss operation

					RECT clientRect;
					GetClientRect(handle, &clientRect);
					// convert screen rect to client coordinates.
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
					ClipCursor(&clientRect); // restrain the cursor in the client rect
					ShowCursor(FALSE);
				}
				else
				{
					ClipCursor(NULL);
					ShowCursor(TRUE);
					ReleaseCapture(); // release current capture
					SetCursorPos(lastCursorPosX, lastCursorPosY);
				}

				isInputCaptured = enable;
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
				isInputCaptured = enable;

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

		void set_virtual_keyboard(uint32_t type)
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

		inline constexpr bool is_pointer_type(DeviceId device) const
		{
#if SG_TOUCH_INPUT
			return false;
#else
			return (device == mouseDeviceID /*|| device == rawMouseDeviceID*/);
#endif
		}

		bool on_device_button_bool(DeviceId device, DeviceButtonId deviceButton, bool oldValue, bool newValue) override
		{
			if (oldValue == newValue) // nothing change
				return false;

			if (mControls[device].size())
			{
				InputActionContext ctx = {};
				ctx.deviceType = pDeviceTypes[device];
				ctx.pCaptured = is_pointer_type(device) ? &isInputCaptured : &useDefaultCapture;
#if SG_TOUCH_INPUT
				uint32_t touchIndex = 0;
				if (device == touchDeviceID)
				{
					touchIndex = TOUCH_USER(deviceButton);
					gainput::InputDeviceTouch* pTouch = (gainput::InputDeviceTouch*)pInputManager->GetDevice(touchDeviceID);
					mTouchPositions[touchIndex][0] = pTouch->GetFloat(TOUCH_X(touchIndex));
					mTouchPositions[touchIndex][1] = pTouch->GetFloat(TOUCH_Y(touchIndex));
					ctx.pPosition = &mTouchPositions[touchIndex];
				}
#else
				if (is_pointer_type(device))
				{
					InputDeviceMouse* pMouse = (InputDeviceMouse*)pInputManager->get_device(mouseDeviceID);
					mMousePosition[0] = pMouse->get_float(SG_MOUSE_AXIS_X);
					mMousePosition[1] = pMouse->get_float(SG_MOUSE_AXIS_Y);
					ctx.pPosition = &mMousePosition;
					ctx.scrollValue = pMouse->get_float(SG_BUTTON_MOUSE_MIDDLE);
				}
#endif
				bool executeNext = true;

				const eastl::vector<IControl*>& controls = mControls[device][deviceButton];
				for (IControl* control : controls) // for all this button's control
				{
					if (!executeNext)
						return true;

					const InputControlType type = control->type;
					const InputActionDesc* pDesc = &control->pAction->desc;
					ctx.pUserData = pDesc->pUserData;
					ctx.binding = pDesc->binding;

					switch (type)
					{
					case SG_CONTROL_TYPE_BUTTON:
					{
						if (pDesc->pFunction)
						{
							ctx.bool1 = newValue;
							if (newValue && !oldValue) // it is being activate
							{
								ctx.phase = SG_INPUT_ACTION_PHASE_STARTED;
								executeNext = pDesc->pFunction(&ctx) && executeNext; // if the function return false, it will shield the flower execution
#if SG_TOUCH_INPUT
								mButtonControlPerformQueue.insert(control);
#else
								ctx.phase = SG_INPUT_ACTION_PHASE_PERFORMED;
								executeNext = pDesc->pFunction(&ctx) && executeNext;
#endif
							}
							else if (oldValue && !newValue) // it is being deactivate
							{
								ctx.phase = SG_INPUT_ACTION_PHASE_CANCELED;
								executeNext = pDesc->pFunction(&ctx) && executeNext;
							}
						}
						break;
					}
					case SG_CONTROL_TYPE_COMPOSITE:
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
							ctx.float1 = pControl->value[axis];
						}
						else
						{
							if (!pControl->value[0] && !pControl->value[1])
								ctx.float2 = Vec2(0.0f);
							else
								ctx.float2 = pControl->value;
						}

						// action started
						if (!pControl->started && !oldValue && newValue)
						{
							pControl->started = 1;
							ctx.phase = SG_INPUT_ACTION_PHASE_STARTED;
							if (pDesc->pFunction)
								executeNext = pDesc->pFunction(&ctx) && executeNext;
						}
						// action performed
						if (pControl->started && newValue && !pControl->performed[index])
						{
							pControl->performed[index] = 1;
							ctx.phase = SG_INPUT_ACTION_PHASE_PERFORMED;
							if (pDesc->pFunction)
								executeNext = pDesc->pFunction(&ctx) && executeNext;
						}
						// action canceled
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
								ctx.float2 = pControl->value;
								ctx.phase = SG_INPUT_ACTION_PHASE_CANCELED;
								if (pDesc->pFunction)
									executeNext = pDesc->pFunction(&ctx) && executeNext;
							}
							else if (pDesc->pFunction)
							{
								ctx.phase = SG_INPUT_ACTION_PHASE_PERFORMED;
								pControl->value[axis] = (float)pControl->pressedVal[axis * 2 + 0] - (float)pControl->pressedVal[axis * 2 + 1];
								ctx.float2 = pControl->value;
								executeNext = pDesc->pFunction(&ctx) && executeNext;
							}
						}

						break;
					}
					// mouse scroll is using on_device_button_bool
					case SG_CONTROL_TYPE_FLOAT:
					{
						if (!oldValue && newValue)
						{
							ASSERT(deviceButton == SG_BUTTON_MOUSE_SCROLL_UP || deviceButton == SG_BUTTON_MOUSE_SCROLL_DOWN);

							FloatControl* pControl = (FloatControl*)control;
							ctx.float2[1] = deviceButton == SG_BUTTON_MOUSE_SCROLL_UP ? 1.0f : -1.0f;
							if (pDesc->pFunction)
							{
								ctx.phase = SG_INPUT_ACTION_PHASE_PERFORMED;
								executeNext = pDesc->pFunction(&ctx) && executeNext;
							}

							mFloatDeltaControlCancelQueue.insert(pControl);
						}
					}
#if SG_TOUCH_INPUT
					case SG_CONTROL_TYPE_VIRTUAL_JOYSTICK:
					{
						VirtualJoystickControl* pControl = (VirtualJoystickControl*)control;

						if (!oldValue && newValue && !pControl->mStarted)
						{
							pControl->mStartPos = mTouchPositions[touchIndex];
							if ((AREA_LEFT == pControl->mArea && pControl->mStartPos[0] <= pInputManager->GetDisplayWidth() * 0.5f) ||
								(AREA_RIGHT == pControl->mArea && pControl->mStartPos[0] > pInputManager->GetDisplayWidth() * 0.5f))
							{
								pControl->mStarted = 0x3;
								pControl->mTouchIndex = touchIndex;
								pControl->mCurrPos = pControl->mStartPos;

								if (pDesc->pFunction)
								{
									ctx.phase = INPUT_ACTION_PHASE_STARTED;
									ctx.mVec2 = Vec2(0.0f);
									ctx.pPosition = &pControl->mCurrPos;
									executeNext = pDesc->pFunction(&ctx) && executeNext;
								}
							}
							else
							{
								pControl->mStarted = 0;
								pControl->mTouchIndex = 0xFF;
							}
						}
						else if (oldValue && !newValue)
						{
							if (pControl->mTouchIndex == touchIndex)
							{
								pControl->mIsPressed = 0;
								pControl->mTouchIndex = 0xFF;
								pControl->mStarted = 0;
								pControl->performed = 0;
								if (pDesc->pFunction)
								{
									ctx.mVec2 = Vec2(0.0f);
									ctx.phase = INPUT_ACTION_PHASE_CANCELED;
									executeNext = pDesc->pFunction(&ctx) && executeNext;
								}
							}
						}
						break;
					}
#endif
					case SG_CONTROL_TYPE_COMBO:
					{
						ComboControl* pControl = (ComboControl*)control;
						if (deviceButton == pControl->pressButton)
						{
							pControl->pressed = (uint8_t)newValue;
						}
						else if (pControl->pressed && oldValue && !newValue && pDesc->pFunction)
						{
							ctx.bool1 = true;
							ctx.phase = SG_INPUT_ACTION_PHASE_PERFORMED;
							pDesc->pFunction(&ctx);
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

		bool on_device_button_float(DeviceId device, DeviceButtonId deviceButton, float oldValue, float newValue) override
		{
#if SG_TOUCH_INPUT
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
				InputDeviceMouse* pMouse = (InputDeviceMouse*)pInputManager->get_device(mouseDeviceID);
				mMousePosition[0] = pMouse->get_float(SG_MOUSE_AXIS_X);
				mMousePosition[1] = pMouse->get_float(SG_MOUSE_AXIS_Y);
			}
#endif

			if (mControls[device].size())
			{
				bool executeNext = true;

				const eastl::vector<IControl*>& controls = mControls[device][deviceButton];
				for (IControl* control : controls)
				{
					if (!executeNext)
						return true;

					const InputControlType type = control->type;
					const InputActionDesc* pDesc = &control->pAction->desc;
					InputActionContext ctx = {};
					ctx.deviceType = pDeviceTypes[device];
					ctx.pUserData = pDesc->pUserData;
					ctx.pCaptured = is_pointer_type(device) ? &isInputCaptured : &useDefaultCapture;

					switch (type)
					{
					case SG_CONTROL_TYPE_FLOAT:
					{
						FloatControl* pControl = (FloatControl*)control;
						const InputActionDesc* pDesc = &pControl->pAction->desc;
						const uint32_t axis = (deviceButton - pControl->startButton);

						if (pControl->delta & 0x1)
						{
							pControl->value[axis] += (axis > 0 ? -1.0f : 1.0f) * (newValue - oldValue);
							ctx.float3 = pControl->value;

							if (((pControl->started >> axis) & 0x1) == 0)
							{
								pControl->started |= (1 << axis);
								if (pControl->started == pControl->target && pDesc->pFunction)
								{
									ctx.phase = SG_INPUT_ACTION_PHASE_STARTED;
									executeNext = pDesc->pFunction(&ctx) && executeNext;
								}

								mFloatDeltaControlCancelQueue.insert(pControl);
							}

							pControl->performed |= (1 << axis);

							if (pControl->performed == pControl->target)
							{
								pControl->performed = 0;
								ctx.phase = SG_INPUT_ACTION_PHASE_PERFORMED;
								if (pDesc->pFunction)
									executeNext = pDesc->pFunction(&ctx) && executeNext;
							}
						}
						else if (pDesc->pFunction)
						{
							pControl->performed |= (1 << axis);
							pControl->value[axis] = newValue;
							if (pControl->performed == pControl->target)
							{
								ctx.phase = SG_INPUT_ACTION_PHASE_PERFORMED;
								pControl->performed = 0;
								ctx.float3 = pControl->value;
								executeNext = pDesc->pFunction(&ctx) && executeNext;
							}
						}
						break;
					}
					case SG_CONTROL_TYPE_AXIS:
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

						if (pControl->performed == pControl->target && pDesc->pFunction)
						{
							ctx.phase = SG_INPUT_ACTION_PHASE_PERFORMED;
							ctx.float3 = pControl->value;
							executeNext = pDesc->pFunction(&ctx) && executeNext;
						}

						if (pControl->performed != pControl->target)
							continue;

						pControl->performed = 0;
						break;
					}
#if SG_TOUCH_INPUT
					case CONTROL_VIRTUAL_JOYSTICK:
					{
						VirtualJoystickControl* pControl = (VirtualJoystickControl*)control;

						const uint32_t axis = TOUCH_AXIS(deviceButton);

						if (!pControl->mStarted || TOUCH_USER(deviceButton) != pControl->mTouchIndex)
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

							ctx.phase = SG_INPUT_ACTION_PHASE_PERFORMED;
							Vec2 dir = ((pControl->mCurrPos - pControl->mStartPos) / halfRad) * pControl->mScale;
							ctx.mVec2 = Vec2(dir[0], -dir[1]);
							ctx.pPosition = &pControl->mCurrPos;
							if (pDesc->pFunction)
								executeNext = pDesc->pFunction(&ctx) && executeNext;
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

//		bool on_device_button_gesture(gainput::DeviceId device, gainput::DeviceButtonId deviceButton, const struct gainput::GestureChange& gesture)
//		{
//#if defined(TARGET_IOS)
//			const InputActionDesc* pDesc = &mGestureControls[deviceButton]->mDesc;
//			if (pDesc->pFunction)
//			{
//				InputActionContext ctx = {};
//				ctx.pUserData = pDesc->pUserData;
//				ctx.mDeviceType = pDeviceTypes[device];
//				ctx.pPosition = (Vec2*)gesture.position;
//				if (gesture.type == gainput::GesturePan)
//				{
//					ctx.mVec2 = { gesture.translation[0], gesture.translation[1] };
//				}
//				else if (gesture.type == gainput::GesturePinch)
//				{
//					ctx.mFloat4 =
//					{
//						gesture.velocity,
//						gesture.scale,
//						gesture.distance[0],
//						gesture.distance[1]
//					};
//				}
//
//				ctx.phase = SG_INPUT_ACTION_PHASE_PERFORMED;
//				pDesc->pFunction(&ctx);
//			}
//#endif
//
//			return true;
//		}

		int get_priority() const
		{
			return 0;
		}

		//void set_dead_zone(unsigned int controllerIndex, float deadZoneSize)
		//{
		//	if (controllerIndex >= MAX_INPUT_GAMEPADS)
		//		return;
		//	InputDevicePad* pDevicePad = (InputDevicePad*)pInputManager->get_device(pGamepadDeviceIDs[controllerIndex]);
		//	pDevicePad->SetDeadZone(PadButton::PadButtonL3, deadZoneSize);
		//	pDevicePad->SetDeadZone(PadButton::PadButtonR3, deadZoneSize);
		//	pDevicePad->SetDeadZone(PadButton::PadButtonL2, deadZoneSize);
		//	pDevicePad->SetDeadZone(PadButton::PadButtonR2, deadZoneSize);
		//	pDevicePad->SetDeadZone(PadButton::PadButtonLeftStickX, deadZoneSize);
		//	pDevicePad->SetDeadZone(PadButton::PadButtonLeftStickY, deadZoneSize);
		//	pDevicePad->SetDeadZone(PadButton::PadButtonRightStickX, deadZoneSize);
		//	pDevicePad->SetDeadZone(PadButton::PadButtonRightStickY, deadZoneSize);
		//	pDevicePad->SetDeadZone(PadButton::PadButtonAxis4, deadZoneSize);
		//	pDevicePad->SetDeadZone(PadButton::PadButtonAxis5, deadZoneSize);
		//}

		//const char* get_gamepad_name(unsigned gamePadIndex)
		//{
		//	if (gamePadIndex >= MAX_INPUT_GAMEPADS)
		//		return "Incorrect gamePadIndex";
		//	gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
		//	//return pDevicePad->GetDeviceName();
		//	return "Gamepad Name not support!";
		//}

		//bool is_gamepad_connected(unsigned int gamePadIndex)
		//{
		//	if (gamePadIndex >= MAX_INPUT_GAMEPADS)
		//		return false;
		//	gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
		//	return pDevicePad->IsAvailable();
		//}

		//void set_on_device_change_callback(void(*onDeviceChnageCallBack)(const char* name, bool added), unsigned int gamePadIndex)
		//{
		//	if (gamePadIndex >= MAX_INPUT_GAMEPADS)
		//		return;
		//	gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
		//	pDevicePad->SetOnDeviceChangeCallBack(onDeviceChnageCallBack);
		//}
	};

	static InputSystemImpl* pInputSystem = nullptr;

#if defined(SG_PLATFORM_WINDOWS) && !defined(XBOX)
	static void reset_input_states()
	{
		pInputSystem->pInputManager->clear_all_states(pInputSystem->mouseDeviceID);
		pInputSystem->pInputManager->clear_all_states(pInputSystem->keyboardDeviceID);
		//for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
		//{
		//	pInputSystem->pInputManager->ClearAllStates(pInputSystem->pGamepadDeviceIDs[i]);
		//}
	}
#endif

	int32_t input_system_handle_message(WindowDesc* pWindow, void* msg)
	{
		if (pInputSystem == nullptr)
		{
			return 0;
		}
#if defined(SG_PLATFORM_WINDOWS) && !defined(XBOX)
		pInputSystem->pInputManager->HandleMessage(*(MSG*)msg);
		if ((*(MSG*)msg).message == WM_ACTIVATEAPP && (*(MSG*)msg).wParam == WA_INACTIVE)
		{
			reset_input_states();
		}
#endif
		return 0;
	}

	bool init_input_system(WindowDesc* pWindow)
	{
		pInputSystem = sg_new(InputSystemImpl);

		set_custom_message_processor(input_system_handle_message);

		return pInputSystem->init(pWindow);
	}

	void exit_input_system()
	{
		ASSERT(pInputSystem);

		set_custom_message_processor(nullptr);

		pInputSystem->exit();
		sg_delete(pInputSystem);
		pInputSystem = nullptr;
	}

	void update_input_system(uint32_t width, uint32_t height)
	{
		ASSERT(pInputSystem);

		pInputSystem->update(width, height);
	}

	InputAction* register_input_action(const InputActionDesc* pDesc)
	{
		ASSERT(pInputSystem);

		return pInputSystem->register_input_action(pDesc);
	}

	void remove_input_action(InputAction* pAction)
	{
		ASSERT(pInputSystem);

		pInputSystem->remove_input_action(pAction);
	}

	bool set_enable_capture_input(bool enable)
	{
		ASSERT(pInputSystem);

		return pInputSystem->set_enable_capture_input(enable);
	}

	void set_virtual_keyboard(uint32_t type)
	{
		ASSERT(pInputSystem);

		pInputSystem->set_virtual_keyboard(type);
	}

	//void set_dead_zone(unsigned int controllerIndex, float deadZoneSize)
	//{
	//	ASSERT(pInputSystem);

	//	pInputSystem->set_dead_zone(controllerIndex, deadZoneSize);
	//}

	//bool set_rumble_effect(int gamePadIndex, float leftMotor, float rightMotor)
	//{
	//	ASSERT(pInputSystem);

	//	return pInputSystem->set_rumble_effect(gamePadIndex, leftMotor, rightMotor);
	//}

}