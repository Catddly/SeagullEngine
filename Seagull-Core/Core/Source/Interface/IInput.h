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
		//typedef enum KeyBindings
		//{
		//	// Gamepad
		//	/// we do not support gamepad for now
		//	//SG_FLOAT_BINDINGS_BEGIN,
		//	//SG_FLOAT_LEFTSTICK = SG_FLOAT_BINDINGS_BEGIN,
		//	//SG_FLOAT_RIGHTSTICK,
		//	//SG_FLOAT_L2,
		//	//SG_FLOAT_R2,
		//	//SG_FLOAT_GRAVITY,
		//	//SG_FLOAT_GYROSCOPE,
		//	//SG_FLOAT_MAGNETICFIELD,
		//	//SG_FLOAT_DPAD,
		//	//SG_FLOAT_BINDINGS_END = SG_FLOAT_DPAD,

		//	//SG_BUTTON_BINDINGS_BEGIN,
		//	//SG_BUTTON_DPAD_LEFT = SG_BUTTON_BINDINGS_BEGIN,
		//	//SG_BUTTON_DPAD_RIGHT,
		//	//SG_BUTTON_DPAD_UP,
		//	//SG_BUTTON_DPAD_DOWN,
		//	//SG_BUTTON_SOUTH, // A/CROSS
		//	//SG_BUTTON_EAST, // B/CIRCLE
		//	//SG_BUTTON_WEST, // X/SQUARE
		//	//SG_BUTTON_NORTH, // Y/TRIANGLE
		//	//SG_BUTTON_L1,
		//	//SG_BUTTON_R1,
		//	//SG_BUTTON_L2,
		//	//SG_BUTTON_R2,
		//	//SG_BUTTON_L3, // LEFT THUMB
		//	//SG_BUTTON_R3, // RIGHT THUMB
		//	//SG_BUTTON_HOME, // PS BUTTON
		//	//SG_BUTTON_START,
		//	//SG_BUTTON_SELECT,
		//	//SG_BUTTON_TOUCH, //PS4 TOUCH
		//	//SG_BUTTON_BACK,
		//	//SG_BUTTON_FULLSCREEN,
		//	//SG_BUTTON_EXIT,
		//	//SG_BUTTON_DUMP,

		//	// Gesture bindings
		//	// we do not support gesture for now
		//	//SG_GESTURE_BINDINGS_BEGIN,
		//	//SG_GESTURE_TAP = SG_GESTURE_BINDINGS_BEGIN,
		//	//SG_GESTURE_PAN,
		//	//SG_GESTURE_PINCH,
		//	//SG_GESTURE_ROTATE,
		//	//SG_GESTURE_LONG_PRESS,
		//	//SG_GESTURE_BINDINGS_END = SG_GESTURE_LONG_PRESS,

		//	SG_TEXT,
		//} KeyBindings;

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
		// to restore the user class instance
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
			wchar_t text;
		};

		Vec2* pPosition;
		const bool* pCaptured;
		float scrollValue;
		uint32_t binding;
		/// What phase is the action currently in
		uint8_t phase;
		uint8_t deviceType;
	} InputActionContext;

	// when something is triggered, we call this function at the same time
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
		float deadzone;
		float outsideRadius;
		float scale;
		/// User management (which user does this action apply to)
		uint8_t userId;
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
	//void set_virtual_keyboard(uint32_t type);

	void set_dead_zone(unsigned int controllerIndex, float deadZoneSize);
	//void set_LED_color(int gamePadIndex, uint8_t r, uint8_t g, uint8_t b);
	//bool set_rumble_effect(int gamePadIndex, float leftMotor, float rightMotor);

	//const char* get_gamepad_name(int gamePadIndex);
	//bool is_game_pad_connected(int gamePadIndex);
	//void set_on_device_change_callback(void(*onDeviceChnageCallBack)(const char* name, bool added), unsigned int gamePadIndex);

} // namespace SG