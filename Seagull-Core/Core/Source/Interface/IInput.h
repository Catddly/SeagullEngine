#include <stdint.h>

#include "BasicType/ISingleton.h"
#include "Interface/IOperatingSystem.h"

#include <include/EASTL/utility.h>

namespace SG
{
	
	enum ButtonBindings
	{
		SG_KEY_ESCAPE,
		SG_KEY_FULLSCREEN,
		SG_KEY_DUMP,
		SG_KEY_F1,
		SG_KEY_F2,
		SG_KEY_F3,
		SG_KEY_F4,
		SG_KEY_F5,
		SG_KEY_F6,
		SG_KEY_F7,
		SG_KEY_F8,
		SG_KEY_F9,
		SG_KEY_F10,
		SG_KEY_F11,
		SG_KEY_F12,
		SG_KEY_F13,
		SG_KEY_F14,
		SG_KEY_F15,
		SG_KEY_F16,
		SG_KEY_F17,
		SG_KEY_F18,
		SG_KEY_F19,
		SG_KEY_PRINT,
		SG_KEY_SCROLLLOCK,
		SG_KEY_BREAK,
		SG_KEY_SPACE,
		SG_KEY_APOSTROPHE,
		SG_KEY_COMMA,
		SG_KEY_MINUS,
		SG_KEY_PERIOD,
		SG_KEY_SLASH,
		SG_KEY_0,
		SG_KEY_1,
		SG_KEY_2,
		SG_KEY_3,
		SG_KEY_4,
		SG_KEY_5,
		SG_KEY_6,
		SG_KEY_7,
		SG_KEY_8,
		SG_KEY_9,
		SG_KEY_SEMICOLON,
		SG_KEY_LESS,
		SG_KEY_EQUAL,
		SG_KEY_A,
		SG_KEY_B,
		SG_KEY_C,
		SG_KEY_D,
		SG_KEY_E,
		SG_KEY_F,
		SG_KEY_G,
		SG_KEY_H,
		SG_KEY_I,
		SG_KEY_J,
		SG_KEY_K,
		SG_KEY_L,
		SG_KEY_M,
		SG_KEY_N,
		SG_KEY_O,
		SG_KEY_P,
		SG_KEY_Q,
		SG_KEY_R,
		SG_KEY_S,
		SG_KEY_T,
		SG_KEY_U,
		SG_KEY_V,
		SG_KEY_W,
		SG_KEY_X,
		SG_KEY_Y,
		SG_KEY_Z,
		SG_KEY_BRACKETLEFT,
		SG_KEY_BACKSLASH,
		SG_KEY_BRACKETRIGHT,
		SG_KEY_GRAVE,
		SG_KEY_LEFT,
		SG_KEY_RIGHT,
		SG_KEY_UP,
		SG_KEY_DOWN,
		SG_KEY_INSERT,
		SG_KEY_HOME,
		SG_KEY_DELETE,
		SG_KEY_END,
		SG_KEY_PAGEUP,
		SG_KEY_PAGEDOWN,
		SG_KEY_NUMLOCK,
		SG_KEY_KPEQUAL,
		SG_KEY_KPDIVIDE,
		SG_KEY_KPMULTIPLY,
		SG_KEY_KPSUBTRACT,
		SG_KEY_KPADD,
		SG_KEY_KPENTER,
		SG_KEY_KPINSERT, // 0
		SG_KEY_KPEND, // 1
		SG_KEY_KPDOWN, // 2
		SG_KEY_KPPAGEDOWN, // 3
		SG_KEY_KPLEFT, // 4
		SG_KEY_KPBEGIN, // 5
		SG_KEY_KPRIGHT, // 6
		SG_KEY_KPHOME, // 7
		SG_KEY_KPUP, // 8
		SG_KEY_KPPAGEUP, // 9
		SG_KEY_KPDELETE, // ,
		SG_KEY_BACKSPACE,
		SG_KEY_TAB,
		SG_KEY_RETURN,
		SG_KEY_CAPSLOCK,
		SG_KEY_SHIFTL,
		SG_KEY_CTRLL,
		SG_KEY_SUPERL,
		SG_KEY_ALTL,
		SG_KEY_ALTR,
		SG_KEY_SUPERR,
		SG_KEY_MENU,
		SG_KEY_CTRLR,
		SG_KEY_SHIFTR,
		SG_KEY_BACK,
		SG_KEY_SOFTLEFT,
		SG_KEY_SOFTRIGHT,
		SG_KEY_CALL,
		SG_KEY_ENDCALL,
		SG_KEY_STAR,
		SG_KEY_POUND,
		SG_KEY_DPADCENTER,
		SG_KEY_VOLUMEUP,
		SG_KEY_VOLUMEDOWN,
		SG_KEY_POWER,
		SG_KEY_CAMERA,
		SG_KEY_CLEAR,
		SG_KEY_SYMBOL,
		SG_KEY_EXPLORER,
		SG_KEY_ENVELOPE,
		SG_KEY_EQUALS,
		SG_KEY_AT,
		SG_KEY_HEADSETHOOK,
		SG_KEY_FOCUS,
		SG_KEY_PLUS,
		SG_KEY_NOTIFICATION,
		SG_KEY_SEARCH,
		SG_KEY_MEDIAPLAYPAUSE,
		SG_KEY_MEDIASTOP,
		SG_KEY_MEDIANEXT,
		SG_KEY_MEDIAPREVIOUS,
		SG_KEY_MEDIAREWIND,
		SG_KEY_MEDIAFASTFORWARD,
		SG_KEY_MUTE,
		SG_KEY_PICTSYMBOLS,
		SG_KEY_SWITCHCHARSET,
		SG_KEY_FORWARD,
		SG_KEY_EXTRA1,
		SG_KEY_EXTRA2,
		SG_KEY_EXTRA3,
		SG_KEY_EXTRA4,
		SG_KEY_EXTRA5,
		SG_KEY_EXTRA6,
		SG_KEY_FN,
		SG_KEY_CIRCUMFLEX,
		SG_KEY_SSHARP,
		SG_KEY_ACUTE,
		SG_KEY_ALTGR,
		SG_KEY_NUMBERSIGN,
		SG_KEY_UDIAERESIS,
		SG_KEY_ADIAERESIS,
		SG_KEY_ODIAERESIS,
		SG_KEY_SECTION,
		SG_KEY_ARING,
		SG_KEY_DIAERESIS,
		SG_KEY_TWOSUPERIOR,
		SG_KEY_RIGHTPARENTHESIS,
		SG_KEY_DOLLAR,
		SG_KEY_UGRAVE,
		SG_KEY_ASTERISK,
		SG_KEY_COLON,
		SG_KEY_EXCLAM,
		SG_KEY_BRACELEFT,
		SG_KEY_BRACERIGHT,
		SG_KEY_SYSRQ,
		SG_KEY_COUNT,

