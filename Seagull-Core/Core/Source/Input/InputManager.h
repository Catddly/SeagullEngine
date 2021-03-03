#pragma once

#include "InputDevice/InputDeviceMouse.h"
#include "InputDevice/InputDeviceKeyboard.h"

#include <include/EASTL/type_traits.h>

namespace SG
{

	class InputManager
	{
	public:
		template <typename T>
		DeviceId CreateDevice();
	};

	template<typename T>
	inline DeviceId InputManager::CreateDevice()
	{
		ASSERT(eastl::is_base_of_v<InputDevice, T>());

		T device;
		return device->CreateDevice();
		return 0;
	}

}