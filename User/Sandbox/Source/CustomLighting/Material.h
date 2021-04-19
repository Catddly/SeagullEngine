#pragma once

#include "Seagull.h"

namespace SG
{

	struct SG_ALIGN(MaterialData, 16)
	{
		Vec3  color = { 1.0f, 1.0f, 1.0f };
		float metallic = 0.8;
		float smoothness = 0.5;
	};

}