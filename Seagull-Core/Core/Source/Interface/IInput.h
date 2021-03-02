#pragma once

#include <stdint.h>

#include "Math/MathTypes.h"

#include "Interface/IOperatingSystem.h"

namespace SG
{

	extern uint32_t MAX_INPUT_MULTI_TOUCHES;
	extern uint32_t MAX_INPUT_GAMEPADS;
	extern uint32_t MAX_INPUT_ACTIONS;

	typedef struct InputBindings
	{
		typedef enum KeyBindings
		{
			// Gamepad
			SG_FLOAT_BINDINGS_BEGIN,
			SG_FLOAT_LEFTSTICK = SG_FLOAT_BINDINGS_BEGIN,
			SG_FLOAT_RIGHTSTICK,
			SG_FLOAT_L2,
			SG_FLOAT_R2,
			SG_FLOAT_GRAVITY,
			SG_FLOAT_GYROSCOPE,
			SG_FLOAT_MAGNETICFIELD,
			SG_FLOAT_DPAD,
			SG_FLOAT_BINDINGS_END = SG_FLOAT_DPAD,

			SG_BUTTON_BINDINGS_BEGIN,
			SG_BUTTON_DPAD_LEFT = SG_BUTTON_BINDINGS_BEGIN,
			SG_BUTTON_DPAD_RIGHT,
			SG_BUTTON_DPAD_UP,
			SG_BUTTON_DPAD_DOWN,
			SG_BUTTON_SOUTH, // A/CROSS
			SG_BUTTON_EAST, // B/CIRCLE
			SG_BUTTON_WEST, // X/SQUARE
			SG_BUTTON_NORTH, // Y/TRIANGLE
			SG_BUTTON_L1,
			SG_BUTTON_R1,
			SG_BUTTON_L2,
			SG_BUTTON_R2,
			SG_BUTTON_L3, // LEFT THUMB
			SG_BUTTON_R3, // RIGHT THUMB
			SG_BUTTON_HOME, // PS BUTTON
			SG_BUTTON_START,
			SG_BUTTON_SELECT,
			SG_BUTTON_TOUCH, //PS4 TOUCH
			SG_BUTTON_BACK,
			SG_BUTTON_FULLSCREEN,
			SG_BUTTON_EXIT,
			SG_BUTTON_DUMP,

			// KeyBoard
			SG_BUTTON_KEY_ESCAPE,
			SG_BUTTON_KEY_F1,
			SG_BUTTON_KEY_F2,
			SG_BUTTON_KEY_F3,
			SG_BUTTON_KEY_F4,
			SG_BUTTON_KEY_F5,
			SG_BUTTON_KEY_F6,
			SG_BUTTON_KEY_F7,
			SG_BUTTON_KEY_F8,
			SG_BUTTON_KEY_F9,
			SG_BUTTON_KEY_F10,
			SG_BUTTON_KEY_F11,
			SG_BUTTON_KEY_F12,
			SG_BUTTON_KEY_F13,
			SG_BUTTON_KEY_F14,
			SG_BUTTON_KEY_F15,
			SG_BUTTON_KEY_F16,
			SG_BUTTON_KEY_F17,
			SG_BUTTON_KEY_F18,
			SG_BUTTON_KEY_F19,
			SG_BUTTON_KEY_PRINT,
			SG_BUTTON_KEY_SCROLLLOCK,
			SG_BUTTON_KEY_BREAK,
			SG_BUTTON_KEY_SPACE,
			SG_BUTTON_KEY_APOSTROPHE,
			SG_BUTTON_KEY_COMMA,
			SG_BUTTON_KEY_MINUS,
			SG_BUTTON_KEY_PERIOD,
			SG_BUTTON_KEY_SLASH,
			SG_BUTTON_KEY_0,
			SG_BUTTON_KEY_1,
			SG_BUTTON_KEY_2,
			SG_BUTTON_KEY_3,
			SG_BUTTON_KEY_4,
			SG_BUTTON_KEY_5,
			SG_BUTTON_KEY_6,
			SG_BUTTON_KEY_7,
			SG_BUTTON_KEY_8,
			SG_BUTTON_KEY_9,
			SG_BUTTON_KEY_SEMICOLON,
			SG_BUTTON_KEY_LESS,
			SG_BUTTON_KEY_EQUAL,
			SG_BUTTON_KEY_A,
			SG_BUTTON_KEY_B,
			SG_BUTTON_KEY_C,
			SG_BUTTON_KEY_D,
			SG_BUTTON_KEY_E,
			SG_BUTTON_KEY_F,
			SG_BUTTON_KEY_G,
			SG_BUTTON_KEY_H,
			SG_BUTTON_KEY_I,
			SG_BUTTON_KEY_J,
			SG_BUTTON_KEY_K,
			SG_BUTTON_KEY_L,
			SG_BUTTON_KEY_M,
			SG_BUTTON_KEY_N,
			SG_BUTTON_KEY_O,
			SG_BUTTON_KEY_P,
			SG_BUTTON_KEY_Q,
			SG_BUTTON_KEY_R,
			SG_BUTTON_KEY_S,
			SG_BUTTON_KEY_T,
			SG_BUTTON_KEY_U,
			SG_BUTTON_KEY_V,
			SG_BUTTON_KEY_W,
			SG_BUTTON_KEY_X,
			SG_BUTTON_KEY_Y,
			SG_BUTTON_KEY_Z,
			SG_BUTTON_KEY_BRACKETLEFT,
			SG_BUTTON_KEY_BACKSLASH,
			SG_BUTTON_KEY_BRACKETRIGHT,
			SG_BUTTON_KEY_GRAVE,
			SG_BUTTON_KEY_LEFT,
			SG_BUTTON_KEY_RIGHT,
			SG_BUTTON_KEY_UP,
			SG_BUTTON_KEY_DOWN,
			SG_BUTTON_KEY_INSERT,
			SG_BUTTON_KEY_HOME,
			SG_BUTTON_KEY_DELETE,
			SG_BUTTON_KEY_END,
			SG_BUTTON_KEY_PAGEUP,
			SG_BUTTON_KEY_PAGEDOWN,
			SG_BUTTON_KEY_NUMLOCK,
			SG_BUTTON_KEY_KPEQUAL,
			SG_BUTTON_KEY_KPDIVIDE,
			SG_BUTTON_KEY_KPMULTIPLY,
			SG_BUTTON_KEY_KPSUBTRACT,
			SG_BUTTON_KEY_KPADD,
			SG_BUTTON_KEY_KPENTER,
			SG_BUTTON_KEY_KPINSERT, // 0
			SG_BUTTON_KEY_KPEND, // 1
			SG_BUTTON_KEY_KPDOWN, // 2
			SG_BUTTON_KEY_KPPAGEDOWN, // 3
			SG_BUTTON_KEY_KPLEFT, // 4
			SG_BUTTON_KEY_KPBEGIN, // 5
			SG_BUTTON_KEY_KPRIGHT, // 6
			SG_BUTTON_KEY_KPHOME, // 7
			SG_BUTTON_KEY_KPUP, // 8
			SG_BUTTON_KEY_KPPAGEUP, // 9
			SG_BUTTON_KEY_KPDELETE, // ,
			SG_BUTTON_KEY_BACKSPACE,
			SG_BUTTON_KEY_TAB,
			SG_BUTTON_KEY_RETURN,
			SG_BUTTON_KEY_CAPSLOCK,
			SG_BUTTON_KEY_SHIFTL,
			SG_BUTTON_KEY_CTRLL,
			SG_BUTTON_KEY_SUPERL,
			SG_BUTTON_KEY_ALTL,
			SG_BUTTON_KEY_ALTR,
			SG_BUTTON_KEY_SUPERR,
			SG_BUTTON_KEY_MENU,
			SG_BUTTON_KEY_CTRLR,
			SG_BUTTON_KEY_SHIFTR,
			SG_BUTTON_KEY_BACK,
			SG_BUTTON_KEY_SOFTLEFT,
			SG_BUTTON_KEY_SOFTRIGHT,
			SG_BUTTON_KEY_CALL,
			SG_BUTTON_KEY_ENDCALL,
			SG_BUTTON_KEY_STAR,
			SG_BUTTON_KEY_POUND,
			SG_BUTTON_KEY_DPADCENTER,
			SG_BUTTON_KEY_VOLUMEUP,
			SG_BUTTON_KEY_VOLUMEDOWN,
			SG_BUTTON_KEY_POWER,
			SG_BUTTON_KEY_CAMERA,
			SG_BUTTON_KEY_CLEAR,
			SG_BUTTON_KEY_SYMBOL,
			SG_BUTTON_KEY_EXPLORER,
			SG_BUTTON_KEY_ENVELOPE,
			SG_BUTTON_KEY_EQUALS,
			SG_BUTTON_KEY_AT,
			SG_BUTTON_KEY_HEADSETHOOK,
			SG_BUTTON_KEY_FOCUS,
			SG_BUTTON_KEY_PLUS,
			SG_BUTTON_KEY_NOTIFICATION,
			SG_BUTTON_KEY_SEARCH,
			SG_BUTTON_KEY_MEDIAPLAYPAUSE,
			SG_BUTTON_KEY_MEDIASTOP,
			SG_BUTTON_KEY_MEDIANEXT,
			SG_BUTTON_KEY_MEDIAPREVIOUS,
			SG_BUTTON_KEY_MEDIAREWIND,
			SG_BUTTON_KEY_MEDIAFASTFORWARD,
			SG_BUTTON_KEY_MUTE,
			SG_BUTTON_KEY_PICTSYMBOLS,
			SG_BUTTON_KEY_SWITCHCHARSET,
			SG_BUTTON_KEY_FORWARD,
			SG_BUTTON_KEY_EXTRA1,
			SG_BUTTON_KEY_EXTRA2,
			SG_BUTTON_KEY_EXTRA3,
			SG_BUTTON_KEY_EXTRA4,
			SG_BUTTON_KEY_EXTRA5,
			SG_BUTTON_KEY_EXTRA6,
			SG_BUTTON_KEY_FN,
			SG_BUTTON_KEY_CIRCUMFLEX,
			SG_BUTTON_KEY_SSHARP,
			SG_BUTTON_KEY_ACUTE,
			SG_BUTTON_KEY_ALTGR,
			SG_BUTTON_KEY_NUMBERSIGN,
			SG_BUTTON_KEY_UDIAERESIS,
			SG_BUTTON_KEY_ADIAERESIS,
			SG_BUTTON_KEY_ODIAERESIS,
			SG_BUTTON_KEY_SECTION,
			SG_BUTTON_KEY_ARING,
			SG_BUTTON_KEY_DIAERESIS,
			SG_BUTTON_KEY_TWOSUPERIOR,
			SG_BUTTON_KEY_RIGHTPARENTHESIS,
			SG_BUTTON_KEY_DOLLAR,
			SG_BUTTON_KEY_UGRAVE,
			SG_BUTTON_KEY_ASTERISK,
			SG_BUTTON_KEY_COLON,
			SG_BUTTON_KEY_EXCLAM,
			SG_BUTTON_KEY_BRACELEFT,
			SG_BUTTON_KEY_BRACERIGHT,
			SG_BUTTON_KEY_SYSRQ,

			// Mouse bindings
			SG_BUTTON_MOUSE_LEFT,
			SG_BUTTON_MOUSE_RIGHT,
			SG_BUTTON_MOUSE_MIDDLE,
			SG_BUTTON_MOUSE_SCROLL_UP,
			SG_BUTTON_MOUSE_SCROLL_DOWN,
			SG_BUTTON_MOUSE_SCROLL_LEFT,
			SG_BUTTON_MOUSE_SCROLL_RIGHT,
			SG_BUTTON_MOUSE_5,
			SG_BUTTON_MOUSE_6,
			SG_BUTTON_MOUSE_7,

			SG_BUTTON_ANY,
			SG_BUTTON_BINDINGS_END = SG_BUTTON_ANY,

			// Gesture bindings
			SG_GESTURE_BINDINGS_BEGIN,
			SG_GESTURE_TAP = SG_GESTURE_BINDINGS_BEGIN,
			SG_GESTURE_PAN,
			SG_GESTURE_PINCH,
			SG_GESTURE_ROTATE,
			SG_GESTURE_LONG_PRESS,
			SG_GESTURE_BINDINGS_END = SG_GESTURE_LONG_PRESS,
			SG_TEXT,
		} KeyBindings;

		typedef struct GestureDesc
		{
			/// Configuring Pan gesture
			uint32_t minNumberOfTouches;
			uint32_t maxNumberOfTouches;
			/// Configuring Tap gesture (single tap, double tap, ...)
			uint32_t numberOfTapsRequired;
			/// Configuring Long press gesture
			float    minimumPressDuration;
		} GestureDesc;

	} InputBindings;