		SG_MOUSE_LEFT = SG_KEY_COUNT,
		SG_MOUSE_RIGHT,
		SG_MOUSE_MIDDLE,
		SG_MOUSE_SCROLL_UP,
		SG_MOUSE_SCROLL_DOWN,
		SG_MOUSE_SCROLL_LEFT,
		SG_MOUSE_SCROLL_RIGHT,
		SG_MOUSE_5,
		SG_MOUSE_6,
		SG_MOUSE_7,
		SG_MOUSE_COUNT = SG_MOUSE_7 - SG_KEY_COUNT + 1,

		SG_BUTTON_ANY = SG_MOUSE_7 + 1,

		SG_GESTURE_BINDINGS_BEGIN,
		SG_GESTURE_TAP = SG_GESTURE_BINDINGS_BEGIN,
		SG_GESTURE_PAN,
		SG_GESTURE_PINCH,
		SG_GESTURE_ROTATE,
		SG_GESTURE_LONG_PRESS,
		SG_GESTURE_BINDINGS_END = SG_GESTURE_LONG_PRESS,

		SG_TEXT
	};

	typedef struct GestureDesc
	{
		/// Configuring Pan gesture
		uint32_t    minNumberOfTouches;
		uint32_t    maxNumberOfTouches;
		/// Configuring Tap gesture (single tap, double tap, ...)
		uint32_t    numberOfTapsRequired;
		/// Configuring Long press gesture
		float       minimumPressDuration;

		uint32_t    triggerBinding;
	} GestureDesc;

	typedef enum InputDeviceType
	{
		SG_INPUT_DEVICE_INVALID = 0,
		SG_INPUT_DEVICE_GAMEPAD,
		SG_INPUT_DEVICE_KEYBOARD,
		SG_INPUT_DEVICE_MOUSE,
	} InputDeviceType;

	typedef enum InputActionPhase
	{
		/// Action is initiated
		INPUT_ACTION_PHASE_STARTED = 0,
		/// Example: mouse delta changed, key pressed, ...
		INPUT_ACTION_PHASE_PERFORMED,
		/// Example: left mouse button was pressed and now released, gesture was started but got canceled
		INPUT_ACTION_PHASE_CANCELED
	} InputActionPhase;

	typedef struct InputActionContext
	{	
		/// you can set this to use the data in the callback function
		void* pUserData;
		uint32_t bindings;
		union 
		{
			float    value;
			/// positions or joystick
			Vec2     value2;
			Vec3     value3;
			/// gesture inputs
			Vec4     value4;
			/// the state of the buttons
			bool     isPressed;
			/// text input
			wchar_t* text;
		};
		Vec2*       pPosition;
		const bool* pCaptured;
		float		scrollValue;

		/// What phase is the action currently in
		/// phase is the usage of the states
		uint8_t     phase;
		uint8_t     deviceType;
	} InputActionContext;

	typedef bool (*InputActionCallback)(InputActionContext* pContext);

	typedef struct InputActionDesc
	{
		/// value for the binding code
		uint32_t bindings;
		/// callback when the button state is changed
		InputActionCallback callback;
		void*    pUserData;
		/// register Gesture
		GestureDesc pGesture;
		/// which device to apply
		uint8_t  userId;
	} InputActionDesc;

	struct InputAction
	{
		InputActionDesc desc;
	};

	bool init_input_system(WindowDesc* pWindow);
	void exit_input_system();
	void update_input_system(uint32_t width, uint32_t height);
	InputAction* add_input_action(const InputActionDesc* pDesc);
	void remove_input_action(InputAction* pAction);

	/// to capture which window and block other windows event
	bool set_enable_capture_input(bool enable);

	//void set_on_device_change_callback(void(*onDeviceChnageCallBack)(const char* name, bool added), unsigned int gamePadIndex);

}