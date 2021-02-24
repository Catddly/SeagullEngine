#pragma once

#include "Interface/IApp.h"
#include "Interface/ILog.h"
#include "Interface/IThread.h"
#include "Interface/IMemory.h"
#include "Interface/ITime.h"

#include "Math/MathTypes.h"

#ifdef SG_GRAPHIC_API_VULKAN
	#include "../../Seagull-Core/Renderer/Vulkan/include/vulkan/vulkan_core.h"
#endif

#include "../../Seagull-Core/Renderer/IRenderer/Include/IRenderer.h"
#include "../../Seagull-Core/Renderer/IRenderer/Include/IResourceLoader.h"
