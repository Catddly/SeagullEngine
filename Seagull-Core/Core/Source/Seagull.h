#pragma once

#include "Interface/IApp.h"
#include "Interface/ILog.h"
#include "Interface/IThread.h"
#include "Interface/ITime.h"
#include "Interface/IInput.h"

#include "Middleware/UI/UIMiddleware.h"

#include "ThreadSystem/ThreadSystem.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <include/glm.hpp>
#include <include/gtc/matrix_transform.hpp>
#include <include/ext/matrix_clip_space.hpp>

#include "Math/MathTypes.h"

#ifdef SG_GRAPHIC_API_VULKAN
	#include "../../Seagull-Core/Renderer/Vulkan/include/vulkan/vulkan_core.h"
#endif

#include "../../Seagull-Core/Renderer/IRenderer/Include/IRenderer.h"
#include "../../Seagull-Core/Renderer/IRenderer/Include/IResourceLoader.h"

#include "Interface/IMemory.h"