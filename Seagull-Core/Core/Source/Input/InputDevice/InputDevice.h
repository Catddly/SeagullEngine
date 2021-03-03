#pragma once

#include <stdint.h>

#define ASSERT(x) if (!(x)) __debugbreak();

namespace SG
{

	typedef uint8_t DeviceId;
	const DeviceId InvalidDeviceId = -1;

	struct InputDevice
	{
		DeviceId CreateDevice();

		DeviceId mDeviceId;
	};

}