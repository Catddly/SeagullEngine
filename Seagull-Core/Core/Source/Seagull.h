#pragma once

#include "Interface/IApp.h"
#include "Interface/ILog.h"
#include "Interface/IThread.h"
#include "Interface/ITime.h"
#include "Interface/IInput.h"

#include "ThreadSystem/ThreadSystem.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <include/glm.hpp>
#include <include/gtc/matrix_transform.hpp>
#include <include/ext/matrix_clip_space.hpp>

#include "Math/MathTypes.h"

#include "../../Seagull-Core/Renderer/IRenderer/Include/IRenderer.h"
#include "../../Seagull-Core/Renderer/IRenderer/Include/IResourceLoader.h"

#include "Middleware/UI/UIMiddleware.h"

#include "Interface/IMemory.h"

#ifndef SG_CUSTOM_MEMORY_MANAGEMENT
#ifndef malloc
#define malloc(size) static_assert(false, "Please use sg_malloc");
#endif
#ifndef calloc
#define calloc(count, size) static_assert(false, "Please use sg_calloc");
#endif
#ifndef memalign
#define memalign(align, size) static_assert(false, "Please use sg_memalign");
#endif
#ifndef realloc
#define realloc(ptr, size) static_assert(false, "Please use sg_realloc");
#endif
#ifndef free
#define free(ptr) static_assert(false, "Please use sg_free");
#endif
#ifndef new
#define new static_assert(false, "Please use sg_placement_new");
#endif
#ifndef delete
#define delete static_assert(false, "Please use sg_free with explicit destructor call");
#endif
#endif

//#ifdef SG_CUSTOM_MEMORY_MANAGEMENT
//#undef SG_CUSTOM_MEMORY_MANAGEMENT
//#endif