	typedef enum InputDeviceType
	{
		SG_INPUT_DEVICE_INVALID = 0,
		SG_INPUT_DEVICE_GAMEPAD,
		SG_INPUT_DEVICE_TOUCH,
		SG_INPUT_DEVICE_KEYBOARD,
		SG_INPUT_DEVICE_MOUSE,
	} InputDeviceType;

	typedef enum InputActionPhase
	{
		/// Action is initiated
		SG_INPUT_ACTION_PHASE_STARTED = 0,
		/// Example: mouse delta changed, key pressed, ...
		SG_INPUT_ACTION_PHASE_PERFORMED,
		/// Example: left mouse button was pressed and now released, gesture was started but got canceled
		SG_INPUT_ACTION_PHASE_CANCELED,
	} InputActionPhase;

	typedef struct InputActionContext
	{
		void* pUserData;
		union
		{
			/// Gesture input
			Vec4 float4;
			/// 3D input (gyroscope, ...)
			Vec3 float3;
			/// 2D input (mouse position, delta, composite input (wasd), gamepad stick, joystick, ...)
			Vec2 float2;
			/// 1D input (composite input (ws), gamepad left trigger, ...)
			float float1;
			/// Button input (mouse left button, keyboard keys, ...)
			bool bool1;
			/// Text input
			char text;
		};

		Vec2* pPosition;
		const bool* pCaptured;
		float scrollValue;
		uint32_t binding;
		/// What phase is the action currently in
		uint8_t phase;
		uint8_t deviceType;
	} InputActionContext;

