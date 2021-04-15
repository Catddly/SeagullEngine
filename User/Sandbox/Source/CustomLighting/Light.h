#pragma once

#include "Seagull.h"

namespace SG
{

	struct Light
	{
		Vec3  position;
		float intensity;
		Vec3  color;
	};

	Vec4 UintToVec4Color(uint32_t color)
	{
		Vec4 col;    // Translate colors back by bit shifting
		col.r = (float)((color & 0xff000000) >> 24);
		col.g = (float)((color & 0x00ff0000) >> 16);
		col.b = (float)((color & 0x0000ff00) >> 8);
		col.a = (float)(color & 0x000000ff);
		return col;
	}

}