	typedef bool (*InputActionCallback)(InputActionContext* pContext);

	typedef struct InputActionDesc
	{
		/// Value from InputBindings::keyBindings enum
		uint32_t binding;
		/// Callback when an action is initiated, performed or canceled
		InputActionCallback pFunction;
		/// User data which will be assigned to InputActionContext::pUserData when calling pFunction
		void* pUserData;
		/// Virtual joystick
		float                       deadzone;
		float                       outsideRadius;
		float                       scale;
		/// User management (which user does this action apply to)
		uint8_t                     userId;
		/// Gesture desc
		InputBindings::GestureDesc* pGesture;
	} InputActionDesc;

	typedef struct InputAction InputAction;

	bool init_input_system(WindowDesc* pWindow);
	void exit_input_system();

	void update_input_system(uint32_t width, uint32_t height);

	InputAction* register_input_action(const InputActionDesc* pDesc);
	void remove_input_action(InputAction* pAction);

	bool set_enable_capture_input(bool enable);
	/// Used to enable/disable text input for non-keyboard setups (virtual keyboards for console/mobile, ...) (a pop out keyboard)
	void set_virtual_keyboard(uint32_t type);

	void set_dead_zone(unsigned int controllerIndex, float deadZoneSize);
	//void set_LED_color(int gamePadIndex, uint8_t r, uint8_t g, uint8_t b);
	//bool set_rumble_effect(int gamePadIndex, float leftMotor, float rightMotor);

	//const char* get_gamepad_name(int gamePadIndex);
	//bool is_game_pad_connected(int gamePadIndex);
	//void set_on_device_change_callback(void(*onDeviceChnageCallBack)(const char* name, bool added), unsigned int gamePadIndex);

} // namespace SG