#define SG_RENDERER_IMPLEMENTATION
#define VMA_IMPLEMENTATION

//#define SG_USE_RENDER_DOC

#include "Core/CompilerConfig.h"
#include "Core/Atomic.h"

#include <include/tinyimageformat_query.h>

#include "IRenderer.h"

#include "Interface/ILog.h"
#include "Interface/IThread.h"

#if defined(SG_PLATFORM_WINDOWS)
// pull in minimal Windows headers
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif
#include <vulkanMemoryAllocator/vk_mem_alloc.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <include/EASTL/vector.h>
#include <include/EASTL/string_hash_map.h>
#include <include/EASTL/sort.h>

// GPUConfig.h still have some issues on <regex>
#include "Core/GPUConfig.h"
#include "VulkanCapsBuilder.h"
//#include "HelperFunc.h"

#include "Interface/IMemory.h"

#ifdef SG_DEBUG
#define SG_ENABLE_GRAPHICS_DEBUG
#endif

//#undef SG_ENABLE_GRAPHICS_DEBUG

#define SG_SAFE_FREE(ptr)      \
	if (ptr)                   \
	{                          \
		sg_free((void*)ptr);   \
	}

#ifdef __cplusplus
	#define SG_DECLARE_ZERO(type, var) type var = {};
#else
	#define SG_DECLARE_ZERO(type, var) type var = { 0 };
#endif

#define SG_CHECK_VKRESULT(exp)                                                 \
{                                                                              \
	VkResult vkres = (exp);                                                    \
	if (VK_SUCCESS != vkres)                                                   \
	{                                                                          \
		SG_LOG_ERROR("%s: FAILED with VkResult: %u", #exp, (uint32_t)vkres);   \
		ASSERT(false);                                                         \
	}                                                                          \
}																		

// indirect drawing in vulkan
//#if defined(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME)
//	PFN_vkCmdDrawIndirectCountKHR pfnVkCmdDrawIndirectCountKHR = nullptr;
//	PFN_vkCmdDrawIndexedIndirectCountKHR pfnVkCmdDrawIndexedIndirectCountKHR = nullptr;
//#else
//	PFN_vkCmdDrawIndirectCountAMD pfnVkCmdDrawIndirectCountKHR = nullptr;
//	PFN_vkCmdDrawIndexedIndirectCountAMD pfnVkCmdDrawIndexedIndirectCountKHR = nullptr;
//#endif

#pragma region (Helper Function)

static inline uint32_t round_up(uint32_t value, uint32_t multiple) { return ((value + multiple - 1) / multiple) * multiple; }
static inline uint32_t round_down(uint32_t value, uint32_t multiple) { return value - value % multiple; }

template <typename T>
static inline size_t sg_mem_hash(const T* mem, size_t size, size_t prev = 2166136261U, uint8_t dummy = 0)
{
	UNREF_PARAM(dummy);
	uint32_t result = (uint32_t)prev; // Intentionally uint32_t instead of size_t, so the behavior is the same
									  // regardless of size.
	while (size--)
		result = (result * 16777619) ^ *mem++; // FNV hash
	return (size_t)result;
}

#pragma endregion (Helper Function)

#if defined(SG_RENDERER_IMPLEMENTATION)

namespace SG
{

#pragma region (Extern Functions)

	extern void vk_create_shader_reflection(const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, ShaderReflection* pOutReflection);

#ifdef SG_ENABLE_RAYTRACING
	extern void add_raytracing_pipeline(const PipelineDesc*, Pipeline**);
	// extern void vk_fill_raytracing_descriptor_data(const AccelerationStructure* pAccelerationStructure, void* pWriteNV);
#endif

#pragma endregion (Extern Functions)

#pragma region (Predefined Global Variable)

	VkBlendOp gVkBlendOpTranslator[BlendMode::SG_MAX_BLEND_MODES] =
	{
		VK_BLEND_OP_ADD,
		VK_BLEND_OP_SUBTRACT,
		VK_BLEND_OP_REVERSE_SUBTRACT,
		VK_BLEND_OP_MIN,
		VK_BLEND_OP_MAX,
	};

	VkBlendFactor gVkBlendConstantTranslator[BlendConstant::SG_MAX_BLEND_CONSTANTS] =
	{
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_SRC_COLOR,
		VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
		VK_BLEND_FACTOR_DST_COLOR,
		VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
		VK_BLEND_FACTOR_SRC_ALPHA,
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		VK_BLEND_FACTOR_DST_ALPHA,
		VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
		VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
		VK_BLEND_FACTOR_CONSTANT_COLOR,
		VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
	};

	VkCompareOp gVkComparisonFuncTranslator[CompareMode::SG_MAX_COMPARE_MODES] =
	{
		VK_COMPARE_OP_NEVER,
		VK_COMPARE_OP_LESS,
		VK_COMPARE_OP_EQUAL,
		VK_COMPARE_OP_LESS_OR_EQUAL,
		VK_COMPARE_OP_GREATER,
		VK_COMPARE_OP_NOT_EQUAL,
		VK_COMPARE_OP_GREATER_OR_EQUAL,
		VK_COMPARE_OP_ALWAYS,
	};

	VkStencilOp gVkStencilOpTranslator[StencilOp::SG_MAX_STENCIL_OPS] =
	{
		VK_STENCIL_OP_KEEP,
		VK_STENCIL_OP_ZERO,
		VK_STENCIL_OP_REPLACE,
		VK_STENCIL_OP_INVERT,
		VK_STENCIL_OP_INCREMENT_AND_WRAP,
		VK_STENCIL_OP_DECREMENT_AND_WRAP,
		VK_STENCIL_OP_INCREMENT_AND_CLAMP,
		VK_STENCIL_OP_DECREMENT_AND_CLAMP,
	};

	VkCullModeFlagBits gVkCullModeTranslator[CullMode::SG_MAX_CULL_MODES] =
	{
		VK_CULL_MODE_NONE,
		VK_CULL_MODE_BACK_BIT,
		VK_CULL_MODE_FRONT_BIT
	};

	VkPolygonMode gVkFillModeTranslator[FillMode::SG_MAX_FILL_MODES] =
	{
		VK_POLYGON_MODE_FILL,
		VK_POLYGON_MODE_LINE
	};

	VkFrontFace gVkFrontFaceTranslator[] =
	{
		VK_FRONT_FACE_COUNTER_CLOCKWISE,
		VK_FRONT_FACE_CLOCKWISE
	};

	VkAttachmentLoadOp gVkAttachmentLoadOpTranslator[LoadActionType::SG_MAX_LOAD_ACTION] =
	{
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_LOAD_OP_LOAD,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
	};

	VkPipelineBindPoint gPipelineBindPoint[SG_PIPELINE_TYPE_COUNT] =
	{
		VK_PIPELINE_BIND_POINT_MAX_ENUM,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
	#ifdef SG_ENABLE_RAYTRACING
		VK_PIPELINE_BIND_POINT_RAY_TRACING_NV
	#endif
	};

#ifdef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
	static bool gDebugUtilsExtension = false;
#endif
	static bool gRenderDocLayerEnabled = false;
	static bool gDedicatedAllocationExtension = false;
	static bool gExternalMemoryExtension = false;
#ifndef NX64
	static bool gDrawIndirectCountExtension = false;
#endif
	static bool gDeviceGroupCreationExtension = false;
	static bool gDescriptorIndexingExtension = false;
	static bool gAMDDrawIndirectCountExtension = false;
	static bool gAMDGCNShaderExtension = false;
	static bool gNVRayTracingExtension = false;
	static bool gYCbCrExtension = false;
	static bool gDebugMarkerSupport = false;

	// +1 for Acceleration Structure
#define SG_DESCRIPTOR_TYPE_RANGE_SIZE (VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT + 2)
	static uint32_t gDescriptorTypeRangeSize = (SG_DESCRIPTOR_TYPE_RANGE_SIZE - 1);

	const char* gVkWantedInstanceExtensions[] =
	{
		VK_KHR_SURFACE_EXTENSION_NAME,
	#if defined(VK_USE_PLATFORM_WIN32_KHR)
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
	#elif defined(VK_USE_PLATFORM_XLIB_KHR)
		VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
	#elif defined(VK_USE_PLATFORM_XCB_KHR)
		VK_KHR_XCB_SURFACE_EXTENSION_NAME,
	#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
		VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
	#elif defined(VK_USE_PLATFORM_GGP)
		VK_GGP_STREAM_DESCRIPTOR_SURFACE_EXTENSION_NAME,
	#elif defined(VK_USE_PLATFORM_VI_NN)
		VK_NN_VI_SURFACE_EXTENSION_NAME,
	#endif
		// Debug utils not supported on all devices yet
	#ifdef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	#else
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
	#endif
		VK_NV_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
		// To legally use HDR formats
		VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
		// VR Extensions
		VK_KHR_DISPLAY_EXTENSION_NAME,
		VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
		// Multi GPU Extensions
	#if VK_KHR_device_group_creation
		  VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME,
	#endif
	#ifndef NX64 
		  // Property querying extensions
		  VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	  #endif
	};

	const char* gVkWantedDeviceExtensions[] =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_MAINTENANCE1_EXTENSION_NAME,
		VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
		VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME,
		VK_EXT_SHADER_SUBGROUP_VOTE_EXTENSION_NAME,
		VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
		VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
	#ifdef USE_EXTERNAL_MEMORY_EXTENSIONS
		VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
		VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
		VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME,
	#if defined(VK_USE_PLATFORM_WIN32_KHR)
		VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
		VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
		VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME,
	#endif
	#endif
		// debug marker extension in case debug utils is not supported
		#ifndef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
			VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
		#endif
		#if defined(VK_USE_PLATFORM_GGP)
			VK_GGP_FRAME_TOKEN_EXTENSION_NAME,
		#endif

		#if VK_KHR_draw_indirect_count
			VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
		#endif
			// Fragment shader interlock extension to be used for ROV type functionality in Vulkan
		#if VK_EXT_fragment_shader_interlock
			VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME,
		#endif
			// NVIDIA Specific Extensions

		#ifdef USE_NV_EXTENSIONS
			VK_NVX_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME,
		#endif
			// AMD Specific Extensions
			VK_AMD_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
			VK_AMD_SHADER_BALLOT_EXTENSION_NAME,
			VK_AMD_GCN_SHADER_EXTENSION_NAME,

			// Multi GPU Extensions
		#if VK_KHR_device_group
			VK_KHR_DEVICE_GROUP_EXTENSION_NAME,
		#endif
			// Bindless & None Uniform access Extensions
			VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
			// Descriptor Update Template Extension for efficient descriptor set updates
			VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
			// Raytracing
		#ifdef SG_ENABLE_RAYTRACING
			VK_NV_RAY_TRACING_EXTENSION_NAME,
		#endif
			// YCbCr format support
		#if VK_KHR_sampler_ycbcr_conversion
			VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
		#endif
			// Nsight Aftermath
		#ifdef USE_NSIGHT_AFTERMATH
				VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME,
				VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME,
		#endif
	};
	// clang-format on

#pragma endregion (Predefined Global Variable)

#pragma region (Utility Function)

	TinyImageFormat get_recommended_swapchain_format(bool hintHDR)
	{
		if (hintHDR)
			return TinyImageFormat_A2B10G10R10_UNORM; // VK_FORMAT_A2B10G10R10_UNORM_PACK32

		// TODO: figure out this properly. BGRA not supported on android.
#if !defined(VK_USE_PLATFORM_ANDROID_KHR) && !defined(VK_USE_PLATFORM_VI_NN)
		return TinyImageFormat_B8G8R8A8_UNORM;
#else
		return TinyImageFormat_R8G8B8A8_UNORM;
#endif
	}

	VkFilter util_to_vk_filter(FilterType filter)
	{
		switch (filter)
		{
		case SG_FILTER_NEAREST: return VK_FILTER_NEAREST;
		case SG_FILTER_LINEAR: return VK_FILTER_LINEAR;
		default: return VK_FILTER_LINEAR;
		}
	}

	VkSamplerMipmapMode util_to_vk_mip_map_mode(MipMapMode mipMapMode)
	{
		switch (mipMapMode)
		{
		case SG_MIPMAP_MODE_NEAREST: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		case SG_MIPMAP_MODE_LINEAR: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		default: ASSERT(false && "Invalid Mip Map Mode"); return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
		}
	}

	VkSamplerAddressMode util_to_vk_address_mode(AddressMode addressMode)
	{
		switch (addressMode)
		{
		case SG_ADDRESS_MODE_MIRROR: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case SG_ADDRESS_MODE_REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case SG_ADDRESS_MODE_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case SG_ADDRESS_MODE_CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		}
	}

	VkShaderStageFlags util_to_vk_shader_stages(ShaderStage shader_stages)
	{
		VkShaderStageFlags result = 0;
		if (SG_SHADER_STAGE_ALL_GRAPHICS == (shader_stages & SG_SHADER_STAGE_ALL_GRAPHICS))
		{
			result = VK_SHADER_STAGE_ALL_GRAPHICS;
		}
		else
		{
			if (SG_SHADER_STAGE_VERT == (shader_stages & SG_SHADER_STAGE_VERT))
			{
				result |= VK_SHADER_STAGE_VERTEX_BIT;
			}
			if (SG_SHADER_STAGE_TESC == (shader_stages & SG_SHADER_STAGE_TESC))
			{
				result |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
			}
			if (SG_SHADER_STAGE_TESE == (shader_stages & SG_SHADER_STAGE_TESE))
			{
				result |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
			}
			if (SG_SHADER_STAGE_GEOM == (shader_stages & SG_SHADER_STAGE_GEOM))
			{
				result |= VK_SHADER_STAGE_GEOMETRY_BIT;
			}
			if (SG_SHADER_STAGE_FRAG == (shader_stages & SG_SHADER_STAGE_FRAG))
			{
				result |= VK_SHADER_STAGE_FRAGMENT_BIT;
			}
			if (SG_SHADER_STAGE_COMP == (shader_stages & SG_SHADER_STAGE_COMP))
			{
				result |= VK_SHADER_STAGE_COMPUTE_BIT;
			}
		}
		return result;
	}

	VkSampleCountFlagBits util_to_vk_sample_count(SampleCount sampleCount)
	{
		VkSampleCountFlagBits result = VK_SAMPLE_COUNT_1_BIT;
		switch (sampleCount)
		{
		case SG_SAMPLE_COUNT_1: result = VK_SAMPLE_COUNT_1_BIT; break;
		case SG_SAMPLE_COUNT_2: result = VK_SAMPLE_COUNT_2_BIT; break;
		case SG_SAMPLE_COUNT_4: result = VK_SAMPLE_COUNT_4_BIT; break;
		case SG_SAMPLE_COUNT_8: result = VK_SAMPLE_COUNT_8_BIT; break;
		case SG_SAMPLE_COUNT_16: result = VK_SAMPLE_COUNT_16_BIT; break;
		}
		return result;
	}

	VkBufferUsageFlags util_to_vk_buffer_usage(DescriptorType usage, bool typed)
	{
		VkBufferUsageFlags result = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		if (usage & SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		{
			result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		}
		if (usage & SG_DESCRIPTOR_TYPE_RW_BUFFER)
		{
			result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			if (typed)
				result |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
		}
		if (usage & SG_DESCRIPTOR_TYPE_BUFFER)
		{
			result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			if (typed)
				result |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
		}
		if (usage & SG_DESCRIPTOR_TYPE_INDEX_BUFFER)
		{
			result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		}
		if (usage & SG_DESCRIPTOR_TYPE_VERTEX_BUFFER)
		{
			result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		}
		if (usage & SG_DESCRIPTOR_TYPE_INDIRECT_BUFFER)
		{
			result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
		}
#ifdef SG_ENABLE_RAYTRACING
		if (usage & SG_DESCRIPTOR_TYPE_RAY_TRACING)
		{
			result |= VK_BUFFER_USAGE_RAY_TRACING_BIT_NV;
		}
#endif
		return result;
	}

	VkImageUsageFlags util_to_vk_image_usage(DescriptorType usage)
	{
		VkImageUsageFlags result = 0;
		if (SG_DESCRIPTOR_TYPE_TEXTURE == (usage & SG_DESCRIPTOR_TYPE_TEXTURE))
			result |= VK_IMAGE_USAGE_SAMPLED_BIT;
		if (SG_DESCRIPTOR_TYPE_RW_TEXTURE == (usage & SG_DESCRIPTOR_TYPE_RW_TEXTURE))
			result |= VK_IMAGE_USAGE_STORAGE_BIT;
		return result;
	}

	VkAccessFlags util_to_vk_access_flags(ResourceState state)
	{
		VkAccessFlags ret = 0;
		if (state & SG_RESOURCE_STATE_COPY_SOURCE)
		{
			ret |= VK_ACCESS_TRANSFER_READ_BIT;
		}
		if (state & SG_RESOURCE_STATE_COPY_DEST)
		{
			ret |= VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		if (state & SG_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		{
			ret |= VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		}
		if (state & SG_RESOURCE_STATE_INDEX_BUFFER)
		{
			ret |= VK_ACCESS_INDEX_READ_BIT;
		}
		if (state & SG_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			ret |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		}
		if (state & SG_RESOURCE_STATE_INDIRECT_ARGUMENT)
		{
			ret |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		}
		if (state & SG_RESOURCE_STATE_RENDER_TARGET)
		{
			ret |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}
		if (state & SG_RESOURCE_STATE_DEPTH_WRITE)
		{
			ret |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}
		if (state & SG_RESOURCE_STATE_SHADER_RESOURCE)
		{
			ret |= VK_ACCESS_SHADER_READ_BIT;
		}
		if (state & SG_RESOURCE_STATE_PRESENT)
		{
			ret |= VK_ACCESS_MEMORY_READ_BIT;
		}
#ifdef SG_ENABLE_RAYTRACING
		if (state & SG_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
		{
			ret |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV;
		}
#endif
		return ret;
	}

	VkImageLayout util_to_vk_image_layout(ResourceState usage)
	{
		if (usage & SG_RESOURCE_STATE_COPY_SOURCE)
			return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		if (usage & SG_RESOURCE_STATE_COPY_DEST)
			return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		if (usage & SG_RESOURCE_STATE_RENDER_TARGET)
			return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		if (usage & SG_RESOURCE_STATE_DEPTH_WRITE)
			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		if (usage & SG_RESOURCE_STATE_UNORDERED_ACCESS)
			return VK_IMAGE_LAYOUT_GENERAL;

		if (usage & SG_RESOURCE_STATE_SHADER_RESOURCE)
			return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		if (usage & SG_RESOURCE_STATE_PRESENT)
			return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		if (usage == SG_RESOURCE_STATE_COMMON)
			return VK_IMAGE_LAYOUT_GENERAL;

		return VK_IMAGE_LAYOUT_UNDEFINED;
	}

	void util_get_planar_vk_image_memory_requirement(VkDevice device, VkImage image, uint32_t planesCount, VkMemoryRequirements* outVkMemReq, uint64_t* outPlanesOffsets)
	{
		outVkMemReq->size = 0;
		outVkMemReq->alignment = 0;
		outVkMemReq->memoryTypeBits = 0;

		VkImagePlaneMemoryRequirementsInfo imagePlaneMemReqInfo = { VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO, NULL };

		VkImageMemoryRequirementsInfo2 imagePlaneMemReqInfo2 = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2 };
		imagePlaneMemReqInfo2.pNext = &imagePlaneMemReqInfo;
		imagePlaneMemReqInfo2.image = image;

		VkMemoryDedicatedRequirements memDedicatedReq = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, NULL };
		VkMemoryRequirements2 memReq2 = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
		memReq2.pNext = &memDedicatedReq;

		for (uint32_t i = 0; i < planesCount; ++i)
		{
			imagePlaneMemReqInfo.planeAspect = (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_PLANE_0_BIT << i);
			vkGetImageMemoryRequirements2(device, &imagePlaneMemReqInfo2, &memReq2);

			outPlanesOffsets[i] += outVkMemReq->size;
			outVkMemReq->alignment = eastl::max(memReq2.memoryRequirements.alignment, outVkMemReq->alignment);
			outVkMemReq->size += round_up(memReq2.memoryRequirements.size, memReq2.memoryRequirements.alignment);
			outVkMemReq->memoryTypeBits |= memReq2.memoryRequirements.memoryTypeBits;
		}
	}

	uint32_t util_get_memory_type(uint32_t typeBits, VkPhysicalDeviceMemoryProperties memoryProperties, VkMemoryPropertyFlags properties, VkBool32* memTypeFound = nullptr)
	{
		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
		{
			if ((typeBits & 1) == 1)
			{
				if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
				{
					if (memTypeFound)
					{
						*memTypeFound = true;
					}
					return i;
				}
			}
			typeBits >>= 1;
		}

		if (memTypeFound)
		{
			*memTypeFound = false;
			return 0;
		}
		else
		{
			SG_LOG_ERROR("Could not find a matching memory type");
			ASSERT(false);
			return 0;
		}
	}

	// Determines pipeline stages involved for given accesses
	VkPipelineStageFlags util_determine_pipeline_stage_flags(Renderer* pRenderer, VkAccessFlags accessFlags, QueueType queueType)
	{
		VkPipelineStageFlags flags = 0;
		switch (queueType)
		{
		case SG_QUEUE_TYPE_GRAPHICS:
		{
			if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

			if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0)
			{
				flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
				flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				if (pRenderer->pActiveGpuSettings->geometryShaderSupported)
				{
					flags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
				}
				if (pRenderer->pActiveGpuSettings->tessellationSupported)
				{
					flags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
					flags |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
				}
				flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
#ifdef SG_ENABLE_RAYTRACING
				if (pRenderer->raytracingExtension)
				{
					flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV;
				}
#endif
			}

			if ((accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0)
				flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

			if ((accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

			if ((accessFlags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;
		}
		case SG_QUEUE_TYPE_COMPUTE:
		{
			if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0 ||
				(accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0 ||
				(accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0 ||
				(accessFlags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
				return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

			if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			break;
		}
		case SG_QUEUE_TYPE_TRANSFER:
			return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		default:
			break;
		}

		// Compatible with both compute and graphics queues
		if ((accessFlags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) != 0)
			flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;

		if ((accessFlags & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0)
			flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;

		if ((accessFlags & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT)) != 0)
			flags |= VK_PIPELINE_STAGE_HOST_BIT;

		if (flags == 0)
			flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		return flags;
	}

	VkImageAspectFlags util_vk_determine_aspect_mask(VkFormat format, bool includeStencilBit)
	{
		VkImageAspectFlags result = 0;
		switch (format)
		{
			// Depth
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_X8_D24_UNORM_PACK32:
		case VK_FORMAT_D32_SFLOAT:
			result = VK_IMAGE_ASPECT_DEPTH_BIT;
			break;
			// Stencil
		case VK_FORMAT_S8_UINT:
			result = VK_IMAGE_ASPECT_STENCIL_BIT;
			break;
			// Depth/stencil
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			result = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (includeStencilBit) result |= VK_IMAGE_ASPECT_STENCIL_BIT;
			break;
			// Assume everything else is Color
		default: result = VK_IMAGE_ASPECT_COLOR_BIT; break;
		}
		return result;
	}

	VkFormatFeatureFlags util_vk_image_usage_to_format_features(VkImageUsageFlags usage)
	{
		auto result = (VkFormatFeatureFlags)0;
		if (VK_IMAGE_USAGE_SAMPLED_BIT == (usage & VK_IMAGE_USAGE_SAMPLED_BIT))
		{
			result |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
		}
		if (VK_IMAGE_USAGE_STORAGE_BIT == (usage & VK_IMAGE_USAGE_STORAGE_BIT))
		{
			result |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
		}
		if (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT == (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
		{
			result |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
		}
		if (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT == (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
		{
			result |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		return result;
	}

	VkQueueFlags util_to_vk_queue_flags(QueueType queueType)
	{
		switch (queueType)
		{
		case SG_QUEUE_TYPE_GRAPHICS: return VK_QUEUE_GRAPHICS_BIT;
		case SG_QUEUE_TYPE_TRANSFER: return VK_QUEUE_TRANSFER_BIT;
		case SG_QUEUE_TYPE_COMPUTE: return VK_QUEUE_COMPUTE_BIT;
		default: ASSERT(false && "Invalid Queue Type"); return VK_QUEUE_FLAG_BITS_MAX_ENUM;
		}
	}

	VkDescriptorType util_to_vk_descriptor_type(DescriptorType type)
	{
		switch (type)
		{
		case SG_DESCRIPTOR_TYPE_UNDEFINED: ASSERT("Invalid DescriptorInfo Type"); return VK_DESCRIPTOR_TYPE_MAX_ENUM;
		case SG_DESCRIPTOR_TYPE_SAMPLER: return VK_DESCRIPTOR_TYPE_SAMPLER;
		case SG_DESCRIPTOR_TYPE_TEXTURE: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		case SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case SG_DESCRIPTOR_TYPE_RW_TEXTURE: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		case SG_DESCRIPTOR_TYPE_BUFFER:
		case SG_DESCRIPTOR_TYPE_RW_BUFFER: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case SG_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		case SG_DESCRIPTOR_TYPE_TEXEL_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		case SG_DESCRIPTOR_TYPE_RW_TEXEL_BUFFER: return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		case SG_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
#ifdef SG_ENABLE_RAYTRACING
		case SG_DESCRIPTOR_TYPE_RAY_TRACING: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
#endif
		default:
			ASSERT("Invalid DescriptorInfo Type");
			return VK_DESCRIPTOR_TYPE_MAX_ENUM;
			break;
		}
	}

	VkShaderStageFlags util_to_vk_shader_stage_flags(ShaderStage stages)
	{
		VkShaderStageFlags res = 0;
		if (stages & SG_SHADER_STAGE_ALL_GRAPHICS)
			return VK_SHADER_STAGE_ALL_GRAPHICS;

		if (stages & SG_SHADER_STAGE_VERT)
			res |= VK_SHADER_STAGE_VERTEX_BIT;
		if (stages & SG_SHADER_STAGE_GEOM)
			res |= VK_SHADER_STAGE_GEOMETRY_BIT;
		if (stages & SG_SHADER_STAGE_TESE)
			res |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		if (stages & SG_SHADER_STAGE_TESC)
			res |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		if (stages & SG_SHADER_STAGE_COMP)
			res |= VK_SHADER_STAGE_COMPUTE_BIT;
#ifdef SG_ENABLE_RAYTRACING
		if (stages & SG_SHADER_STAGE_RAYTRACING)
			res |= (
				VK_SHADER_STAGE_RAYGEN_BIT_NV |
				VK_SHADER_STAGE_ANY_HIT_BIT_NV |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
				VK_SHADER_STAGE_MISS_BIT_NV |
				VK_SHADER_STAGE_INTERSECTION_BIT_NV |
				VK_SHADER_STAGE_CALLABLE_BIT_NV);
#endif
		ASSERT(res != 0);
		return res;
	}

	void util_find_queue_family_index(const Renderer* pRenderer, uint32_t nodeIndex, QueueType queueType,
		VkQueueFamilyProperties* pOutProps, uint8_t* pOutFamilyIndex, uint8_t* pOutQueueIndex)
	{
		uint32_t       queueFamilyIndex = UINT32_MAX;
		uint32_t       queueIndex = UINT32_MAX;
		VkQueueFlags   requiredFlags = util_to_vk_queue_flags(queueType);
		bool           found = false;

		// Get queue family properties
		uint32_t queueFamilyPropertyCount = 0;
		VkQueueFamilyProperties* queueFamilyProperties = nullptr;
		vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->pVkActiveGPU, &queueFamilyPropertyCount, nullptr);
		queueFamilyProperties = (VkQueueFamilyProperties*)alloca(queueFamilyPropertyCount * sizeof(VkQueueFamilyProperties));
		vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->pVkActiveGPU, &queueFamilyPropertyCount, queueFamilyProperties);

		uint32_t minQueueFlag = UINT32_MAX;

		// Try to find a dedicated queue of this type
		for (uint32_t index = 0; index < queueFamilyPropertyCount; ++index)
		{
			VkQueueFlags queueFlags = queueFamilyProperties[index].queueFlags;
			bool graphicsQueue = (queueFlags & VK_QUEUE_GRAPHICS_BIT) ? true : false;
			uint32_t flagAnd = (queueFlags & requiredFlags);
			if (queueType == SG_QUEUE_TYPE_GRAPHICS && graphicsQueue)
			{
				found = true;
				queueFamilyIndex = index;
				queueIndex = 0;
				break;
			}
			if ((queueFlags & requiredFlags) && ((queueFlags & ~requiredFlags) == 0) &&
				pRenderer->pUsedQueueCount[nodeIndex][queueFlags] < pRenderer->pAvailableQueueCount[nodeIndex][queueFlags])
			{
				found = true;
				queueFamilyIndex = index;
				queueIndex = pRenderer->pUsedQueueCount[nodeIndex][queueFlags];
				break;
			}
			if (flagAnd && ((queueFlags - flagAnd) < minQueueFlag) && !graphicsQueue &&
				pRenderer->pUsedQueueCount[nodeIndex][queueFlags] < pRenderer->pAvailableQueueCount[nodeIndex][queueFlags])
			{
				found = true;
				minQueueFlag = (queueFlags - flagAnd);
				queueFamilyIndex = index;
				queueIndex = pRenderer->pUsedQueueCount[nodeIndex][queueFlags];
				break;
			}
		}

		// If hardware doesn't provide a dedicated queue try to find a non-dedicated one
		if (!found)
		{
			for (uint32_t index = 0; index < queueFamilyPropertyCount; ++index)
			{
				VkQueueFlags queueFlags = queueFamilyProperties[index].queueFlags;
				if ((queueFlags & requiredFlags) &&
					pRenderer->pUsedQueueCount[nodeIndex][queueFlags] < pRenderer->pAvailableQueueCount[nodeIndex][queueFlags])
				{
					found = true;
					queueFamilyIndex = index;
					queueIndex = pRenderer->pUsedQueueCount[nodeIndex][queueFlags];
					break;
				}
			}
		}

		if (!found)
		{
			found = true;
			queueFamilyIndex = 0;
			queueIndex = 0;

			SG_LOG_WARNING("Could not find queue of type %u. Using default queue", (uint32_t)queueType);
		}

		if (pOutProps)
			*pOutProps = queueFamilyProperties[queueFamilyIndex];
		if (pOutFamilyIndex)
			*pOutFamilyIndex = (uint8_t)queueFamilyIndex;
		if (pOutQueueIndex)
			*pOutQueueIndex = (uint8_t)queueIndex;
	}

	static VkPipelineCacheCreateFlags util_to_pipeline_cache_flags(PipelineCacheFlags flags)
	{
		VkPipelineCacheCreateFlags ret = 0;
#if VK_EXT_pipeline_creation_cache_control
		if (flags & SG_PIPELINE_CACHE_FLAG_EXTERNALLY_SYNCHRONIZED)
		{
			ret |= VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT_EXT;
		}
#endif
		return ret;
	}

	/// Multi GPU Helper Functions
	uint32_t util_calculate_shared_device_mask(uint32_t gpuCount)
	{
		return (1 << gpuCount) - 1;
	}

	void util_calculate_device_indices(Renderer* pRenderer, uint32_t nodeIndex, uint32_t* pSharedNodeIndices, uint32_t sharedNodeIndexCount, uint32_t* pIndices)
	{
		for (uint32_t i = 0; i < pRenderer->linkedNodeCount; ++i)
			pIndices[i] = i;

		pIndices[nodeIndex] = nodeIndex;
		/************************************************************************/
		// Set the node indices which need sharing access to the creation node
		// Example: Texture created on GPU0 but GPU1 will need to access it, GPU2 does not care
		//		  pIndices = { 0, 0, 2 }
		/************************************************************************/
		for (uint32_t i = 0; i < sharedNodeIndexCount; ++i)
			pIndices[pSharedNodeIndices[i]] = nodeIndex;
	}

	static inline VkPipelineColorBlendStateCreateInfo util_to_blend_desc(const BlendStateDesc* pDesc, VkPipelineColorBlendAttachmentState* pAttachments)
	{
		int blendDescIndex = 0;
#if defined(SG_ENABLE_GRAPHICS_DEBUG)
		for (int i = 0; i < SG_MAX_RENDER_TARGET_ATTACHMENTS; ++i)
		{
			if (pDesc->renderTargetMask & (1 << i))
			{
				ASSERT(pDesc->srcFactors[blendDescIndex] < BlendConstant::SG_MAX_BLEND_CONSTANTS);
				ASSERT(pDesc->dstFactors[blendDescIndex] < BlendConstant::SG_MAX_BLEND_CONSTANTS);
				ASSERT(pDesc->srcAlphaFactors[blendDescIndex] < BlendConstant::SG_MAX_BLEND_CONSTANTS);
				ASSERT(pDesc->dstAlphaFactors[blendDescIndex] < BlendConstant::SG_MAX_BLEND_CONSTANTS);
				ASSERT(pDesc->blendModes[blendDescIndex] < BlendMode::SG_MAX_BLEND_MODES);
				ASSERT(pDesc->blendAlphaModes[blendDescIndex] < BlendMode::SG_MAX_BLEND_MODES);
			}

			if (pDesc->independentBlend)
				++blendDescIndex;
		}

		blendDescIndex = 0;
#endif
		for (int i = 0; i < SG_MAX_RENDER_TARGET_ATTACHMENTS; ++i)
		{
			if (pDesc->renderTargetMask & (1 << i))
			{
				//VkBool32 blendEnable =
				//	(gVkBlendConstantTranslator[pDesc->srcFactors[blendDescIndex]] != VK_BLEND_FACTOR_ONE ||
				//		gVkBlendConstantTranslator[pDesc->dstFactors[blendDescIndex]] != VK_BLEND_FACTOR_ZERO ||
				//		gVkBlendConstantTranslator[pDesc->srcAlphaFactors[blendDescIndex]] != VK_BLEND_FACTOR_ONE ||
				//		gVkBlendConstantTranslator[pDesc->dstAlphaFactors[blendDescIndex]] != VK_BLEND_FACTOR_ZERO);

				VkBool32 blendEnable = 1;

				pAttachments[i].blendEnable = blendEnable;
				pAttachments[i].colorWriteMask = pDesc->masks[blendDescIndex];
				pAttachments[i].srcColorBlendFactor = gVkBlendConstantTranslator[pDesc->srcFactors[blendDescIndex]];
				pAttachments[i].dstColorBlendFactor = gVkBlendConstantTranslator[pDesc->dstFactors[blendDescIndex]];
				pAttachments[i].colorBlendOp = gVkBlendOpTranslator[pDesc->blendModes[blendDescIndex]];
				pAttachments[i].srcAlphaBlendFactor = gVkBlendConstantTranslator[pDesc->srcAlphaFactors[blendDescIndex]];
				pAttachments[i].dstAlphaBlendFactor = gVkBlendConstantTranslator[pDesc->dstAlphaFactors[blendDescIndex]];
				pAttachments[i].alphaBlendOp = gVkBlendOpTranslator[pDesc->blendAlphaModes[blendDescIndex]];
			}

			if (pDesc->independentBlend)
				++blendDescIndex;
		}

		VkPipelineColorBlendStateCreateInfo cb = {};
		cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		cb.pNext = nullptr;
		cb.flags = 0;
		cb.logicOpEnable = VK_FALSE;
		cb.logicOp = VK_LOGIC_OP_CLEAR;
		cb.pAttachments = pAttachments;
		cb.blendConstants[0] = 0.0f;
		cb.blendConstants[1] = 0.0f;
		cb.blendConstants[2] = 0.0f;
		cb.blendConstants[3] = 0.0f;

		return cb;
	}

	static inline VkPipelineDepthStencilStateCreateInfo util_to_depth_desc(const DepthStateDesc* pDesc)
	{
		ASSERT(pDesc->depthFunc < CompareMode::SG_MAX_COMPARE_MODES);
		ASSERT(pDesc->stencilFrontFunc < CompareMode::SG_MAX_COMPARE_MODES);
		ASSERT(pDesc->stencilFrontFail < StencilOp::SG_MAX_STENCIL_OPS);
		ASSERT(pDesc->depthFrontFail < StencilOp::SG_MAX_STENCIL_OPS);
		ASSERT(pDesc->stencilFrontPass < StencilOp::SG_MAX_STENCIL_OPS);
		ASSERT(pDesc->stencilBackFunc < CompareMode::SG_MAX_COMPARE_MODES);
		ASSERT(pDesc->stencilBackFail < StencilOp::SG_MAX_STENCIL_OPS);
		ASSERT(pDesc->depthBackFail < StencilOp::SG_MAX_STENCIL_OPS);
		ASSERT(pDesc->stencilBackPass < StencilOp::SG_MAX_STENCIL_OPS);

		VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr };
		ds.flags = 0;
		ds.depthTestEnable = pDesc->depthTest ? VK_TRUE : VK_FALSE;
		ds.depthWriteEnable = pDesc->depthWrite ? VK_TRUE : VK_FALSE;
		ds.depthCompareOp = gVkComparisonFuncTranslator[pDesc->depthFunc];
		ds.depthBoundsTestEnable = VK_FALSE;
		ds.stencilTestEnable = pDesc->stencilTest ? VK_TRUE : VK_FALSE;

		ds.front.failOp = gVkStencilOpTranslator[pDesc->stencilFrontFail];
		ds.front.passOp = gVkStencilOpTranslator[pDesc->stencilFrontPass];
		ds.front.depthFailOp = gVkStencilOpTranslator[pDesc->depthFrontFail];
		ds.front.compareOp = VkCompareOp(pDesc->stencilFrontFunc);
		ds.front.compareMask = pDesc->stencilReadMask;
		ds.front.writeMask = pDesc->stencilWriteMask;
		ds.front.reference = 0;

		ds.back.failOp = gVkStencilOpTranslator[pDesc->stencilBackFail];
		ds.back.passOp = gVkStencilOpTranslator[pDesc->stencilBackPass];
		ds.back.depthFailOp = gVkStencilOpTranslator[pDesc->depthBackFail];
		ds.back.compareOp = gVkComparisonFuncTranslator[pDesc->stencilBackFunc];
		ds.back.compareMask = pDesc->stencilReadMask;
		ds.back.writeMask = pDesc->stencilWriteMask;
		ds.back.reference = 0;

		ds.minDepthBounds = 0;
		ds.maxDepthBounds = 1;

		return ds;
	}

	static inline VkPipelineRasterizationStateCreateInfo util_to_rasterizer_desc(const RasterizerStateDesc* pDesc)
	{
		ASSERT(pDesc->fillMode < FillMode::SG_MAX_FILL_MODES);
		ASSERT(pDesc->cullMode < CullMode::SG_MAX_CULL_MODES);
		ASSERT(pDesc->frontFace == SG_FRONT_FACE_CCW || pDesc->frontFace == SG_FRONT_FACE_CW);

		VkPipelineRasterizationStateCreateInfo rs = {};
		rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rs.pNext = nullptr;
		rs.flags = 0;
		rs.depthClampEnable = pDesc->depthClampEnable ? VK_TRUE : VK_FALSE;
		rs.rasterizerDiscardEnable = VK_FALSE;
		rs.polygonMode = gVkFillModeTranslator[pDesc->fillMode];
		rs.cullMode = gVkCullModeTranslator[pDesc->cullMode];
		rs.frontFace = gVkFrontFaceTranslator[pDesc->frontFace];
		rs.depthBiasEnable = (pDesc->depthBias != 0) ? VK_TRUE : VK_FALSE;
		rs.depthBiasConstantFactor = float(pDesc->depthBias);
		rs.depthBiasClamp = 0.0f;
		rs.depthBiasSlopeFactor = pDesc->slopeScaledDepthBias;
		rs.lineWidth = 1;

		return rs;
	}

#pragma endregion (Utility Function)

#pragma region (Per Thread Frame Buffer Logic)

	/// Render-passes are not exposed to the app code since they are not available on all apis
	/// This map takes care of hashing a render pass based on the render targets passed to cmdBeginRender
	using RenderPassMap = eastl::hash_map<uint64_t, struct RenderPass*>;
	using RenderPassMapNode = RenderPassMap::value_type;
	using RenderPassMapIter = RenderPassMap::iterator;
	using FrameBufferMap = eastl::hash_map<uint64_t, struct FrameBuffer*>;
	using FrameBufferMapNode = FrameBufferMap::value_type;
	using FrameBufferMapIt = FrameBufferMap::iterator;

	// RenderPass map per thread (this will make lookups lock free and we only need a lock when inserting a RenderPass Map for the first time)
	eastl::hash_map<ThreadID, RenderPassMap>* gRenderPassMap;
	// FrameBuffer map per thread (this will make lookups lock free and we only need a lock when inserting a FrameBuffer map for the first time)
	eastl::hash_map<ThreadID, FrameBufferMap>* gFrameBufferMap;
	Mutex* pRenderPassMutex;

	static RenderPassMap& get_render_pass_map()
	{
		// Only need a lock when creating a new render pass map for this thread
		MutexLock lock(*pRenderPassMutex);
		eastl::hash_map<ThreadID, RenderPassMap>::iterator it = gRenderPassMap->find(Thread::get_curr_thread_id()); // render pass based on thread id
		if (it == gRenderPassMap->end()) // empty
		{
			return gRenderPassMap->insert(Thread::get_curr_thread_id()).first->second;
		}
		else
		{
			return it->second;
		}
	}

	static FrameBufferMap& get_frame_buffer_map()
	{
		// Only need a lock when creating a new framebuffer map for this thread
		MutexLock lock(*pRenderPassMutex);
		eastl::hash_map<ThreadID, FrameBufferMap>::iterator it = gFrameBufferMap->find(Thread::get_curr_thread_id());
		if (it == gFrameBufferMap->end())
		{
			return gFrameBufferMap->insert(Thread::get_curr_thread_id()).first->second;
		}
		else
		{
			return it->second;
		}
	}

#pragma endregion (Per Thread Frame Buffer Logic)

#pragma region (Vulkan Rewrite Function)

	// Proxy log callback
	static void internal_log(LogType type, const char* msg, const char* component)
	{
#ifndef NX64
		switch (type)
		{
			case SG_LOG_TYPE_INFO:  SG_LOG_INFO("%s ( %s )", component, msg);   break;
			case SG_LOG_TYPE_WARN:  SG_LOG_WARNING("%s ( %s )", component, msg); break;
			case SG_LOG_TYPE_DEBUG: SG_LOG_DEBUG("%s ( %s )", component, msg);  break;
			case SG_LOG_TYPE_ERROR: SG_LOG_ERROR("%s ( %s )", component, msg);  break;
			default: break;
		}
#endif
	}

	// rewrite vulkan memory allocation
	//static void* VKAPI_PTR gVkAllocation(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
	//{
	//	return sg_memalign(alignment, size);
	//}

	//static void* VKAPI_PTR gVkReallocation(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
	//{
	//	return sg_realloc_align(pOriginal, alignment, size);
	//}

	//static void VKAPI_PTR gVkFree(void* pUserData, void* pMemory) 
	//{ 
	//	sg_free(pMemory);
	//}

	//static void VKAPI_PTR gVkInternalAllocation(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope) 
	//{
	//}

	//static void VKAPI_PTR gVkInternalFree(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope) 
	//{
	//}

	//VkAllocationCallbacks gVkAllocationCallbacks =
	//{
	//	// pUserData
	//	nullptr,
	//	// pfnAllocation
	//	gVkAllocation,
	//	// pfnReallocation
	//	gVkReallocation,
	//	// pfnFree
	//	gVkFree,
	//	// pfnInternalAllocation
	//	gVkInternalAllocation,
	//	// pfnInternalFree
	//	gVkInternalFree
	//};

#ifdef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
	// debug callback for vulkan layers
	static VkBool32 VKAPI_PTR internal_debug_report_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		const char* pLayerPrefix = pCallbackData->pMessageIdName;
		const char* pMessage = pCallbackData->pMessage;
		int32_t     messageCode = pCallbackData->messageIdNumber;
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		{
			SG_LOG_INFO("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			SG_LOG_WARNING("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			SG_LOG_ERROR("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
			ASSERT(false);
		}
		return VK_FALSE;
	}
#else
	static VKAPI_ATTR VkBool32 VKAPI_CALL internal_debug_report_callback(
		VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode,
		const char* pLayerPrefix, const char* pMessage, void* pUserData)
	{
		if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
		{
			SG_LOG_INFO("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
		}
		else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
		{
			SG_LOG_WARNING("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
		}
		else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		{
			SG_LOG_WARNING("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
		}
		else if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		{
			SG_LOG_ERROR("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
			ASSERT(false);
		}
		return VK_FALSE;
	}
#endif

#pragma endregion (Vulkan Rewrite Function)

#pragma region (Internal Init Function)

	void create_instance(const char* apname, const RendererCreateDesc* pDesc, uint32_t userDefinedInstanceLayerCount,
		const char** userDefinedInstanceLayers, Renderer* pRenderer)
	{
		// These are the extensions that we have loaded
		const char* instanceExtensionCache[SG_MAX_INSTANCE_EXTENSIONS] = {};

		uint32_t layerCount = 0;
		uint32_t extCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);

		auto* layers = (VkLayerProperties*)alloca(sizeof(VkLayerProperties) * layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, layers);

		auto* exts = (VkExtensionProperties*)alloca(sizeof(VkExtensionProperties) * extCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extCount, exts);

#if defined(SG_ENUM_VK_EXT_AND_LAYER)
		for (uint32_t i = 0; i < layerCount; ++i)
		{
			internal_log(SG_LOG_TYPE_INFO, layers[i].layerName, "vkinstance-layer");
		}

		for (uint32_t i = 0; i < extCount; ++i)
		{
			internal_log(SG_LOG_TYPE_INFO, exts[i].extensionName, "vkinstance-ext");
		}
#endif

		SG_DECLARE_ZERO(VkApplicationInfo, appInfo);
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = apname;
		appInfo.applicationVersion = VK_MAKE_VERSION(0, 2, 0);
		appInfo.pEngineName = "Seagull";
		appInfo.engineVersion = VK_MAKE_VERSION(0, 2, 0);
#ifdef ANDROID
		appInfo.apiVersion = VK_API_VERSION_1_0;
#else
		appInfo.apiVersion = VK_API_VERSION_1_2;
#endif

		eastl::vector<const char*> layerTemp = eastl::vector<const char*>(userDefinedInstanceLayerCount);
		memcpy(layerTemp.data(), userDefinedInstanceLayers, layerTemp.size() * sizeof(char*));

		// Instance
		{
			// check to see if the layers are present
			for (uint32_t i = 0; i < (uint32_t)layerTemp.size(); ++i)
			{
				bool layerFound = false;
				for (uint32_t j = 0; j < layerCount; ++j)
				{
					if (strcmp(userDefinedInstanceLayers[i], layers[j].layerName) == 0)
					{
						layerFound = true;
						break;
					}
				}
				if (layerFound == false)
				{
					internal_log(SG_LOG_TYPE_WARN, userDefinedInstanceLayers[i], "vkinstance-layer-missing");
					// deleate layer and get new index
					i = (uint32_t)(layerTemp.erase(layerTemp.begin() + i) - layerTemp.begin());
				}
			}

			uint32_t extensionCount = 0;
			const uint32_t initialCount = sizeof(gVkWantedInstanceExtensions) / sizeof(gVkWantedInstanceExtensions[0]);
			const uint32_t userRequestedCount = (uint32_t)pDesc->instanceExtensionCount;
			eastl::vector<const char*> wantedInstanceExtensions(initialCount + userRequestedCount);

			for (uint32_t i = 0; i < initialCount; ++i)
			{
				wantedInstanceExtensions[i] = gVkWantedInstanceExtensions[i];
			}
			for (uint32_t i = 0; i < userRequestedCount; ++i)
			{
				wantedInstanceExtensions[initialCount + i] = pDesc->ppInstanceExtensions[i];
			}
			const uint32_t wantedExtensionCount = (uint32_t)wantedInstanceExtensions.size();

			//#if defined(NX64)
			//	loadExtensionsNX(pRenderer->pVkInstance);
			//#else
			//	// Load Vulkan instance functions
			//	volkLoadInstance(pRenderer->pVkInstance);
			//#endif

			// Layer extensions
			for (size_t i = 0; i < layerTemp.size(); ++i)
			{
				const char* layerName = layerTemp[i];
				uint32_t    count = 0;

				vkEnumerateInstanceExtensionProperties(layerName, &count, nullptr);
				VkExtensionProperties* properties = count ? (VkExtensionProperties*)sg_calloc(count, sizeof(*properties)) : nullptr;
				ASSERT(properties != nullptr || count == 0);
				vkEnumerateInstanceExtensionProperties(layerName, &count, properties);

				for (uint32_t j = 0; j < count; ++j)
				{
					for (uint32_t k = 0; k < wantedExtensionCount; ++k)
					{
						if (strcmp(wantedInstanceExtensions[k], properties[j].extensionName) == 0)
						{
							if (strcmp(wantedInstanceExtensions[k], VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME) == 0)
								gDeviceGroupCreationExtension = true;
#ifdef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
							if (strcmp(wantedInstanceExtensions[k], VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
								gDebugUtilsExtension = true;
#endif
							instanceExtensionCache[extensionCount++] = wantedInstanceExtensions[k];
							// clear wanted extension so we don't load it more then once
							wantedInstanceExtensions[k] = "";
							break;
						}
					}
				}
				SG_SAFE_FREE((void*)properties);
			}

			// Standalone extensions
			{
				const char* layerName = nullptr;
				uint32_t    count = 0;
				vkEnumerateInstanceExtensionProperties(layerName, &count, nullptr);

				if (count > 0)
				{
					VkExtensionProperties* properties = (VkExtensionProperties*)sg_calloc(count, sizeof(*properties));
					ASSERT(properties != nullptr);

					vkEnumerateInstanceExtensionProperties(layerName, &count, properties);
					for (uint32_t j = 0; j < count; ++j)
					{
						for (uint32_t k = 0; k < wantedExtensionCount; ++k)
						{
							if (strcmp(wantedInstanceExtensions[k], properties[j].extensionName) == 0)
							{
								instanceExtensionCache[extensionCount++] = wantedInstanceExtensions[k];
								// clear wanted extenstion so we don't load it more then once
								//gVkWantedInstanceExtensions[k] = "";
								if (strcmp(wantedInstanceExtensions[k], VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME) == 0)
									gDeviceGroupCreationExtension = true;
#ifdef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
								if (strcmp(wantedInstanceExtensions[k], VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
									gDebugUtilsExtension = true;
#endif
								break;
							}
						}
					}
					SG_SAFE_FREE((void*)properties);
				}
			}

#if VK_HEADER_VERSION >= 108
			VkValidationFeaturesEXT validationFeaturesExt = { VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
			VkValidationFeatureEnableEXT enabledValidationFeatures[] =
			{
				VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
			};

			if (pDesc->enableGPUBasedValidation)
			{
				validationFeaturesExt.enabledValidationFeatureCount = 1;
				validationFeaturesExt.pEnabledValidationFeatures = enabledValidationFeatures;
			}
#endif
			// add more extensions here
			SG_DECLARE_ZERO(VkInstanceCreateInfo, createInfo);
			createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
#if VK_HEADER_VERSION >= 108
			createInfo.pNext = &validationFeaturesExt;
			//createInfo.pNext = &debugCreateInfo;
#endif
			createInfo.flags = 0;
			createInfo.pApplicationInfo = &appInfo;
			createInfo.enabledLayerCount = (uint32_t)layerTemp.size();
			createInfo.ppEnabledLayerNames = layerTemp.data();
			createInfo.enabledExtensionCount = extensionCount;
			createInfo.ppEnabledExtensionNames = instanceExtensionCache;

			SG_CHECK_VKRESULT(vkCreateInstance(&createInfo, nullptr, &(pRenderer->pVkInstance)));
		}

		// debug
		{
#ifdef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
			VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
			if (gDebugUtilsExtension)
			{
				debugCreateInfo.pfnUserCallback = internal_debug_report_callback;
				debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
				debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
				debugCreateInfo.flags = 0;
				debugCreateInfo.pUserData = nullptr;

				VkResult res = VK_RESULT_MAX_ENUM;
				auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
					vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkCreateDebugUtilsMessengerEXT"); // load this function from extensions
				if (func != nullptr)
				{
					res = func(pRenderer->pVkInstance, &debugCreateInfo, nullptr, &(pRenderer->pVkDebugUtilsMessenger));
				}
				else
				{
					res = VK_ERROR_EXTENSION_NOT_PRESENT;
				}

				// vkResult res = vkCreateDebugUtilsMessengerEXT(pRenderer->pVkInstance, &createInfo, nullptr, &(pRenderer->pVkDebugUtilsMessenger));
				if (VK_SUCCESS != res)
				{
					internal_log(SG_LOG_TYPE_ERROR, "vkCreateDebugUtilsMessengerEXT failed - disabling Vulkan debug callbacks",
						"internal_vk_init_instance");
				}
			}
#else
#if defined(__ANDROID__)
			if (vkCreateDebugReportCallbackEXT)
#endif
			{
				SG_DECLARE_ZERO(VkDebugReportCallbackCreateInfoEXT, createInfo);
				createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
				createInfo.pNext = nullptr;
				createInfo.pfnCallback = internal_debug_report_callback;
				createInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
#if defined(NX64) || defined(__ANDROID__)
					VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | // Performance warnings are not very vaild on desktop
#endif
					VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT/* | VK_DEBUG_REPORT_INFORMATION_BIT_EXT*/;

				auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkCreateDebugReportCallbackEXT");
				VkResult res = VK_RESULT_MAX_ENUM;
				if (func)
				{
					res = func(pRenderer->pVkInstance, &createInfo, nullptr, &(pRenderer->pVkDebugReport));
				}
				//VkResult res = vkCreateDebugReportCallbackEXT(pRenderer->pVkInstance, &createInfo, nullptr, &(pRenderer->pVkDebugReport));
				if (VK_SUCCESS != res)
				{
					internal_log(SG_LOG_TYPE_ERROR, "vkCreateDebugReportCallbackEXT failed - disabling Vulkan debug callbacks", "internal_vk_init_instance");
				}
			}
#endif
		}
	}

	static void remove_instance(Renderer* pRenderer)
	{
		ASSERT(pRenderer->pVkInstance != VK_NULL_HANDLE);

#ifdef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
		if (pRenderer->pVkDebugUtilsMessenger)
		{
			auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
				vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkDestroyDebugUtilsMessengerEXT"); // load this function from extensions
			if (func != nullptr)
			{
				func(pRenderer->pVkInstance, pRenderer->pVkDebugUtilsMessenger, nullptr);
			}
			//vkDestroyDebugUtilsMessengerEXT(pRenderer->pVkInstance, pRenderer->pVkDebugUtilsMessenger, nullptr);
			pRenderer->pVkDebugUtilsMessenger = nullptr;
		}
#else
		if (pRenderer->pVkDebugReport)
		{
			auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkDestroyDebugReportCallbackEXT");
			if (func)
			{
				func(pRenderer->pVkInstance, pRenderer->pVkDebugReport, nullptr);
			}
			//vkDestroyDebugReportCallbackEXT(pRenderer->pVkInstance, pRenderer->pVkDebugReport, nullptr);
			pRenderer->pVkDebugReport = nullptr;
		}
#endif
		vkDestroyInstance(pRenderer->pVkInstance, nullptr);
	}

	static bool add_device(const RendererCreateDesc* pDesc, Renderer* pRenderer)
	{
		ASSERT(VK_NULL_HANDLE != pRenderer->pVkInstance);

		// these are the extensions that we have loaded
		const char* deviceExtensionCache[SG_MAX_DEVICE_EXTENSIONS] = {};
		VkResult vkRes = VK_RESULT_MAX_ENUM;

#if VK_KHR_device_group_creation
		VkDeviceGroupDeviceCreateInfoKHR deviceGroupInfo = { VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO_KHR };
		VkPhysicalDeviceGroupPropertiesKHR props[SG_MAX_LINKED_GPUS] = {};

		pRenderer->linkedNodeCount = 1;
		if (pRenderer->gpuMode == SG_GPU_MODE_LINKED && gDeviceGroupCreationExtension)
		{
			// (not shown) fill out devCreateInfo as usual.
			uint32_t deviceGroupCount = 0;

			// Query the number of device groups
			vkEnumeratePhysicalDeviceGroups(pRenderer->pVkInstance, &deviceGroupCount, nullptr);

			// Allocate and initialize structures to query the device groups
			for (uint32_t i = 0; i < deviceGroupCount; ++i)
			{
				props[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES_KHR;
				props[i].pNext = NULL;
			}
			vkRes = vkEnumeratePhysicalDeviceGroups(pRenderer->pVkInstance, &deviceGroupCount, props);
			ASSERT(VK_SUCCESS == vkRes);

			// If the first device group has more than one physical device. create
			// a logical device using all of the physical devices.
			for (uint32_t i = 0; i < deviceGroupCount; ++i)
			{
				if (props[i].physicalDeviceCount > 1)
				{
					deviceGroupInfo.physicalDeviceCount = props[i].physicalDeviceCount;
					deviceGroupInfo.pPhysicalDevices = props[i].physicalDevices;
					pRenderer->linkedNodeCount = deviceGroupInfo.physicalDeviceCount;
					break;
				}
			}
		}
#endif
		if (pRenderer->linkedNodeCount < 2)
		{
			pRenderer->gpuMode = SG_GPU_MODE_SINGLE;
		}

		uint32_t gpuCount = 0;

		vkRes = vkEnumeratePhysicalDevices(pRenderer->pVkInstance, &gpuCount, nullptr);
		ASSERT(VK_SUCCESS == vkRes);
		if (!gpuCount)
			SG_LOG_ERROR("No supported gpu!");
		ASSERT(gpuCount);

		VkPhysicalDevice* gpus = (VkPhysicalDevice*)alloca(gpuCount * sizeof(VkPhysicalDevice));
		VkPhysicalDeviceProperties2* gpuProperties = (VkPhysicalDeviceProperties2*)alloca(gpuCount * sizeof(VkPhysicalDeviceProperties2));

		VkPhysicalDeviceMemoryProperties* gpuMemoryProperties = (VkPhysicalDeviceMemoryProperties*)alloca(gpuCount * sizeof(VkPhysicalDeviceMemoryProperties));
		VkPhysicalDeviceFeatures2KHR* gpuFeatures = (VkPhysicalDeviceFeatures2KHR*)alloca(gpuCount * sizeof(VkPhysicalDeviceFeatures2KHR));

		VkQueueFamilyProperties** queueFamilyProperties = (VkQueueFamilyProperties**)alloca(gpuCount * sizeof(VkQueueFamilyProperties*));
		uint32_t* queueFamilyPropertyCount = (uint32_t*)alloca(gpuCount * sizeof(uint32_t));

		vkRes = vkEnumeratePhysicalDevices(pRenderer->pVkInstance, &gpuCount, gpus);
		ASSERT(VK_SUCCESS == vkRes);

		// Select discrete gpus first
		// if we have multiple discrete gpus prefer with bigger VRAM size
		// to find VRAM in vulkan, loop through all the heaps and find if the
		// heap has the DEVICE_LOCAL_BIT flag set
		typedef bool (*DeviceBetterFunc)(uint32_t, uint32_t, const GPUSettings*, const VkPhysicalDeviceProperties2*, const VkPhysicalDeviceMemoryProperties*);
		DeviceBetterFunc isDeviceBetter = [](
			uint32_t testIndex, uint32_t refIndex,
			const GPUSettings* gpuSettings,
			const VkPhysicalDeviceProperties2* gpuProperties,
			const VkPhysicalDeviceMemoryProperties* gpuMemoryProperties)
		{
			const GPUSettings& testSettings = gpuSettings[testIndex];
			const GPUSettings& refSettings = gpuSettings[refIndex];

			// first test the preset level
			if (testSettings.gpuVendorPreset.presetLevel != refSettings.gpuVendorPreset.presetLevel)
				return testSettings.gpuVendorPreset.presetLevel > refSettings.gpuVendorPreset.presetLevel;

			// next test discrete vs integrated/software
			const VkPhysicalDeviceProperties& testProps = gpuProperties[testIndex].properties;
			const VkPhysicalDeviceProperties& refProps = gpuProperties[refIndex].properties;

			// if first is a discrete gpu and second is not discrete (integrated, software, ...), always prefer first
			if (testProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && refProps.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				return true;

			// if first is not a discrete gpu (integrated, software, ...) and second is a discrete gpu, always prefer second
			if (testProps.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && refProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				return false;
			}

			// Compare by VRAM if both gpu's are of same type (integrated vs discrete)
			if (testProps.vendorID == refProps.vendorID && testProps.deviceID == refProps.deviceID)
			{
				const VkPhysicalDeviceMemoryProperties& testMemoryProps = gpuMemoryProperties[testIndex];
				const VkPhysicalDeviceMemoryProperties& refMemoryProps = gpuMemoryProperties[refIndex];
				//if presets are the same then sort by vram size
				VkDeviceSize totalTestVram = 0;
				VkDeviceSize totalRefVram = 0;
				for (uint32_t i = 0; i < testMemoryProps.memoryHeapCount; ++i)
				{
					if (VK_MEMORY_HEAP_DEVICE_LOCAL_BIT & testMemoryProps.memoryHeaps[i].flags)
						totalTestVram += testMemoryProps.memoryHeaps[i].size;
				}
				for (uint32_t i = 0; i < refMemoryProps.memoryHeapCount; ++i)
				{
					if (VK_MEMORY_HEAP_DEVICE_LOCAL_BIT & refMemoryProps.memoryHeaps[i].flags)
						totalRefVram += refMemoryProps.memoryHeaps[i].size;
				}

				return totalTestVram >= totalRefVram;
			}

			return false;
		};

		uint32_t gpuIndex = UINT32_MAX;
		GPUSettings* gpuSettings = (GPUSettings*)alloca(gpuCount * sizeof(GPUSettings));

		for (uint32_t i = 0; i < gpuCount; ++i)
		{
			gpuProperties[i] = {};
			gpuMemoryProperties[i] = {};
			gpuFeatures[i] = {};
			queueFamilyProperties[i] = nullptr;
			queueFamilyPropertyCount[i] = 0;

			// get memory properties
			vkGetPhysicalDeviceMemoryProperties(gpus[i], &gpuMemoryProperties[i]);

			// get features
			gpuFeatures[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;

#if VK_EXT_fragment_shader_interlock
			VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT fragmentShaderInterlockFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT };
			gpuFeatures[i].pNext = &fragmentShaderInterlockFeatures;
#endif
			vkGetPhysicalDeviceFeatures2(gpus[i], &gpuFeatures[i]);

			// Get device properties
			VkPhysicalDeviceSubgroupProperties subgroupProperties = {};
			subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
			subgroupProperties.pNext = nullptr;
			gpuProperties[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
			gpuProperties[i].pNext = &subgroupProperties;
#ifdef ANDROID
			vkGetPhysicalDeviceProperties2KHR(gpus[i], &gpuProperties[i]);
#else
			vkGetPhysicalDeviceProperties2(gpus[i], &gpuProperties[i]);
#endif

			// get queue family properties
			vkGetPhysicalDeviceQueueFamilyProperties(gpus[i], &queueFamilyPropertyCount[i], nullptr);
			queueFamilyProperties[i] = (VkQueueFamilyProperties*)sg_calloc(queueFamilyPropertyCount[i], sizeof(VkQueueFamilyProperties));
			vkGetPhysicalDeviceQueueFamilyProperties(gpus[i], &queueFamilyPropertyCount[i], queueFamilyProperties[i]);

			gpuSettings[i] = {};
			gpuSettings[i].uniformBufferAlignment = (uint32_t)gpuProperties[i].properties.limits.minUniformBufferOffsetAlignment;
			gpuSettings[i].uploadBufferTextureAlignment = (uint32_t)gpuProperties[i].properties.limits.optimalBufferCopyOffsetAlignment;
			gpuSettings[i].uploadBufferTextureRowAlignment = (uint32_t)gpuProperties[i].properties.limits.optimalBufferCopyRowPitchAlignment;
			gpuSettings[i].maxVertexInputBindings = gpuProperties[i].properties.limits.maxVertexInputBindings;
			gpuSettings[i].multiDrawIndirect = gpuProperties[i].properties.limits.maxDrawIndirectCount > 1;

			gpuSettings[i].waveLaneCount = subgroupProperties.subgroupSize;
			gpuSettings[i].waveOpsSupportFlags = SG_WAVE_OPS_SUPPORT_FLAG_NONE;
			if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT)
				gpuSettings[i].waveOpsSupportFlags |= SG_WAVE_OPS_SUPPORT_FLAG_BASIC_BIT;
			if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_VOTE_BIT)
				gpuSettings[i].waveOpsSupportFlags |= SG_WAVE_OPS_SUPPORT_FLAG_VOTE_BIT;
			if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT)
				gpuSettings[i].waveOpsSupportFlags |= SG_WAVE_OPS_SUPPORT_FLAG_ARITHMETIC_BIT;
			if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT)
				gpuSettings[i].waveOpsSupportFlags |= SG_WAVE_OPS_SUPPORT_FLAG_BALLOT_BIT;
			if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT)
				gpuSettings[i].waveOpsSupportFlags |= SG_WAVE_OPS_SUPPORT_FLAG_SHUFFLE_BIT;
			if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT)
				gpuSettings[i].waveOpsSupportFlags |= SG_WAVE_OPS_SUPPORT_FLAG_SHUFFLE_RELATIVE_BIT;
			if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_CLUSTERED_BIT)
				gpuSettings[i].waveOpsSupportFlags |= SG_WAVE_OPS_SUPPORT_FLAG_CLUSTERED_BIT;
			if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_QUAD_BIT)
				gpuSettings[i].waveOpsSupportFlags |= SG_WAVE_OPS_SUPPORT_FLAG_QUAD_BIT;
			if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_PARTITIONED_BIT_NV)
				gpuSettings[i].waveOpsSupportFlags |= SG_WAVE_OPS_SUPPORT_FLAG_PARTITIONED_BIT_NV;

#if VK_EXT_fragment_shader_interlock
			gpuSettings[i].ROVsSupported = (bool)fragmentShaderInterlockFeatures.fragmentShaderPixelInterlock;
#endif
			gpuSettings[i].tessellationSupported = gpuFeatures[i].features.tessellationShader;
			gpuSettings[i].geometryShaderSupported = gpuFeatures[i].features.geometryShader;

			// save vendor and model Id as string
			sprintf(gpuSettings[i].gpuVendorPreset.modelId, "%#x", gpuProperties[i].properties.deviceID);
			sprintf(gpuSettings[i].gpuVendorPreset.vendorId, "%#x", gpuProperties[i].properties.vendorID);
			strncpy(
				gpuSettings[i].gpuVendorPreset.gpuName, gpuProperties[i].properties.deviceName,
				SG_MAX_GPU_VENDOR_STRING_LENGTH);

			// TODO: Fix once vulkan adds support for revision ID
			strncpy(gpuSettings[i].gpuVendorPreset.revisionId, "0x00", SG_MAX_GPU_VENDOR_STRING_LENGTH);
			//gpuSettings[i].gpuVendorPreset.presetLevel = get_GPU_preset_level(
			//	gpuSettings[i].gpuVendorPreset.vendorId, gpuSettings[i].gpuVendorPreset.modelId,
			//	gpuSettings[i].gpuVendorPreset.revisionId);
			gpuSettings[i].gpuVendorPreset.presetLevel = SG_GPU_PRESET_ULTRA;

			SG_LOG_INFO("GPU[%i] detected. Vendor ID: %x, Model ID: %x, Preset: %s, GPU Name: %s", i,
				gpuSettings[i].gpuVendorPreset.vendorId,
				gpuSettings[i].gpuVendorPreset.modelId,
				preset_Level_to_string(gpuSettings[i].gpuVendorPreset.presetLevel),
				gpuSettings[i].gpuVendorPreset.gpuName);

			// Check that gpu supports at least graphics
			if (gpuIndex == UINT32_MAX || isDeviceBetter(i, gpuIndex, gpuSettings, gpuProperties, gpuMemoryProperties))
			{
				uint32_t count = queueFamilyPropertyCount[i];
				VkQueueFamilyProperties* properties = queueFamilyProperties[i];

				//select if graphics queue is available
				for (uint32_t j = 0; j < count; j++)
				{
					//get graphics queue family
					if (properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
					{
						gpuIndex = i;
						break;
					}
				}
			}
		}

#if defined(AUTOMATED_TESTING) && defined(ACTIVE_TESTING_GPU) && !defined(__ANDROID__) && !defined(NX64)
		selectActiveGpu(gpuSettings, &gpuIndex, gpuCount);
#endif

		// If we don't own the instance or device, then we need to set the gpuIndex to the correct physical device
#if defined(VK_USE_DISPATCH_TABLES)
		gpuIndex = UINT32_MAX;
		for (uint32_t i = 0; i < gpuCount; i++)
		{
			if (gpus[i] == pRenderer->pVkActiveGPU)
			{
				gpuIndex = i;
			}
		}
#endif

		if (VK_PHYSICAL_DEVICE_TYPE_CPU == gpuProperties[gpuIndex].properties.deviceType)
		{
			SG_LOG_ERROR("The only available GPU is of type VK_PHYSICAL_DEVICE_TYPE_CPU. Early exiting");
			ASSERT(false);
			return false;
		}

		ASSERT(gpuIndex != UINT32_MAX);
		pRenderer->pVkActiveGPU = gpus[gpuIndex];
		pRenderer->pVkActiveGPUProperties = (VkPhysicalDeviceProperties2*)sg_malloc(sizeof(VkPhysicalDeviceProperties2));
		pRenderer->pActiveGpuSettings = (GPUSettings*)sg_malloc(sizeof(GPUSettings));
		*pRenderer->pVkActiveGPUProperties = gpuProperties[gpuIndex];
		*pRenderer->pActiveGpuSettings = gpuSettings[gpuIndex];
		ASSERT(VK_NULL_HANDLE != pRenderer->pVkActiveGPU);

		SG_LOG_INFO("GPU[%d] is selected as default GPU", gpuIndex);
		SG_LOG_INFO("Name of selected gpu: %s", pRenderer->pActiveGpuSettings->gpuVendorPreset.gpuName);
		SG_LOG_INFO("Vendor id of selected gpu: %s", pRenderer->pActiveGpuSettings->gpuVendorPreset.vendorId);
		SG_LOG_INFO("Model id of selected gpu: %s", pRenderer->pActiveGpuSettings->gpuVendorPreset.modelId);
		SG_LOG_INFO("Preset of selected gpu: %s", preset_Level_to_string(pRenderer->pActiveGpuSettings->gpuVendorPreset.presetLevel));

		uint32_t layerCount = 0;
		uint32_t extCount = 0;
		vkEnumerateDeviceLayerProperties(pRenderer->pVkActiveGPU, &layerCount, nullptr);
		vkEnumerateDeviceExtensionProperties(pRenderer->pVkActiveGPU, nullptr, &extCount, nullptr);

		auto* layers = (VkLayerProperties*)alloca(sizeof(VkLayerProperties) * layerCount);
		vkEnumerateDeviceLayerProperties(pRenderer->pVkActiveGPU, &layerCount, layers);

		auto* exts = (VkExtensionProperties*)alloca(sizeof(VkExtensionProperties) * extCount);
		vkEnumerateDeviceExtensionProperties(pRenderer->pVkActiveGPU, NULL, &extCount, exts);

#if defined(SG_ENUM_VK_EXT_AND_LAYER)
		for (uint32_t i = 0; i < layerCount; ++i)
		{
			internal_log(SG_LOG_TYPE_INFO, layers[i].layerName, "vkdevice-layer");
			if (strcmp(layers[i].layerName, "VK_LAYER_RENDERDOC_Capture") == 0)
				gRenderDocLayerEnabled = true;
		}

		for (uint32_t i = 0; i < extCount; ++i)
		{
			internal_log(SG_LOG_TYPE_INFO, exts[i].extensionName, "vkdevice-ext");
		}
#endif

		uint32_t extension_count = 0;
		bool     dedicatedAllocationExtension = false;
		bool     memoryReq2Extension = false;
		bool     externalMemoryExtension = false;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
		bool externalMemoryWin32Extension = false;
#endif
		// Standalone extensions
		{
			const char* layerName = nullptr;
			uint32_t initialCount = sizeof(gVkWantedDeviceExtensions) / sizeof(gVkWantedDeviceExtensions[0]);
			const uint32_t userRequestedCount = (uint32_t)pDesc->deviceExtensionCount;
			eastl::vector<const char*>   wantedDeviceExtensions(initialCount + userRequestedCount);
			for (uint32_t i = 0; i < initialCount; ++i)
			{
				wantedDeviceExtensions[i] = gVkWantedDeviceExtensions[i];
			}
			for (uint32_t i = 0; i < userRequestedCount; ++i)
			{
				wantedDeviceExtensions[initialCount + i] = pDesc->ppDeviceExtensions[i];
			}
			const uint32_t wantedExtensionCount = (uint32_t)wantedDeviceExtensions.size();
			uint32_t count = 0;
			vkEnumerateDeviceExtensionProperties(pRenderer->pVkActiveGPU, layerName, &count, nullptr);

			if (count > 0)
			{
				VkExtensionProperties* properties = (VkExtensionProperties*)sg_calloc(count, sizeof(*properties));
				ASSERT(properties != nullptr);
				vkEnumerateDeviceExtensionProperties(pRenderer->pVkActiveGPU, layerName, &count, properties);
				for (uint32_t j = 0; j < count; ++j)
				{
					for (uint32_t k = 0; k < wantedExtensionCount; ++k)
					{
						if (strcmp(wantedDeviceExtensions[k], properties[j].extensionName) == 0)
						{
							deviceExtensionCache[extension_count++] = wantedDeviceExtensions[k];

#ifndef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
							if (strcmp(wantedDeviceExtensions[k], VK_EXT_DEBUG_MARKER_EXTENSION_NAME) == 0)
								gDebugMarkerSupport = true;
#endif
							if (strcmp(wantedDeviceExtensions[k], VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) == 0)
								dedicatedAllocationExtension = true;
							if (strcmp(wantedDeviceExtensions[k], VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) == 0)
								memoryReq2Extension = true;
							if (strcmp(wantedDeviceExtensions[k], VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME) == 0)
								externalMemoryExtension = true;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
							if (strcmp(wantedDeviceExtensions[k], VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME) == 0)
								externalMemoryWin32Extension = true;
#endif
#ifdef VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME
							if (strcmp(wantedDeviceExtensions[k], VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME) == 0)
								gDrawIndirectCountExtension = true;
#endif
							if (strcmp(wantedDeviceExtensions[k], VK_AMD_DRAW_INDIRECT_COUNT_EXTENSION_NAME) == 0)
								gAMDDrawIndirectCountExtension = true;
							if (strcmp(wantedDeviceExtensions[k], VK_AMD_GCN_SHADER_EXTENSION_NAME) == 0)
								gAMDGCNShaderExtension = true;
							if (strcmp(wantedDeviceExtensions[k], VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) == 0)
								gDescriptorIndexingExtension = true;
#ifdef VK_NV_RAY_TRACING_SPEC_VERSION
							if (strcmp(wantedDeviceExtensions[k], VK_NV_RAY_TRACING_EXTENSION_NAME) == 0)
							{
								pRenderer->raytracingExtension = 1;
								gNVRayTracingExtension = true;
								gDescriptorTypeRangeSize = SG_DESCRIPTOR_TYPE_RANGE_SIZE;
							}
#endif
#ifdef VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME
							if (strcmp(wantedDeviceExtensions[k], VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME) == 0)
							{
								gYCbCrExtension = true;
							}
#endif
#ifdef USE_NSIGHT_AFTERMATH
							if (strcmp(wantedDeviceExtensions[k], VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME) == 0)
							{
								pRenderer->mDiagnosticCheckPointsSupport = true;
							}
							if (strcmp(wantedDeviceExtensions[k], VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME) == 0)
							{
								pRenderer->mDiagnosticsConfigSupport = true;
							}
#endif
							break;
						}
					}
				}
				SG_SAFE_FREE((void*)properties);
			}
		}

#if !defined(VK_USE_DISPATCH_TABLES)
		// Add more extensions here
#if VK_EXT_fragment_shader_interlock
		VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT fragmentShaderInterlockFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT };
		VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexingFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT, &fragmentShaderInterlockFeatures };
#else
		VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexingFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT };
#endif // VK_EXT_fragment_shader_interlock

		VkPhysicalDeviceFeatures2KHR gpuFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };
		gpuFeatures2.pNext = &descriptorIndexingFeatures;

#ifdef VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME
		VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcrFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES };
		if (gYCbCrExtension)
		{
			gpuFeatures2.pNext = &ycbcrFeatures;
			ycbcrFeatures.pNext = &descriptorIndexingFeatures;
		}
#endif

#ifndef NX64
		vkGetPhysicalDeviceFeatures2(pRenderer->pVkActiveGPU, &gpuFeatures2);
#else
		vkGetPhysicalDeviceFeatures2(pRenderer->pVkActiveGPU, &gpuFeatures2);
#endif

		// need a queue_priorite for each queue in the queue family we create
		uint32_t queueFamiliesCount = queueFamilyPropertyCount[gpuIndex];
		VkQueueFamilyProperties* queueFamiliesProperties = queueFamilyProperties[gpuIndex];
		auto** queueFamilyPriorities = (float**)alloca(queueFamiliesCount * sizeof(float*));
		uint32_t queueCreateInfosCount = 0;
		VkDeviceQueueCreateInfo queueCreateInfos[4] = {};

		const uint32_t maxQueueFlag = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT | VK_QUEUE_PROTECTED_BIT;
		pRenderer->pAvailableQueueCount = (uint32_t**)sg_malloc(pRenderer->linkedNodeCount * sizeof(uint32_t*));
		pRenderer->pUsedQueueCount = (uint32_t**)sg_malloc(pRenderer->linkedNodeCount * sizeof(uint32_t*));
		for (uint32_t i = 0; i < pRenderer->linkedNodeCount; ++i)
		{
			pRenderer->pAvailableQueueCount[i] = (uint32_t*)sg_calloc(maxQueueFlag, sizeof(uint32_t));
			pRenderer->pUsedQueueCount[i] = (uint32_t*)sg_calloc(maxQueueFlag, sizeof(uint32_t));
		}

		for (uint32_t i = 0; i < queueFamiliesCount; i++)
		{
			uint32_t queueCount = queueFamiliesProperties[i].queueCount;
			if (queueCount > 0)
			{
				queueFamilyPriorities[i] = (float*)alloca(queueCount * sizeof(float));
				memset(queueFamilyPriorities[i], 0, queueCount * sizeof(float));

				// request only one queue of each type if mRequestAllAvailableQueues is not set to true
				if (queueCount > 1 && !pDesc->requestAllAvailableQueues)
				{
					queueCount = 1;
				}

				queueCreateInfos[queueCreateInfosCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueCreateInfos[queueCreateInfosCount].pNext = nullptr;
				queueCreateInfos[queueCreateInfosCount].flags = 0;
				queueCreateInfos[queueCreateInfosCount].queueFamilyIndex = i;
				queueCreateInfos[queueCreateInfosCount].queueCount = queueCount;
				queueCreateInfos[queueCreateInfosCount].pQueuePriorities = queueFamilyPriorities[i];
				queueCreateInfosCount++;

				for (uint32_t n = 0; n < pRenderer->linkedNodeCount; ++n)
				{
					pRenderer->pAvailableQueueCount[n][queueFamiliesProperties[i].queueFlags] = queueCount;
				}
			}
		}

		SG_DECLARE_ZERO(VkDeviceCreateInfo, createInfo);
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pNext = &gpuFeatures2;
		createInfo.flags = 0;
		createInfo.queueCreateInfoCount = queueCreateInfosCount;
		createInfo.pQueueCreateInfos = queueCreateInfos;
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
		createInfo.enabledExtensionCount = extension_count;
		createInfo.ppEnabledExtensionNames = deviceExtensionCache;
		createInfo.pEnabledFeatures = nullptr;

#if defined(USE_NSIGHT_AFTERMATH)
		if (pRenderer->mDiagnosticCheckPointsSupport && pRenderer->mDiagnosticsConfigSupport)
		{
			pRenderer->mAftermathSupport = true;
			LOGF(LogLevel::eINFO, "Successfully loaded Aftermath extensions");
		}

		if (pRenderer->mAftermathSupport)
		{
			DECLARE_ZERO(VkDeviceDiagnosticsConfigCreateInfoNV, diagnostics_createInfo);
			diagnostics_createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_createInfo_NV;
			diagnostics_createInfo.flags =
				VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV |
				VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV |
				VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV;
			gpuFeatures2.pNext = &diagnostics_createInfo;
			diagnostics_createInfo.pNext = &descriptorIndexingFeatures;
			// Enable Nsight Aftermath GPU crash dump creation.
			// This needs to be done before the Vulkan device is created.
			CreateAftermathTracker(pRenderer->name, &pRenderer->mAftermathTracker);
		}
#endif

		// Add Device Group Extension if requested and available
#if VK_KHR_device_group_creation
		if (pRenderer->gpuMode == SG_GPU_MODE_LINKED)
		{
			createInfo.pNext = &deviceGroupInfo;
		}
#endif
		SG_CHECK_VKRESULT(vkCreateDevice(pRenderer->pVkActiveGPU, &createInfo, nullptr, &pRenderer->pVkDevice));

		//#if !defined(NX64)
		//	// Load vulkan device functions to bypass loader
		//	volkLoadDevice(pRenderer->pVkDevice);
		//#endif
#endif

		gDedicatedAllocationExtension = dedicatedAllocationExtension && memoryReq2Extension;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
		gExternalMemoryExtension = externalMemoryExtension && externalMemoryWin32Extension;
#endif

		if (gDedicatedAllocationExtension)
		{
			SG_LOG_INFO("Successfully loaded Dedicated Allocation extension");
		}

		if (gExternalMemoryExtension)
		{
			SG_LOG_INFO("Successfully loaded External Memory extension");
		}

//#ifdef VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME
//		if (gDrawIndirectCountExtension)
//		{
//			pfnVkCmdDrawIndirectCountKHR = vkCmdDrawIndirectCountKHR;
//			pfnVkCmdDrawIndexedIndirectCountKHR = vkCmdDrawIndexedIndirectCountKHR;
//			SG_LOG_INFO("Successfully loaded Draw Indirect extension");
//		}
//		else if (gAMDDrawIndirectCountExtension)
//#endif
//		{
//			pfnVkCmdDrawIndirectCountKHR = vkCmdDrawIndirectCountAMD;
//			pfnVkCmdDrawIndexedIndirectCountKHR = vkCmdDrawIndexedIndirectCountAMD;
//			SG_LOG_INFO("Successfully loaded AMD Draw Indirect extension");
//		}

		if (gAMDGCNShaderExtension)
		{
			SG_LOG_INFO("Successfully loaded AMD GCN Shader extension");
		}

		if (gDescriptorIndexingExtension)
		{
			SG_LOG_INFO("Successfully loaded Descriptor Indexing extension");
		}

		if (gNVRayTracingExtension)
		{
			SG_LOG_INFO("Successfully loaded Nvidia Ray Tracing extension");
		}

#ifdef USE_DEBUG_UTILS_EXTENSION
		gDebugMarkerSupport = (&vkCmdBeginDebugUtilsLabelEXT) && (&vkCmdEndDebugUtilsLabelEXT) && (&vkCmdInsertDebugUtilsLabelEXT) && (&vkSetDebugUtilsObjectNameEXT);
#endif

		for (uint32_t i = 0; i < gpuCount; ++i)
			SG_SAFE_FREE(queueFamilyProperties[i]);

		utils_caps_builder(pRenderer);

		return true;
	}

	static void remove_device(Renderer* pRenderer)
	{
		vkDestroyDevice(pRenderer->pVkDevice, nullptr);
		SG_SAFE_FREE(pRenderer->pActiveGpuSettings);
		SG_SAFE_FREE(pRenderer->pVkActiveGPUProperties);

#if defined(USE_NSIGHT_AFTERMATH)
		if (pRenderer->aftermathSupport)
		{
			DestroyAftermathTracker(&pRenderer->mAftermathTracker);
		}
#endif
	}

#pragma endregion (Internal Init Function)

#pragma region (Debug Naming)
#ifdef SG_ENABLE_GRAPHICS_DEBUG

	// TODO: may be add more objects
#ifdef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
	void util_set_object_name(Renderer* pRenderer, VkDevice pDevice, uint64_t handle, VkObjectType type, const char* name)
	{
#if defined(SG_ENABLE_GRAPHICS_DEBUG)
		if (gDebugMarkerSupport)
		{
			VkDebugUtilsObjectNameInfoEXT nameInfo = {};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = type;
			nameInfo.objectHandle = handle;
			nameInfo.pObjectName = name;

			VkResult res = VK_RESULT_MAX_ENUM;
			auto func = (PFN_vkSetDebugUtilsObjectNameEXT)
				vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkSetDebugUtilsObjectNameEXT"); // load this function from extensions
			if (func != nullptr)
			{
				res = func(pDevice, &nameInfo);
			}
			else
			{
				res = VK_ERROR_EXTENSION_NOT_PRESENT;
			}

			if (VK_SUCCESS != res)
			{
				internal_log(SG_LOG_TYPE_ERROR, "vkSetDebugUtilsObjectNameEXT failed - disabling Vulkan debug callbacks",
					"internal_vk_init_instance");
			}
			//vkSetDebugUtilsObjectNameEXT(pDevice, &nameInfo);
		}
#endif
	}
#else
	void util_set_object_name(VkDevice pDevice, uint64_t handle, VkDebugReportObjectTypeEXT type, const char* name)
	{
#if defined(ENABLE_GRAPHICS_DEBUG)
		if (gDebugMarkerSupport)
		{
			VkDebugMarkerObjectNameInfoEXT nameInfo = {};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = type;
			nameInfo.object = (uint64_t)handle;
			nameInfo.pObjectName = name;
			vkDebugMarkerSetObjectNameEXT(pDevice, &nameInfo);
		}
#endif
	}
#endif

	void set_buffer_name(Renderer* pRenderer, Buffer* pBuffer, const char* name)
	{
		ASSERT(pRenderer);
		ASSERT(pBuffer);
		ASSERT(name);

#ifdef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
		util_set_object_name(pRenderer, pRenderer->pVkDevice, (uint64_t)pBuffer->pVkBuffer, VK_OBJECT_TYPE_BUFFER, name);
#else
		util_set_object_name(pRenderer, pRenderer->pVkDevice, (uint64_t)pBuffer->pVkBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, name);
#endif
	}

	void set_texture_name(Renderer* pRenderer, Texture* pTexture, const char* name)
	{
		ASSERT(pRenderer);
		ASSERT(pTexture);
		ASSERT(name);

#ifdef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
		util_set_object_name(pRenderer, pRenderer->pVkDevice, (uint64_t)pTexture->pVkImage, VK_OBJECT_TYPE_IMAGE, name);
#else
		util_set_object_name(pRenderer, pRenderer->pVkDevice, (uint64_t)pTexture->pVkImage, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, name);
#endif
	}

	void set_render_target_name(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* name)
	{
		set_texture_name(pRenderer, pRenderTarget->pTexture, name);
	}

	void set_pipeline_name(Renderer* pRenderer, Pipeline* pPipeline, const char* name)
	{
		ASSERT(pRenderer);
		ASSERT(pPipeline);
		ASSERT(name);

#ifdef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
		util_set_object_name(pRenderer, pRenderer->pVkDevice, (uint64_t)pPipeline->pVkPipeline, VK_OBJECT_TYPE_PIPELINE, name);
#else
		util_set_object_name(pRenderer, pRenderer->pVkDevice, (uint64_t)pPipeline->pVkPipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, name);
#endif
	}

#endif
#pragma endregion (Debug Naming)

#pragma region (Render Pass And Frame Buffer)

	typedef struct RenderPassCreateDesc
	{
		TinyImageFormat* pColorFormats;
		const LoadActionType* pLoadActionsColor;
		bool* pSrgbValues;
		uint32_t              renderTargetCount;
		SampleCount           sampleCount;
		TinyImageFormat       depthStencilFormat;
		LoadActionType        loadActionDepth;
		LoadActionType        loadActionStencil;
	} RenderPassCreateDesc;

	typedef struct RenderPass
	{
		VkRenderPass	     pRenderPass;
		RenderPassCreateDesc desc;
	} RenderPass;

	typedef struct FrameBufferCreateDesc
	{
		RenderPass* pRenderPass;
		RenderTarget** ppRenderTargets;
		RenderTarget* pDepthStencil;
		uint32_t* pColorArraySlices;
		uint32_t* pColorMipSlices;
		uint32_t  depthArraySlice;
		uint32_t  depthMipSlice;
		uint32_t  renderTargetCount;
	} FrameBufferCreateDesc;

	typedef struct FrameBuffer
	{
		VkFramebuffer pFramebuffer;
		uint32_t      width;
		uint32_t      height;
		uint32_t      arraySize;
	} FrameBuffer;

	static void add_render_pass(Renderer* pRenderer, const RenderPassCreateDesc* pDesc, RenderPass** ppRenderPass)
	{
		RenderPass* pRenderPass = (RenderPass*)sg_calloc(1, sizeof(*pRenderPass));
		pRenderPass->desc = *pDesc;

		// add render pass
		ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

		uint32_t colorAttachmentCount = pDesc->renderTargetCount;
		uint32_t depthAttachmentCount = (pDesc->depthStencilFormat != TinyImageFormat_UNDEFINED) ? 1 : 0;

		VkAttachmentDescription* attachments = nullptr;
		VkAttachmentReference* colorAttachmentRefs = nullptr;
		VkAttachmentReference* depthStencilAttachmentRef = nullptr;

		VkSampleCountFlagBits sampleCount = util_to_vk_sample_count(pDesc->sampleCount);

		// fill out attachment descriptions and references
		{
			attachments = (VkAttachmentDescription*)sg_calloc(colorAttachmentCount + depthAttachmentCount, sizeof(*attachments));
			ASSERT(attachments);

			if (colorAttachmentCount > 0)
			{
				colorAttachmentRefs = (VkAttachmentReference*)sg_calloc(colorAttachmentCount, sizeof(*colorAttachmentRefs));
				ASSERT(colorAttachmentRefs);
			}
			if (depthAttachmentCount > 0)
			{
				depthStencilAttachmentRef = (VkAttachmentReference*)sg_calloc(1, sizeof(*depthStencilAttachmentRef));
				ASSERT(depthStencilAttachmentRef);
			}

			// color
			for (uint32_t i = 0; i < colorAttachmentCount; ++i)
			{
				const uint32_t ssidx = i;

				// descriptions
				attachments[ssidx].flags = 0;
				attachments[ssidx].format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->pColorFormats[i]);
				attachments[ssidx].samples = sampleCount;
				attachments[ssidx].loadOp = pDesc->pLoadActionsColor ? gVkAttachmentLoadOpTranslator[pDesc->pLoadActionsColor[i]] : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachments[ssidx].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				attachments[ssidx].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachments[ssidx].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
				attachments[ssidx].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				attachments[ssidx].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				// references
				colorAttachmentRefs[i].attachment = ssidx;
				colorAttachmentRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}
		}

		// Depth stencil
		if (depthAttachmentCount > 0)
		{
			uint32_t idx = colorAttachmentCount;
			attachments[idx].flags = 0;
			attachments[idx].format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->depthStencilFormat);
			attachments[idx].samples = sampleCount;
			attachments[idx].loadOp = gVkAttachmentLoadOpTranslator[pDesc->loadActionDepth];
			attachments[idx].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[idx].stencilLoadOp = gVkAttachmentLoadOpTranslator[pDesc->loadActionStencil];
			attachments[idx].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

			attachments[idx].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachments[idx].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			depthStencilAttachmentRef[0].attachment = idx;
			depthStencilAttachmentRef[0].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		SG_DECLARE_ZERO(VkSubpassDescription, subpass);
		subpass.flags = 0;
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.inputAttachmentCount = 0;
		subpass.pInputAttachments = nullptr;
		subpass.colorAttachmentCount = colorAttachmentCount;
		subpass.pColorAttachments = colorAttachmentRefs;
		subpass.pResolveAttachments = nullptr;
		subpass.pDepthStencilAttachment = depthStencilAttachmentRef;
		subpass.preserveAttachmentCount = 0;
		subpass.pPreserveAttachments = nullptr;

		uint32_t attachmentCount = colorAttachmentCount;
		attachmentCount += depthAttachmentCount;

		SG_DECLARE_ZERO(VkRenderPassCreateInfo, createInfo);
		createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.attachmentCount = attachmentCount;
		createInfo.pAttachments = attachments;
		createInfo.subpassCount = 1;
		createInfo.pSubpasses = &subpass;
		createInfo.dependencyCount = 0;
		createInfo.pDependencies = nullptr;

		SG_CHECK_VKRESULT(vkCreateRenderPass(pRenderer->pVkDevice, &createInfo, nullptr, &(pRenderPass->pRenderPass)));

		SG_SAFE_FREE(attachments);
		SG_SAFE_FREE(colorAttachmentRefs);
		SG_SAFE_FREE(depthStencilAttachmentRef);

		*ppRenderPass = pRenderPass;
	}

	static void remove_render_pass(Renderer* pRenderer, RenderPass* pRenderPass)
	{
		vkDestroyRenderPass(pRenderer->pVkDevice, pRenderPass->pRenderPass, nullptr);
		SG_SAFE_FREE(pRenderPass);
	}

	static void add_framebuffer(Renderer* pRenderer, const FrameBufferCreateDesc* pDesc, FrameBuffer** ppFrameBuffer)
	{
		ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

		FrameBuffer* pFrameBuffer = (FrameBuffer*)sg_calloc(1, sizeof(*pFrameBuffer));
		ASSERT(pFrameBuffer);

		uint32_t colorAttachmentCount = pDesc->renderTargetCount;
		uint32_t depthAttachmentCount = (pDesc->pDepthStencil) ? 1 : 0;

		if (colorAttachmentCount)
		{
			pFrameBuffer->width = pDesc->ppRenderTargets[0]->width;
			pFrameBuffer->height = pDesc->ppRenderTargets[0]->height;
			if (pDesc->pColorArraySlices)
				pFrameBuffer->arraySize = 1;
			else			  
				pFrameBuffer->arraySize = pDesc->ppRenderTargets[0]->arraySize;
		}
		else
		{
			pFrameBuffer->width = pDesc->pDepthStencil->width;
			pFrameBuffer->height = pDesc->pDepthStencil->height;
			if (pDesc->depthArraySlice != -1)
				pFrameBuffer->arraySize = 1;
			else
				pFrameBuffer->arraySize = pDesc->pDepthStencil->arraySize;
		}

		if (colorAttachmentCount && pDesc->ppRenderTargets[0]->depth > 1)
		{
			pFrameBuffer->arraySize = pDesc->ppRenderTargets[0]->depth;
		}

		// add frame buffer
		uint32_t attachmentCount = colorAttachmentCount;
		attachmentCount += depthAttachmentCount;

		VkImageView* pImageViews = (VkImageView*)sg_calloc(attachmentCount, sizeof(*pImageViews));
		ASSERT(pImageViews);

		VkImageView* iterAttachments = pImageViews;
		// Color
		for (uint32_t i = 0; i < pDesc->renderTargetCount; ++i)
		{
			if (!pDesc->pColorMipSlices && !pDesc->pColorArraySlices)
			{
				*iterAttachments = pDesc->ppRenderTargets[i]->pVkDescriptor;
				++iterAttachments;
			}
			else
			{
				uint32_t handle = 0;
				if (pDesc->pColorMipSlices)
				{
					if (pDesc->pColorArraySlices)
						handle = pDesc->pColorMipSlices[i] * pDesc->ppRenderTargets[i]->arraySize + pDesc->pColorArraySlices[i];
					else
						handle = pDesc->pColorMipSlices[i];
				}
				else if (pDesc->pColorArraySlices)
				{
					handle = pDesc->pColorArraySlices[i];
				}
				*iterAttachments = pDesc->ppRenderTargets[i]->pVkSliceDescriptors[handle];
				++iterAttachments;
			}
		}
		// Depth/stencil
		if (pDesc->pDepthStencil)
		{
			if (-1 == pDesc->depthMipSlice && -1 == pDesc->depthArraySlice)
			{
				*iterAttachments = pDesc->pDepthStencil->pVkDescriptor;
				++iterAttachments;
			}
			else
			{
				uint32_t handle = 0;
				if (pDesc->depthMipSlice != -1)
				{
					if (pDesc->depthArraySlice != -1)
						handle = pDesc->depthMipSlice * pDesc->pDepthStencil->arraySize + pDesc->depthArraySlice;
					else
						handle = pDesc->depthMipSlice;
				}
				else if (pDesc->depthArraySlice != -1)
				{
					handle = pDesc->depthArraySlice;
				}
				*iterAttachments = pDesc->pDepthStencil->pVkSliceDescriptors[handle];
				++iterAttachments;
			}
		}

		SG_DECLARE_ZERO(VkFramebufferCreateInfo, createInfo);
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.pNext = NULL;
		createInfo.flags = 0;
		createInfo.renderPass = pDesc->pRenderPass->pRenderPass;
		createInfo.attachmentCount = attachmentCount;
		createInfo.pAttachments = pImageViews;
		createInfo.width = pFrameBuffer->width;
		createInfo.height = pFrameBuffer->height;
		createInfo.layers = pFrameBuffer->arraySize;
		SG_CHECK_VKRESULT(vkCreateFramebuffer(pRenderer->pVkDevice, &createInfo, nullptr, &(pFrameBuffer->pFramebuffer)));
		SG_SAFE_FREE(pImageViews);

		*ppFrameBuffer = pFrameBuffer;
	}

	static void remove_framebuffer(Renderer* pRenderer, FrameBuffer* pFrameBuffer)
	{
		ASSERT(pRenderer);
		ASSERT(pFrameBuffer);

		vkDestroyFramebuffer(pRenderer->pVkDevice, pFrameBuffer->pFramebuffer, nullptr);
		SG_SAFE_FREE(pFrameBuffer);
	}

#pragma endregion (Render Pass And Frame Buffer)

#pragma region (Descriptor Pool)

	typedef struct DescriptorPool
	{
		VkDevice                        pVkDevice;
		VkDescriptorPool                pCurrentPool;
		VkDescriptorPoolSize*		    pPoolSizes;
		eastl::vector<VkDescriptorPool> descriptorPools;
		uint32_t                        poolSizeCount;
		uint32_t                        nudescriptorsets;
		uint32_t                        usedDescriptorSetCount;
		VkDescriptorPoolCreateFlags     flags;
		Mutex*					        pMutex;

		DescriptorPool(uint32_t num)
			: pPoolSizes(nullptr), poolSizeCount(num), nudescriptorsets(0),
			usedDescriptorSetCount(0), pMutex(nullptr)
		{
			pVkDevice = {};
			pCurrentPool = {};
			descriptorPools = {};
			flags = {};
		}
	} DescriptorPool;

	static void add_descriptor_pool(Renderer* pRenderer, uint32_t nudescriptorsets, VkDescriptorPoolCreateFlags flags, VkDescriptorPoolSize* pPoolSizes,
		uint32_t numPoolSizes, DescriptorPool** ppPool)
	{
		DescriptorPool* pPool = nullptr;
		pPool = sg_placement_new<DescriptorPool>(sg_malloc(sizeof(DescriptorPool)), 0);
		pPool->descriptorPools.resize(numPoolSizes);
		pPool->flags = flags;
		pPool->nudescriptorsets = nudescriptorsets;
		pPool->usedDescriptorSetCount = 0;
		pPool->pVkDevice = pRenderer->pVkDevice;

		pPool->pMutex = (Mutex*)sg_calloc(1, sizeof(Mutex));
		pPool->pMutex->Init();

		pPool->poolSizeCount = numPoolSizes;
		pPool->pPoolSizes = (VkDescriptorPoolSize*)sg_calloc(numPoolSizes, sizeof(VkDescriptorPoolSize));
		for (uint32_t i = 0; i < numPoolSizes; ++i)
			pPool->pPoolSizes[i] = pPoolSizes[i];

		VkDescriptorPoolCreateInfo poolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr };
		poolCreateInfo.poolSizeCount = numPoolSizes;
		poolCreateInfo.pPoolSizes = pPoolSizes;
		poolCreateInfo.flags = flags;
		poolCreateInfo.maxSets = nudescriptorsets;

		SG_CHECK_VKRESULT(vkCreateDescriptorPool(pPool->pVkDevice, &poolCreateInfo, nullptr,
			&pPool->pCurrentPool));

		pPool->descriptorPools.emplace_back(pPool->pCurrentPool);
		*ppPool = pPool;
	}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif
	static void reset_descriptor_pool(DescriptorPool* pPool)
	{
		SG_CHECK_VKRESULT(vkResetDescriptorPool(pPool->pVkDevice, pPool->pCurrentPool, pPool->flags));
		pPool->usedDescriptorSetCount = 0;
	}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

	static void remove_descriptor_pool(Renderer* pRenderer, DescriptorPool* pPool)
	{
		for (uint32_t i = 0; i < (uint32_t)pPool->descriptorPools.size(); ++i)
			vkDestroyDescriptorPool(pRenderer->pVkDevice, pPool->descriptorPools[i], nullptr);

		pPool->descriptorPools.~vector();

		pPool->pMutex->Destroy();
		sg_free(pPool->pMutex);
		SG_SAFE_FREE(pPool->pPoolSizes);
		SG_SAFE_FREE(pPool);
	}

	static void allocate_descriptor_sets(DescriptorPool* pPool, const VkDescriptorSetLayout* pLayouts, VkDescriptorSet** pSets, uint32_t nudescriptorsets)
	{
		// need a lock since vkAllocateDescriptorSets needs to be externally synchronized
		// This is fine since this will only happen during Init time
		MutexLock lock(*pPool->pMutex);

		SG_DECLARE_ZERO(VkDescriptorSetAllocateInfo, allocInfo);
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.pNext = nullptr;
		allocInfo.descriptorPool = pPool->pCurrentPool;
		allocInfo.descriptorSetCount = nudescriptorsets;
		allocInfo.pSetLayouts = pLayouts;

		VkResult vkRes = vkAllocateDescriptorSets(pPool->pVkDevice, &allocInfo, *pSets);
		if (VK_SUCCESS != vkRes)
		{
			// try to recreate the pool
			VkDescriptorPool pDescriptorPool = VK_NULL_HANDLE;

			VkDescriptorPoolCreateInfo poolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO ,nullptr };
			poolCreateInfo.poolSizeCount = pPool->poolSizeCount;
			poolCreateInfo.pPoolSizes = pPool->pPoolSizes;
			poolCreateInfo.flags = pPool->flags;
			poolCreateInfo.maxSets = pPool->nudescriptorsets;

			VkResult res = vkCreateDescriptorPool(pPool->pVkDevice, &poolCreateInfo, nullptr, &pDescriptorPool);
			ASSERT(VK_SUCCESS == res);

			pPool->descriptorPools.emplace_back(pDescriptorPool);

			pPool->pCurrentPool = pDescriptorPool;
			pPool->usedDescriptorSetCount = 0;

			allocInfo.descriptorPool = pPool->pCurrentPool;
			vkRes = vkAllocateDescriptorSets(pPool->pVkDevice, &allocInfo, *pSets);
		}
		ASSERT(VK_SUCCESS == vkRes);
		pPool->usedDescriptorSetCount += nudescriptorsets;
	}

#pragma endregion (Descriptor Pool)

#pragma region (Descriptor Set)

	using DescriptorNameToIndexMap = eastl::string_hash_map<uint32_t>;

	union DescriptorUpdateData
	{
		VkDescriptorImageInfo imageInfo;
		VkDescriptorBufferInfo bufferInfo;
		VkBufferView bufferView;
	};

	struct SizeOffset
	{
		uint32_t size;
		uint32_t offset;
	};

	typedef struct DescriptorIndexMap
	{
		DescriptorNameToIndexMap mMap;
	} DescriptorIndexMap;

	// each root signature contains its descriptor by name
	static const DescriptorInfo* get_descriptor(const RootSignature* pRootSignature, const char* name)
	{
		DescriptorNameToIndexMap::const_iterator it = pRootSignature->descriptorNameToIndexMap->mMap.find(name);
		if (it != pRootSignature->descriptorNameToIndexMap->mMap.end())
		{
			return &pRootSignature->descriptors[it->second];
		}
		else
		{
			SG_LOG_ERROR("Invalid descriptor param (%s)", name);
			return nullptr;
		}
	}

	void add_descriptor_set(Renderer* pRenderer, const DescriptorSetCreateDesc* pDesc, DescriptorSet** ppDescriptorSet)
	{
		ASSERT(pRenderer);
		ASSERT(pDesc);
		ASSERT(ppDescriptorSet);

		const RootSignature* pRootSignature = pDesc->pRootSignature;
		const DescriptorUpdateFrequency updateFreq = pDesc->updateFrequency;
		const uint32_t nodeIndex = pDesc->nodeIndex;
		const uint32_t descriptorCount = pRootSignature->vkCumulativeDescriptorCounts[updateFreq];
		const uint32_t dynamicOffsetCount = pRootSignature->vkDynamicDescriptorCounts[updateFreq];

		uint32_t totalSize = sizeof(DescriptorSet);
		if (VK_NULL_HANDLE != pRootSignature->vkDescriptorSetLayouts[updateFreq])
		{
			totalSize += pDesc->maxSets * sizeof(VkDescriptorSet);
			totalSize += pDesc->maxSets * sizeof(DescriptorUpdateData*);
			totalSize += pDesc->maxSets * descriptorCount * sizeof(DescriptorUpdateData);
		}
		if (dynamicOffsetCount)
		{
			ASSERT(1 == dynamicOffsetCount);
			totalSize += pDesc->maxSets * sizeof(SizeOffset);
		}

		DescriptorSet* pDescriptorSet = (DescriptorSet*)sg_calloc_memalign(1, alignof(DescriptorSet), totalSize);

		pDescriptorSet->pRootSignature = pRootSignature;
		pDescriptorSet->updateFrequency = updateFreq;
		pDescriptorSet->dynamicOffsetCount = dynamicOffsetCount;
		pDescriptorSet->nodeIndex = nodeIndex;
		pDescriptorSet->maxSets = pDesc->maxSets;

		uint8_t* pMem = (uint8_t*)(pDescriptorSet + 1);
		pDescriptorSet->pHandles = (VkDescriptorSet*)pMem;

		if (VK_NULL_HANDLE != pRootSignature->vkDescriptorSetLayouts[updateFreq])
		{
			pMem += pDesc->maxSets * sizeof(VkDescriptorSet);

			pDescriptorSet->ppUpdateData = (DescriptorUpdateData**)pMem;
			pMem += pDesc->maxSets * sizeof(DescriptorUpdateData*);

			VkDescriptorSetLayout* pLayouts = (VkDescriptorSetLayout*)alloca(pDesc->maxSets * sizeof(VkDescriptorSetLayout));
			VkDescriptorSet** pHandles = (VkDescriptorSet**)alloca(pDesc->maxSets * sizeof(VkDescriptorSet*));

			for (uint32_t i = 0; i < pDesc->maxSets; ++i)
			{
				pLayouts[i] = pRootSignature->vkDescriptorSetLayouts[updateFreq];
				pHandles[i] = &pDescriptorSet->pHandles[i];

				pDescriptorSet->ppUpdateData[i] = (DescriptorUpdateData*)pMem;
				pMem += descriptorCount * sizeof(DescriptorUpdateData);
				memcpy(pDescriptorSet->ppUpdateData[i], pRootSignature->pUpdateTemplateData[updateFreq][pDescriptorSet->nodeIndex], descriptorCount * sizeof(DescriptorUpdateData));
			}

			allocate_descriptor_sets(pRenderer->pDescriptorPool, pLayouts, pHandles, pDesc->maxSets);
		}
		else
		{
			SG_LOG_ERROR("NULL Descriptor Set Layout for update frequency %u. Cannot allocate descriptor set", (uint32_t)updateFreq);
			ASSERT(false && "NULL Descriptor Set Layout for update frequency. Cannot allocate descriptor set");
		}

		if (pDescriptorSet->dynamicOffsetCount)
		{
			ASSERT(1 == pDescriptorSet->dynamicOffsetCount);
			pDescriptorSet->pDynamicSizeOffsets = (SizeOffset*)pMem;
			pMem += pDescriptorSet->maxSets * sizeof(SizeOffset);
		}

		*ppDescriptorSet = pDescriptorSet;
	}

	void remove_descriptor_set(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
	{
		ASSERT(pRenderer);
		ASSERT(pDescriptorSet);

		SG_SAFE_FREE(pDescriptorSet);
	}

	void update_descriptor_set(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
	{
#if defined(SG_ENABLE_GRAPHICS_DEBUG)
#define VALIDATE_DESCRIPTOR(descriptor,...)																\
	if (!(descriptor))																					\
	{																									\
		eastl::string msg = __FUNCTION__ + eastl::string(" : ") + eastl::string().sprintf(__VA_ARGS__);	\
		SG_LOG_ERROR(msg.c_str());																		\
		failed_assert(__FILE__, __LINE__, msg.c_str());													\
		continue;																						\
	}
#else
#define VALIDATE_DESCRIPTOR(descriptor,...)
#endif

		ASSERT(pRenderer);
		ASSERT(pDescriptorSet);
		ASSERT(pDescriptorSet->pHandles);
		ASSERT(index < pDescriptorSet->maxSets);

		const RootSignature* pRootSignature = pDescriptorSet->pRootSignature;
		DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)pDescriptorSet->updateFrequency;
		DescriptorUpdateData* pUpdateData = pDescriptorSet->ppUpdateData[index];
		bool update = false;

#ifdef SG_ENABLE_RAYTRACING
		VkWriteDescriptorSet* raytracingWrites = NULL;
		VkWriteDescriptorSetAccelerationStructureNV* raytracingWritesNV = NULL;
		uint32_t raytracingWriteCount = 0;

		if (pRootSignature->vkRaytracingDescriptorCounts[updateFreq])
		{
			raytracingWrites = (VkWriteDescriptorSet*)alloca(pRootSignature->vkRaytracingDescriptorCounts[updateFreq] * sizeof(VkWriteDescriptorSet));
			raytracingWritesNV = (VkWriteDescriptorSetAccelerationStructureNV*)alloca(pRootSignature->vkRaytracingDescriptorCounts[updateFreq] * sizeof(VkWriteDescriptorSetAccelerationStructureNV));
		}
#endif

		for (uint32_t i = 0; i < count; ++i)
		{
			const DescriptorData* pParam = pParams + i;
			uint32_t paramIndex = pParam->index;

			VALIDATE_DESCRIPTOR(pParam->name || (paramIndex != -1), "DescriptorData has NULL name and invalid index");

			const DescriptorInfo* pDesc = (paramIndex != -1) ? (pRootSignature->descriptors + paramIndex) : get_descriptor(pRootSignature, pParam->name);
			if (paramIndex != -1)
			{
				VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param index (%u)", paramIndex);
			}
			else
			{
				VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param name (%s)", pParam->name);
			}

			const DescriptorType type = (DescriptorType)pDesc->type;
			const uint32_t arrayCount = eastl::max(1U, pParam->count);

			VALIDATE_DESCRIPTOR(pDesc->updateFrequency == updateFreq,
				"Descriptor (%s) - Mismatching update frequency and set index", pDesc->name);

			switch (type)
			{
			case SG_DESCRIPTOR_TYPE_SAMPLER:
			{
				// Index is invalid when descriptor is a static sampler
				VALIDATE_DESCRIPTOR(pDesc->indexInParent != -1,
					"Trying to update a static sampler (%s). All static samplers must be set in addRootSignature and cannot be updated later",
					pDesc->name);

				VALIDATE_DESCRIPTOR(pParam->ppSamplers, "NULL Sampler (%s)", pDesc->name);

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppSamplers[arr], "NULL Sampler (%s [%u] )", pDesc->name, arr);

					pUpdateData[pDesc->handleIndex + arr].imageInfo = { pParam->ppSamplers[arr]->pVkSampler, VK_NULL_HANDLE };
					update = true;
				}
				break;
			}
			case SG_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			{
				VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL Texture (%s)", pDesc->name);

				DescriptorNameToIndexMap::const_iterator it = pRootSignature->descriptorNameToIndexMap->mMap.find(pDesc->name);
				if (it == pRootSignature->descriptorNameToIndexMap->mMap.end())
				{
					SG_LOG_ERROR("No Static Sampler called (%s)", pDesc->name);
					ASSERT(false);
				}

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->name, arr);

					pUpdateData[pDesc->handleIndex + arr].imageInfo =
					{
						nullptr,										// Sampler
						pParam->ppTextures[arr]->pVkSRVDescriptor,		// Image View
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL		// Image Layout 
					};

					update = true;
				}
				break;
			}
			case SG_DESCRIPTOR_TYPE_TEXTURE:
			{
				VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL Texture (%s)", pDesc->name);

				if (!pParam->bindStencilResource)
				{
					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->name, arr);

						pUpdateData[pDesc->handleIndex + arr].imageInfo =
						{
							VK_NULL_HANDLE,                                    // Sampler
							pParam->ppTextures[arr]->pVkSRVDescriptor,         // Image View
							VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL           // Image Layout
						};

						update = true;
					}
				}
				else
				{
					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->name, arr);

						pUpdateData[pDesc->handleIndex + arr].imageInfo =
						{
							VK_NULL_HANDLE,                                    // Sampler
							pParam->ppTextures[arr]->pVkSRVStencilDescriptor, // Image View
							VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL           // Image Layout
						};

						update = true;
					}
				}
				break;
			}
			case SG_DESCRIPTOR_TYPE_RW_TEXTURE:
			{
				VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL RW Texture (%s)", pDesc->name);

				if (pParam->bindMipChain)
				{
					VALIDATE_DESCRIPTOR(pParam->ppTextures[0], "NULL RW Texture (%s)", pDesc->name);

					for (uint32_t arr = 0; arr < pParam->ppTextures[0]->mipLevels; ++arr)
					{
						pUpdateData[pDesc->handleIndex + arr].imageInfo = {
							VK_NULL_HANDLE,                                          // Sampler
							pParam->ppTextures[0]->pVkUAVDescriptors[arr],			 // Image View
							VK_IMAGE_LAYOUT_GENERAL                                  // Image Layout
						};

						update = true;
					}
				}
				else
				{
					const uint32_t mipSlice = pParam->UAVMipSlice;

					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL RW Texture (%s [%u] )", pDesc->name, arr);
						VALIDATE_DESCRIPTOR(
							mipSlice < pParam->ppTextures[arr]->mipLevels, "Descriptor : (%s [%u] ) Mip Slice (%u) exceeds mip levels (%u)",
							pDesc->name, arr, mipSlice, pParam->ppTextures[arr]->mipLevels);

						pUpdateData[pDesc->handleIndex + arr].imageInfo = {
							VK_NULL_HANDLE,                                          // Sampler
							pParam->ppTextures[arr]->pVkUAVDescriptors[mipSlice],    // Image View
							VK_IMAGE_LAYOUT_GENERAL                                  // Image Layout
						};

						update = true;
					}
				}
				break;
			}
			case SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			{
				if (pDesc->vkType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
				{
					VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Uniform Buffer (%s)", pDesc->name);
					VALIDATE_DESCRIPTOR(pParam->ppBuffers[0], "NULL Uniform Buffer (%s [%u] )", pDesc->name, 0);
					VALIDATE_DESCRIPTOR(arrayCount == 1, "Descriptor (%s) : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC does not support arrays", pDesc->name);
					VALIDATE_DESCRIPTOR(pParam->sizes, "Descriptor (%s) : Must provide pSizes for VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC", pDesc->name);
					VALIDATE_DESCRIPTOR(pParam->sizes[0] > 0, "Descriptor (%s) - pSizes[%u] is zero", pDesc->name, 0);
					VALIDATE_DESCRIPTOR(pParam->sizes[0] <= pRenderer->pVkActiveGPUProperties->properties.limits.maxUniformBufferRange,
						"Descriptor (%s) - pSizes[%u] is %ull which exceeds max size %u", pDesc->name, 0,
						pParam->sizes[0],
						pRenderer->pVkActiveGPUProperties->properties.limits.maxUniformBufferRange);

					pDescriptorSet->pDynamicSizeOffsets[index].offset = pParam->offsets ? (uint32_t)pParam->offsets[0] : 0;
					pUpdateData[pDesc->handleIndex + 0].bufferInfo =
					{
						pParam->ppBuffers[0]->pVkBuffer,
						pParam->ppBuffers[0]->offset,
						pParam->sizes[0]
					};

					// If this is a different size we have to update the VkDescriptorBufferInfo::range so a call to vkUpdateDescriptorSet is necessary
					if (pParam->sizes[0] != (uint32_t)pDescriptorSet->pDynamicSizeOffsets[index].size)
					{
						pDescriptorSet->pDynamicSizeOffsets[index].size = (uint32_t)pParam->sizes[0];
						update = true;
					}

					break;
				}
			case SG_DESCRIPTOR_TYPE_BUFFER:
			case SG_DESCRIPTOR_TYPE_BUFFER_RAW:
			case SG_DESCRIPTOR_TYPE_RW_BUFFER:
			case SG_DESCRIPTOR_TYPE_RW_BUFFER_RAW:
			{
				VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Buffer (%s)", pDesc->name);

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Buffer (%s [%u] )", pDesc->name, arr);

					pUpdateData[pDesc->handleIndex + arr].bufferInfo =
					{
						pParam->ppBuffers[arr]->pVkBuffer,
						pParam->ppBuffers[arr]->offset,
						VK_WHOLE_SIZE
					};
					if (pParam->offsets)
					{
						VALIDATE_DESCRIPTOR(pParam->sizes, "Descriptor (%s) - pSizes must be provided with pOffsets", pDesc->name);
						VALIDATE_DESCRIPTOR(pParam->sizes[arr] > 0, "Descriptor (%s) - pSizes[%u] is zero", pDesc->name, arr);
						VALIDATE_DESCRIPTOR(pParam->sizes[arr] <= pRenderer->pVkActiveGPUProperties->properties.limits.maxUniformBufferRange,
							"Descriptor (%s) - pSizes[%u] is %ull which exceeds max size %u", pDesc->name, arr,
							pParam->sizes[arr],
							pRenderer->pVkActiveGPUProperties->properties.limits.maxUniformBufferRange);

						pUpdateData[pDesc->handleIndex + arr].bufferInfo.offset = pParam->offsets[arr];
						pUpdateData[pDesc->handleIndex + arr].bufferInfo.range = pParam->sizes[arr];
					}

					update = true;
				}

			}
			break;
			}
			case SG_DESCRIPTOR_TYPE_TEXEL_BUFFER:
			{
				VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Texel Buffer (%s)", pDesc->name);

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Texel Buffer (%s [%u] )", pDesc->name, arr);
					pUpdateData[pDesc->handleIndex + arr].bufferView = pParam->ppBuffers[arr]->pVkUniformTexelView;
					update = true;
				}

				break;
			}
			case SG_DESCRIPTOR_TYPE_RW_TEXEL_BUFFER:
			{
				VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL RW Texel Buffer (%s)", pDesc->name);

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL RW Texel Buffer (%s [%u] )", pDesc->name, arr);
					pUpdateData[pDesc->handleIndex + arr].bufferView = pParam->ppBuffers[arr]->pVkStorageTexelView;
					update = true;
				}

				break;
			}
#ifdef SG_ENABLE_RAYTRACING
			case SG_DESCRIPTOR_TYPE_RAY_TRACING:
			{
				VALIDATE_DESCRIPTOR(pParam->ppAccelerationStructures, "NULL Acceleration Structure (%s)", pDesc->name);

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppAccelerationStructures[arr], "Acceleration Structure (%s [%u] )", pDesc->name, arr);

					VkWriteDescriptorSet* pWrite = raytracingWrites + raytracingWriteCount;
					VkWriteDescriptorSetAccelerationStructureNV* pWriteNV = raytracingWritesNV + raytracingWriteCount;

					pWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					pWrite->pNext = pWriteNV;
					pWrite->dstSet = pDescriptorSet->pHandles[index];
					pWrite->descriptorCount = 1;
					pWrite->descriptorType = (VkDescriptorType)pDesc->vkType;
					pWrite->dstArrayElement = arr;
					pWrite->dstBinding = pDesc->reg;

					vk_FillRaytracingDescriptorData(pParam->ppAccelerationStructures[arr], pWriteNV);

					++raytracingWriteCount;
				}
				break;
			}
#endif
			default:
				break;
			}
		}

		// If this was called to just update a dynamic offset skip the update
		if (update)
			vkUpdateDescriptorSetWithTemplate(pRenderer->pVkDevice, pDescriptorSet->pHandles[index],
				pRootSignature->updateTemplates[updateFreq], pUpdateData);

#ifdef SG_ENABLE_RAYTRACING
		// Raytracing Update Descriptor Set since it does not support update template
		if (raytracingWriteCount)
			vkUpdateDescriptorSets(pRenderer->pVkDevice, raytracingWriteCount, raytracingWrites, 0, NULL);
#endif
	}

#pragma endregion (Descriptor Set)

#pragma region (Buffer Function)

	void add_buffer(Renderer* pRenderer, const BufferCreateDesc* pDesc, Buffer** ppBuffer)
	{
		ASSERT(pRenderer);
		ASSERT(pDesc);
		ASSERT(pDesc->size > 0);
		ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

		auto* pBuffer = (Buffer*)sg_calloc_memalign(1, alignof(Buffer), sizeof(Buffer));
		ASSERT(ppBuffer);

		uint64_t allocationSize = pDesc->size;
		// Align the buffer size to multiples of the dynamic uniform buffer minimum size
		if (pDesc->descriptors & SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		{
			uint64_t minAlignment = pRenderer->pActiveGpuSettings->uniformBufferAlignment;
			allocationSize = round_up(allocationSize, minAlignment);
		}

		SG_DECLARE_ZERO(VkBufferCreateInfo, addInfo);
		addInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		addInfo.pNext = NULL;
		addInfo.flags = 0;
		addInfo.size = allocationSize;
		addInfo.usage = util_to_vk_buffer_usage(pDesc->descriptors, pDesc->format != TinyImageFormat_UNDEFINED);
		addInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		addInfo.queueFamilyIndexCount = 0;
		addInfo.pQueueFamilyIndices = nullptr;

		// buffer can be used as dst in a transfer command (Uploading data to a storage buffer, readback query data)
		if (pDesc->memoryUsage == SG_RESOURCE_MEMORY_USAGE_GPU_ONLY || pDesc->memoryUsage == SG_RESOURCE_MEMORY_USAGE_GPU_TO_CPU)
			addInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		const bool linkedMultiGpu = (pRenderer->gpuMode == SG_GPU_MODE_LINKED && (pDesc->pSharedNodeIndices || pDesc->nodeIndex));

		VmaAllocationCreateInfo vmaMemReqs = { 0 };
		vmaMemReqs.usage = (VmaMemoryUsage)pDesc->memoryUsage;
		vmaMemReqs.flags = 0;
		if (pDesc->flags & SG_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT)
			vmaMemReqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		if (pDesc->flags & SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT)
			vmaMemReqs.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
		if (linkedMultiGpu)
			vmaMemReqs.flags |= VMA_ALLOCATION_CREATE_DONT_BIND_BIT;

		VmaAllocationInfo allocInfo = {};
		SG_CHECK_VKRESULT(vmaCreateBuffer(pRenderer->vmaAllocator, &addInfo, &vmaMemReqs,
			&pBuffer->pVkBuffer, &pBuffer->vkAllocation, &allocInfo));

		pBuffer->pCpuMappedAddress = allocInfo.pMappedData;

		// buffer to be used on multiple GPUs
		if (linkedMultiGpu)
		{
			VmaAllocationInfo allocInfo = {};
			vmaGetAllocationInfo(pRenderer->vmaAllocator, pBuffer->vkAllocation, &allocInfo);
			// Set all the device indices to the index of the device where we will create the buffer
			uint32_t* pIndices = (uint32_t*)alloca(pRenderer->linkedNodeCount * sizeof(uint32_t));
			util_calculate_device_indices(pRenderer, pDesc->nodeIndex, pDesc->pSharedNodeIndices, pDesc->sharedNodeIndexCount, pIndices);

			// #TODO : Move this to the Vulkan memory allocator
			VkBindBufferMemoryInfoKHR bindInfo = { VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO_KHR };
			VkBindBufferMemoryDeviceGroupInfoKHR bindDeviceGroup = { VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO_KHR };
			bindDeviceGroup.deviceIndexCount = pRenderer->linkedNodeCount;
			bindDeviceGroup.pDeviceIndices = pIndices;
			bindInfo.buffer = pBuffer->pVkBuffer;
			bindInfo.memory = allocInfo.deviceMemory;
			bindInfo.memoryOffset = allocInfo.offset;
			bindInfo.pNext = &bindDeviceGroup;
			SG_CHECK_VKRESULT(vkBindBufferMemory2(pRenderer->pVkDevice, 1, &bindInfo));
		}

		// set descriptor data
		if ((pDesc->descriptors & SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER) || (pDesc->descriptors & SG_DESCRIPTOR_TYPE_BUFFER) ||
			(pDesc->descriptors & SG_DESCRIPTOR_TYPE_RW_BUFFER))
		{
			if ((pDesc->descriptors & SG_DESCRIPTOR_TYPE_BUFFER) || (pDesc->descriptors & SG_DESCRIPTOR_TYPE_RW_BUFFER))
			{
				pBuffer->offset = pDesc->structStride * pDesc->firstElement;
			}
		}

		if (addInfo.usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT)
		{
			VkBufferViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, nullptr };
			viewInfo.buffer = pBuffer->pVkBuffer;
			viewInfo.flags = 0;
			viewInfo.format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->format);
			viewInfo.offset = pDesc->firstElement * pDesc->structStride;
			viewInfo.range = pDesc->elementCount * pDesc->structStride;
			VkFormatProperties formatProps = {};
			vkGetPhysicalDeviceFormatProperties(pRenderer->pVkActiveGPU, viewInfo.format, &formatProps);
			if (!(formatProps.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT))
			{
				SG_LOG_WARNING("Failed to create uniform texel buffer view for format %u", (uint32_t)pDesc->format);
			}
			else
			{
				SG_CHECK_VKRESULT(vkCreateBufferView(pRenderer->pVkDevice, &viewInfo, nullptr, &pBuffer->pVkUniformTexelView));
			}
		}
		if (addInfo.usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)
		{
			VkBufferViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, nullptr };
			viewInfo.buffer = pBuffer->pVkBuffer;
			viewInfo.flags = 0;
			viewInfo.format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->format);
			viewInfo.offset = pDesc->firstElement * pDesc->structStride;
			viewInfo.range = pDesc->elementCount * pDesc->structStride;
			VkFormatProperties formatProps = {};
			vkGetPhysicalDeviceFormatProperties(pRenderer->pVkActiveGPU, viewInfo.format, &formatProps);
			if (!(formatProps.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
			{
				SG_LOG_WARNING("Failed to create storage texel buffer view for format %u", (uint32_t)pDesc->format);
			}
			else
			{
				SG_CHECK_VKRESULT(vkCreateBufferView(pRenderer->pVkDevice, &viewInfo, nullptr, &pBuffer->pVkStorageTexelView));
			}
		}

#if defined(SG_ENABLE_GRAPHICS_DEBUG)
		if (pDesc->name)
		{
			set_buffer_name(pRenderer, pBuffer, pDesc->name);
		}
#endif

		pBuffer->size = (uint32_t)pDesc->size;
		pBuffer->memoryUsage = pDesc->memoryUsage;
		pBuffer->nodeIndex = pDesc->nodeIndex;
		pBuffer->descriptors = pDesc->descriptors;

		*ppBuffer = pBuffer;
	}

	void remove_buffer(Renderer* pRenderer, Buffer* pBuffer)
	{
		ASSERT(pRenderer);
		ASSERT(pBuffer);
		ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
		ASSERT(VK_NULL_HANDLE != pBuffer->pVkBuffer);

		if (pBuffer->pVkUniformTexelView)
		{
			vkDestroyBufferView(pRenderer->pVkDevice, pBuffer->pVkUniformTexelView, nullptr);
			pBuffer->pVkUniformTexelView = VK_NULL_HANDLE;
		}
		if (pBuffer->pVkStorageTexelView)
		{
			vkDestroyBufferView(pRenderer->pVkDevice, pBuffer->pVkStorageTexelView, nullptr);
			pBuffer->pVkStorageTexelView = VK_NULL_HANDLE;
		}

		vmaDestroyBuffer(pRenderer->vmaAllocator, pBuffer->pVkBuffer, pBuffer->vkAllocation);
		SG_SAFE_FREE(pBuffer);
	}

	void map_buffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
	{
		ASSERT(pBuffer->memoryUsage != SG_RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to map non-cpu accessible resource");

		VkResult vk_res = vmaMapMemory(pRenderer->vmaAllocator, pBuffer->vkAllocation, &pBuffer->pCpuMappedAddress);
		ASSERT(vk_res == VK_SUCCESS);

		if (pRange)
		{
			pBuffer->pCpuMappedAddress = ((uint8_t*)pBuffer->pCpuMappedAddress + pRange->offset);
		}
	}

	void unmap_buffer(Renderer* pRenderer, Buffer* pBuffer)
	{
		ASSERT(pBuffer->memoryUsage != SG_RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to unmap non-cpu accessible resource");

		vmaUnmapMemory(pRenderer->vmaAllocator, pBuffer->vkAllocation);
		pBuffer->pCpuMappedAddress = nullptr;
	}

#pragma endregion (Buffer)

#pragma region (Texture)
		
	void add_texture(Renderer* pRenderer, const TextureCreateDesc* pDesc, Texture** ppTexture)
	{
		ASSERT(pRenderer);
		ASSERT(pDesc && pDesc->width && pDesc->height && (pDesc->depth || pDesc->arraySize));

		if (pDesc->sampleCount > SG_SAMPLE_COUNT_1 && pDesc->mipLevels > 1)
		{
			SG_LOG_ERROR("Multi-Sampled textures cannot have mip maps");
			ASSERT(false);
			return;
		}

		size_t totalSize = sizeof(Texture);
		totalSize += (pDesc->descriptors & SG_DESCRIPTOR_TYPE_RW_TEXTURE ? (pDesc->mipLevels * sizeof(VkImageView)) : 0);
		auto* pTexture = (Texture*)sg_calloc_memalign(1, alignof(Texture), totalSize);
		ASSERT(pTexture);

		if (pDesc->descriptors & SG_DESCRIPTOR_TYPE_RW_TEXTURE)
			pTexture->pVkUAVDescriptors = (VkImageView*)(pTexture + 1);

		if (pDesc->nativeHandle && !(pDesc->flags & SG_TEXTURE_CREATION_FLAG_IMPORT_BIT))
		{
			pTexture->ownsImage = false;
			pTexture->pVkImage = (VkImage)pDesc->nativeHandle;
		}
		else
		{
			pTexture->ownsImage = true;
		}

		VkImageUsageFlags additionalFlags = 0;
		if (pDesc->startState & SG_RESOURCE_STATE_RENDER_TARGET)
			additionalFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		else if (pDesc->startState & SG_RESOURCE_STATE_DEPTH_WRITE)
			additionalFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		VkImageType imageType = VK_IMAGE_TYPE_MAX_ENUM;
		if (pDesc->flags & SG_TEXTURE_CREATION_FLAG_FORCE_2D)
		{
			ASSERT(pDesc->depth == 1);
			imageType = VK_IMAGE_TYPE_2D;
		}
		else if (pDesc->flags & SG_TEXTURE_CREATION_FLAG_FORCE_3D)
		{
			imageType = VK_IMAGE_TYPE_3D;
		}
		else
		{
			if (pDesc->depth > 1)
				imageType = VK_IMAGE_TYPE_3D;
			else if (pDesc->height > 1)
				imageType = VK_IMAGE_TYPE_2D;
			else
				imageType = VK_IMAGE_TYPE_1D;
		}

		DescriptorType descriptors = pDesc->descriptors;
		bool cubemapRequired = (SG_DESCRIPTOR_TYPE_TEXTURE_CUBE == (descriptors & SG_DESCRIPTOR_TYPE_TEXTURE_CUBE));
		bool arrayRequired = false;

		const bool isPlanarFormat = TinyImageFormat_IsPlanar(pDesc->format);
		const uint32_t numOfPlanes = TinyImageFormat_NumOfPlanes(pDesc->format);
		const bool isSinglePlane = TinyImageFormat_IsSinglePlane(pDesc->format);
		ASSERT(((isSinglePlane && numOfPlanes == 1) || (!isSinglePlane && numOfPlanes > 1 && numOfPlanes <= MAX_PLANE_COUNT))
			&& "Number of planes for multi-planar formats must be 2 or 3 and for single-planar formats it must be 1.");

		if (imageType == VK_IMAGE_TYPE_3D)
			arrayRequired = true;

		if (VK_NULL_HANDLE == pTexture->pVkImage)
		{
			SG_DECLARE_ZERO(VkImageCreateInfo, createInfo);
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			createInfo.pNext = nullptr;
			createInfo.flags = 0;
			createInfo.imageType = imageType;
			createInfo.format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->format);
			createInfo.extent.width = pDesc->width;
			createInfo.extent.height = pDesc->height;
			createInfo.extent.depth = pDesc->depth;
			createInfo.mipLevels = pDesc->mipLevels;
			createInfo.arrayLayers = pDesc->arraySize;
			createInfo.samples = util_to_vk_sample_count(pDesc->sampleCount);
			createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			createInfo.usage = util_to_vk_image_usage(descriptors);
			createInfo.usage |= additionalFlags;
			createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
			createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			if (cubemapRequired)
				createInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
			if (arrayRequired)
				createInfo.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR;

			SG_DECLARE_ZERO(VkFormatProperties, formatProps);
			vkGetPhysicalDeviceFormatProperties(pRenderer->pVkActiveGPU, createInfo.format, &formatProps);
			if (isPlanarFormat) // multi-planar formats must have each plane separately bound to memory, rather than having a single memory binding for the whole image
			{
				ASSERT(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DISJOINT_BIT);
				createInfo.flags |= VK_IMAGE_CREATE_DISJOINT_BIT;
			}

			if ((VK_IMAGE_USAGE_SAMPLED_BIT & createInfo.usage) || (VK_IMAGE_USAGE_STORAGE_BIT & createInfo.usage))
			{
				// Make it easy to copy to and from textures
				createInfo.usage |= (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
			}

			ASSERT(pRenderer->pCapBits->canShaderReadFrom[pDesc->format] && "GPU shader can't read from this format");
			// verify that GPU supports this format
			VkFormatFeatureFlags formatFeatures = util_vk_image_usage_to_format_features(createInfo.usage);

			VkFormatFeatureFlags flags = formatProps.optimalTilingFeatures & formatFeatures;
			ASSERT((0 != flags) && "Format is not supported for GPU local images (i.e. not host visible images)");

			const bool linkedMultiGpu = (pRenderer->gpuMode == SG_GPU_MODE_LINKED) && (pDesc->pSharedNodeIndices || pDesc->nodeIndex);

			VmaAllocationCreateInfo memReqs = { 0 };
			if (pDesc->flags & SG_TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT)
				memReqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
			if (linkedMultiGpu)
				memReqs.flags |= VMA_ALLOCATION_CREATE_DONT_BIND_BIT;
			memReqs.usage = (VmaMemoryUsage)VMA_MEMORY_USAGE_GPU_ONLY;

			VkExternalMemoryImageCreateInfoKHR externalInfo = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR, nullptr };
#if defined(VK_USE_PLATFORM_WIN32_KHR)
			VkImportMemoryWin32HandleInfoKHR importInfo = { VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR, nullptr };
#endif
			VkExportMemoryAllocateInfoKHR exportMemoryInfo = { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR, nullptr };
			if (gExternalMemoryExtension && pDesc->flags & SG_TEXTURE_CREATION_FLAG_IMPORT_BIT)
			{
				createInfo.pNext = &externalInfo;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
				struct ImportHandleInfo
				{
					void* pHandle;
					VkExternalMemoryHandleTypeFlagBitsKHR mHandleType;
				};

				ImportHandleInfo* pHandleInfo = (ImportHandleInfo*)pDesc->nativeHandle;
				importInfo.handle = pHandleInfo->pHandle;
				importInfo.handleType = pHandleInfo->mHandleType;

				externalInfo.handleTypes = pHandleInfo->mHandleType;

				memReqs.pUserData = &importInfo;
				// Allocate external (importable / exportable) memory as dedicated memory to avoid adding unnecessary complexity to the Vulkan Memory Allocator
				memReqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
#endif
			}
			else if (gExternalMemoryExtension && pDesc->flags & SG_TEXTURE_CREATION_FLAG_EXPORT_BIT)
			{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
				exportMemoryInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
#endif

				memReqs.pUserData = &exportMemoryInfo;
				// Allocate external (importable / exportable) memory as dedicated memory to avoid adding unnecessary complexity to the Vulkan Memory Allocator
				memReqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
			}

			VmaAllocationInfo allocInfo = {};
			if (isSinglePlane)
			{
				SG_CHECK_VKRESULT(vmaCreateImage(pRenderer->vmaAllocator, &createInfo, &memReqs,
					&pTexture->pVkImage, &pTexture->vkAllocation, &allocInfo));
			}
			else // Multi-planar formats
			{
				// Create info requires the mutable format flag set for multi planar images
				// Also pass the format list for mutable formats as per recommendation from the spec
				// Might help to keep DCC enabled if we ever use this as a output format
				// DCC gets disabled when we pass mutable format bit to the create info. Passing the format list helps the driver to enable it
				VkFormat planarFormat = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->format);
				VkImageFormatListCreateInfoKHR formatList = {};
				formatList.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT;
				formatList.pNext = NULL;
				formatList.pViewFormats = &planarFormat;
				formatList.viewFormatCount = 1;

				createInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
				createInfo.pNext = &formatList;

				// Create Image
				SG_CHECK_VKRESULT(vkCreateImage(pRenderer->pVkDevice, &createInfo, nullptr, &pTexture->pVkImage));

				VkMemoryRequirements vkMemReq = {};
				uint64_t planesOffsets[MAX_PLANE_COUNT] = { 0 };
				util_get_planar_vk_image_memory_requirement(pRenderer->pVkDevice, pTexture->pVkImage, numOfPlanes, &vkMemReq, planesOffsets);

				// Allocate image memory
				VkMemoryAllocateInfo mem_allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
				mem_allocInfo.allocationSize = vkMemReq.size;
				VkPhysicalDeviceMemoryProperties memProps = {};
				vkGetPhysicalDeviceMemoryProperties(pRenderer->pVkActiveGPU, &memProps);
				mem_allocInfo.memoryTypeIndex = util_get_memory_type(vkMemReq.memoryTypeBits, memProps, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				SG_CHECK_VKRESULT(vkAllocateMemory(pRenderer->pVkDevice, &mem_allocInfo, nullptr, &pTexture->pVkDeviceMemory));

				// Bind planes to their memories
				VkBindImageMemoryInfo bindImagesMemoryInfo[MAX_PLANE_COUNT];
				VkBindImagePlaneMemoryInfo bindImagePlanesMemoryInfo[MAX_PLANE_COUNT];
				for (uint32_t i = 0; i < numOfPlanes; ++i)
				{
					VkBindImagePlaneMemoryInfo& bindImagePlaneMemoryInfo = bindImagePlanesMemoryInfo[i];
					bindImagePlaneMemoryInfo.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO;
					bindImagePlaneMemoryInfo.pNext = nullptr;
					bindImagePlaneMemoryInfo.planeAspect = (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_PLANE_0_BIT << i);

					VkBindImageMemoryInfo& bindImageMemoryInfo = bindImagesMemoryInfo[i];
					bindImageMemoryInfo.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
					bindImageMemoryInfo.pNext = &bindImagePlaneMemoryInfo;
					bindImageMemoryInfo.image = pTexture->pVkImage;
					bindImageMemoryInfo.memory = pTexture->pVkDeviceMemory;
					bindImageMemoryInfo.memoryOffset = planesOffsets[i];
				}

				SG_CHECK_VKRESULT(vkBindImageMemory2(pRenderer->pVkDevice, numOfPlanes, bindImagesMemoryInfo));
			}

			// Texture to be used on multiple GPUs
			if (linkedMultiGpu)
			{
				VmaAllocationInfo allocInfo = {};
				vmaGetAllocationInfo(pRenderer->vmaAllocator, pTexture->vkAllocation, &allocInfo);

				// set all the device indices to the index of the device where we will create the texture
				uint32_t* pIndices = (uint32_t*)alloca(pRenderer->linkedNodeCount * sizeof(uint32_t));
				util_calculate_device_indices(pRenderer, pDesc->nodeIndex, pDesc->pSharedNodeIndices, pDesc->sharedNodeIndexCount, pIndices);

				// #TODO : Move this to the Vulkan memory allocator
				VkBindImageMemoryInfoKHR            bindInfo = { VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO_KHR };
				VkBindImageMemoryDeviceGroupInfoKHR bindDeviceGroup = { VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO_KHR };
				bindDeviceGroup.deviceIndexCount = pRenderer->linkedNodeCount;
				bindDeviceGroup.pDeviceIndices = pIndices;
				bindInfo.image = pTexture->pVkImage;
				bindInfo.memory = allocInfo.deviceMemory;
				bindInfo.memoryOffset = allocInfo.offset;
				bindInfo.pNext = &bindDeviceGroup;
				SG_CHECK_VKRESULT(vkBindImageMemory2(pRenderer->pVkDevice, 1, &bindInfo));
			}
		}

		// Create image view
		VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
		switch (imageType)
		{
		case VK_IMAGE_TYPE_1D: viewType = pDesc->arraySize > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D; break;
		case VK_IMAGE_TYPE_2D:
			if (cubemapRequired)
				viewType = (pDesc->arraySize > 6) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
			else
				viewType = pDesc->arraySize > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
			break;
		case VK_IMAGE_TYPE_3D:
			if (pDesc->arraySize > 1)
			{
				SG_LOG_ERROR("Cannot support 3D Texture Array in Vulkan");
				ASSERT(false);
			}
			viewType = VK_IMAGE_VIEW_TYPE_3D;
			break;
		default: ASSERT(false && "Image Format not supported!"); break;
		}

		ASSERT(viewType != VK_IMAGE_VIEW_TYPE_MAX_ENUM && "Invalid Image View");

		VkImageViewCreateInfo srvDesc = {};
		// SRV
		srvDesc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		srvDesc.pNext = nullptr;
		srvDesc.flags = 0;
		srvDesc.image = pTexture->pVkImage;
		srvDesc.viewType = viewType;
		srvDesc.format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->format);
		srvDesc.components.r = VK_COMPONENT_SWIZZLE_R;
		srvDesc.components.g = VK_COMPONENT_SWIZZLE_G;
		srvDesc.components.b = VK_COMPONENT_SWIZZLE_B;
		srvDesc.components.a = VK_COMPONENT_SWIZZLE_A;
		srvDesc.subresourceRange.aspectMask = util_vk_determine_aspect_mask(srvDesc.format, false);
		srvDesc.subresourceRange.baseMipLevel = 0;
		srvDesc.subresourceRange.levelCount = pDesc->mipLevels;
		srvDesc.subresourceRange.baseArrayLayer = 0;
		srvDesc.subresourceRange.layerCount = pDesc->arraySize;
		pTexture->aspectMask = util_vk_determine_aspect_mask(srvDesc.format, true);

		if (pDesc->pVkSamplerYcbcrConversionInfo)
		{
			srvDesc.pNext = pDesc->pVkSamplerYcbcrConversionInfo;
		}

		if (descriptors & SG_DESCRIPTOR_TYPE_TEXTURE)
		{
			SG_CHECK_VKRESULT(vkCreateImageView(pRenderer->pVkDevice, &srvDesc, nullptr, &pTexture->pVkSRVDescriptor));
		}

		// SRV stencil
		if ((TinyImageFormat_HasStencil(pDesc->format))
			&& (descriptors & SG_DESCRIPTOR_TYPE_TEXTURE))
		{
			srvDesc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
			SG_CHECK_VKRESULT(vkCreateImageView(pRenderer->pVkDevice, &srvDesc, nullptr, &pTexture->pVkSRVStencilDescriptor));
		}

		// UAV
		if (descriptors & SG_DESCRIPTOR_TYPE_RW_TEXTURE)
		{
			VkImageViewCreateInfo uavDesc = srvDesc;
			// #NOTE : We don't support imageCube, imageCubeArray for consistency with other APIs
			// All cubemaps will be used as image2DArray for Image Load / Store ops
			if (uavDesc.viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY || uavDesc.viewType == VK_IMAGE_VIEW_TYPE_CUBE)
				uavDesc.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			uavDesc.subresourceRange.levelCount = 1;
			for (uint32_t i = 0; i < pDesc->mipLevels; ++i)
			{
				uavDesc.subresourceRange.baseMipLevel = i;
				SG_CHECK_VKRESULT(vkCreateImageView(pRenderer->pVkDevice, &uavDesc, nullptr, &pTexture->pVkUAVDescriptors[i]));
			}
		}

		pTexture->nodeIndex = pDesc->nodeIndex;
		pTexture->width = pDesc->width;
		pTexture->height = pDesc->height;
		pTexture->depth = pDesc->depth;
		pTexture->mipLevels = pDesc->mipLevels;
		pTexture->uav = pDesc->descriptors & SG_DESCRIPTOR_TYPE_RW_TEXTURE;
		pTexture->arraySizeMinusOne = pDesc->arraySize - 1;
		pTexture->format = pDesc->format;

#if defined(SG_ENABLE_GRAPHICS_DEBUG)
		if (pDesc->name)
		{
			set_texture_name(pRenderer, pTexture, pDesc->name);
		}
#endif

		*ppTexture = pTexture;
	}

	void remove_texture(Renderer* pRenderer, Texture* pTexture)
	{
		ASSERT(pRenderer);
		ASSERT(pTexture);
		ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
		ASSERT(VK_NULL_HANDLE != pTexture->pVkImage);

		if (pTexture->ownsImage)
		{
			const TinyImageFormat fmt = (TinyImageFormat)pTexture->format;
			const bool isSinglePlane = TinyImageFormat_IsSinglePlane(fmt);
			if (isSinglePlane)
			{
				vmaDestroyImage(pRenderer->vmaAllocator, pTexture->pVkImage, pTexture->vkAllocation);
			}
			else
			{
				vkDestroyImage(pRenderer->pVkDevice, pTexture->pVkImage, nullptr);
				vkFreeMemory(pRenderer->pVkDevice, pTexture->pVkDeviceMemory, nullptr);
			}
		}

		if (VK_NULL_HANDLE != pTexture->pVkSRVDescriptor)
			vkDestroyImageView(pRenderer->pVkDevice, pTexture->pVkSRVDescriptor, nullptr);
		if (VK_NULL_HANDLE != pTexture->pVkSRVStencilDescriptor)
			vkDestroyImageView(pRenderer->pVkDevice, pTexture->pVkSRVStencilDescriptor, nullptr);

		if (pTexture->pVkUAVDescriptors)
		{
			for (uint32_t i = 0; i < pTexture->mipLevels; ++i)
			{
				vkDestroyImageView(pRenderer->pVkDevice, pTexture->pVkUAVDescriptors[i], nullptr);
			}
		}

		//if (pTexture->pSvt)
		//{
		//	remove_virtual_texture(pRenderer, pTexture->pSvt);
		//}

		SG_SAFE_FREE(pTexture);
	}

#pragma endregion (Texture)

#pragma region (Shader)

	void add_shader_binary(Renderer* pRenderer, const BinaryShaderCreateDesc* pDesc, Shader** ppShaderProgram)
	{
		ASSERT(pRenderer);
		ASSERT(pDesc);
		ASSERT(ppShaderProgram);
		ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

		uint32_t counter = 0;

		size_t totalSize = sizeof(Shader);
		totalSize += sizeof(PipelineReflection);

		for (uint32_t i = 0; i < SG_SHADER_STAGE_COUNT; ++i)
		{
			ShaderStage stageMask = (ShaderStage)(1 << i);
			if (stageMask == (pDesc->stages & stageMask))
			{
				switch (stageMask)
				{
					case SG_SHADER_STAGE_VERT: totalSize += (strlen(pDesc->vert.pEntryPoint) + 1) * sizeof(char); break;
					case SG_SHADER_STAGE_TESC: totalSize += (strlen(pDesc->hull.pEntryPoint) + 1) * sizeof(char); break;
					case SG_SHADER_STAGE_TESE: totalSize += (strlen(pDesc->domain.pEntryPoint) + 1) * sizeof(char); break;
					case SG_SHADER_STAGE_GEOM: totalSize += (strlen(pDesc->geom.pEntryPoint) + 1) * sizeof(char); break;
					case SG_SHADER_STAGE_FRAG: totalSize += (strlen(pDesc->frag.pEntryPoint) + 1) * sizeof(char); break;
					case SG_SHADER_STAGE_COMP: totalSize += (strlen(pDesc->comp.pEntryPoint) + 1) * sizeof(char); break;
					case SG_SHADER_STAGE_RAYTRACING: totalSize += (strlen(pDesc->comp.pEntryPoint) + 1) * sizeof(char); break;
					default: break;
				}
				++counter;
			}
		}

		totalSize += counter * sizeof(VkShaderModule);
		totalSize += counter * sizeof(char*);
		Shader* pShaderProgram = (Shader*)sg_calloc(1, totalSize);

		pShaderProgram->stages = pDesc->stages;
		pShaderProgram->pReflection = (PipelineReflection*)(pShaderProgram + 1);
		pShaderProgram->pShaderModules = (VkShaderModule*)(pShaderProgram->pReflection + 1);
		pShaderProgram->pEntryNames = (char**)(pShaderProgram->pShaderModules + counter);

		uint8_t* mem = (uint8_t*)(pShaderProgram->pEntryNames + counter);
		counter = 0;
		ShaderReflection stageReflections[SG_SHADER_STAGE_COUNT] = {};

		for (uint32_t i = 0; i < SG_SHADER_STAGE_COUNT; ++i)
		{
			ShaderStage stageMask = (ShaderStage)(1 << i);
			if (stageMask == (pShaderProgram->stages & stageMask))
			{
				SG_DECLARE_ZERO(VkShaderModuleCreateInfo, createInfo);
				createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
				createInfo.pNext = nullptr;
				createInfo.flags = 0;

				const BinaryShaderStageDesc* pStageDesc = nullptr;
				switch (stageMask)
				{
				case SG_SHADER_STAGE_VERT:
				{
					vk_create_shader_reflection((const uint8_t*)pDesc->vert.pByteCode, (uint32_t)pDesc->vert.byteCodeSize, stageMask,
						&stageReflections[counter]);

					createInfo.codeSize = pDesc->vert.byteCodeSize;
					createInfo.pCode = (const uint32_t*)pDesc->vert.pByteCode;
					pStageDesc = &pDesc->vert;
					SG_CHECK_VKRESULT(vkCreateShaderModule(pRenderer->pVkDevice, &createInfo, nullptr, &(pShaderProgram->pShaderModules[counter])));
				}
				break;
				case SG_SHADER_STAGE_TESC:
				{
					vk_create_shader_reflection((const uint8_t*)pDesc->hull.pByteCode, (uint32_t)pDesc->hull.byteCodeSize, stageMask,
						&stageReflections[counter]);

					createInfo.codeSize = pDesc->hull.byteCodeSize;
					createInfo.pCode = (const uint32_t*)pDesc->hull.pByteCode;
					pStageDesc = &pDesc->hull;
					SG_CHECK_VKRESULT(vkCreateShaderModule(pRenderer->pVkDevice, &createInfo, nullptr, &(pShaderProgram->pShaderModules[counter])));
				}
				break;
				case SG_SHADER_STAGE_TESE:
				{
					vk_create_shader_reflection((const uint8_t*)pDesc->domain.pByteCode, (uint32_t)pDesc->domain.byteCodeSize, stageMask,
						&stageReflections[counter]);

					createInfo.codeSize = pDesc->domain.byteCodeSize;
					createInfo.pCode = (const uint32_t*)pDesc->domain.pByteCode;
					pStageDesc = &pDesc->domain;
					SG_CHECK_VKRESULT(vkCreateShaderModule(pRenderer->pVkDevice, &createInfo, nullptr, &(pShaderProgram->pShaderModules[counter])));
				}
				break;
				case SG_SHADER_STAGE_GEOM:
				{
					vk_create_shader_reflection((const uint8_t*)pDesc->geom.pByteCode, (uint32_t)pDesc->geom.byteCodeSize, stageMask,
						&stageReflections[counter]);

					createInfo.codeSize = pDesc->geom.byteCodeSize;
					createInfo.pCode = (const uint32_t*)pDesc->geom.pByteCode;
					pStageDesc = &pDesc->geom;
					SG_CHECK_VKRESULT(vkCreateShaderModule(pRenderer->pVkDevice, &createInfo, nullptr, &(pShaderProgram->pShaderModules[counter])));
				}
				break;
				case SG_SHADER_STAGE_FRAG:
				{
					vk_create_shader_reflection((const uint8_t*)pDesc->frag.pByteCode, (uint32_t)pDesc->frag.byteCodeSize, stageMask,
						&stageReflections[counter]);

					createInfo.codeSize = pDesc->frag.byteCodeSize;
					createInfo.pCode = (const uint32_t*)pDesc->frag.pByteCode;
					pStageDesc = &pDesc->frag;
					SG_CHECK_VKRESULT(vkCreateShaderModule(pRenderer->pVkDevice, &createInfo, nullptr, &(pShaderProgram->pShaderModules[counter])));
				}
				break;
				case SG_SHADER_STAGE_COMP:
#ifdef SG_ENABLE_RAYTRACING
				case SG_SHADER_STAGE_RAYTRACING:
#endif
				{
					vk_create_shader_reflection((const uint8_t*)pDesc->comp.pByteCode, (uint32_t)pDesc->comp.byteCodeSize, stageMask,
						&stageReflections[counter]);

					createInfo.codeSize = pDesc->comp.byteCodeSize;
					createInfo.pCode = (const uint32_t*)pDesc->comp.pByteCode;
					pStageDesc = &pDesc->comp;
					SG_CHECK_VKRESULT(vkCreateShaderModule(pRenderer->pVkDevice, &createInfo, nullptr, &(pShaderProgram->pShaderModules[counter])));
				}
				break;
				default: ASSERT(false && "Shader Stage not supported!"); break;
				}

				pShaderProgram->pEntryNames[counter] = (char*)mem;
				mem += (strlen(pStageDesc->pEntryPoint) + 1) * sizeof(char);
				strcpy(pShaderProgram->pEntryNames[counter], pStageDesc->pEntryPoint);
				++counter;
			}
		}

		create_pipeline_reflection(stageReflections, counter, pShaderProgram->pReflection);

		*ppShaderProgram = pShaderProgram;
	}

	void remove_shader(Renderer* pRenderer, Shader* pShaderProgram)
	{
		ASSERT(pRenderer);

		ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

		if (pShaderProgram->stages & SG_SHADER_STAGE_VERT)
		{
			vkDestroyShaderModule(pRenderer->pVkDevice, pShaderProgram->pShaderModules[pShaderProgram->pReflection->vertexStageIndex], nullptr);
		}

		if (pShaderProgram->stages & SG_SHADER_STAGE_TESC)
		{
			vkDestroyShaderModule(pRenderer->pVkDevice, pShaderProgram->pShaderModules[pShaderProgram->pReflection->hullStageIndex], nullptr);
		}

		if (pShaderProgram->stages & SG_SHADER_STAGE_TESE)
		{
			vkDestroyShaderModule(pRenderer->pVkDevice, pShaderProgram->pShaderModules[pShaderProgram->pReflection->domainStageIndex], nullptr);
		}

		if (pShaderProgram->stages & SG_SHADER_STAGE_GEOM)
		{
			vkDestroyShaderModule(pRenderer->pVkDevice, pShaderProgram->pShaderModules[pShaderProgram->pReflection->geometryStageIndex], nullptr);
		}

		if (pShaderProgram->stages & SG_SHADER_STAGE_FRAG)
		{
			vkDestroyShaderModule(pRenderer->pVkDevice, pShaderProgram->pShaderModules[pShaderProgram->pReflection->pixelStageIndex], nullptr);
		}

		if (pShaderProgram->stages & SG_SHADER_STAGE_COMP)
		{
			vkDestroyShaderModule(pRenderer->pVkDevice, pShaderProgram->pShaderModules[0], nullptr);
		}
#ifdef SG_ENABLE_RAYTRACING
		if (pShaderProgram->stages & SG_SHADER_STAGE_RAYTRACING)
		{
			vkDestroyShaderModule(pRenderer->pVkDevice, pShaderProgram->pShaderModules[0], nullptr);
		}
#endif

		destroy_pipeline_reflection(pShaderProgram->pReflection);
		SG_SAFE_FREE(pShaderProgram);
	}

#pragma endregion (Shader)

#pragma region (Sampler)

void add_sampler(Renderer* pRenderer, const SamplerCreateDesc* pDesc, Sampler** ppSampler)
{
	ASSERT(pRenderer);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	ASSERT(pDesc->compareFunc < SG_MAX_COMPARE_MODES);
	ASSERT(ppSampler);

	auto* pSampler = (Sampler*)sg_calloc_memalign(1, alignof(Sampler), sizeof(Sampler));
	ASSERT(pSampler);

	SG_DECLARE_ZERO(VkSamplerCreateInfo, createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.magFilter = util_to_vk_filter(pDesc->magFilter);
	createInfo.minFilter = util_to_vk_filter(pDesc->minFilter);
	createInfo.mipmapMode = util_to_vk_mip_map_mode(pDesc->mipMapMode);
	createInfo.addressModeU = util_to_vk_address_mode(pDesc->addressU);
	createInfo.addressModeV = util_to_vk_address_mode(pDesc->addressV);
	createInfo.addressModeW = util_to_vk_address_mode(pDesc->addressW);

	createInfo.mipLodBias = pDesc->mipLodBias;
	createInfo.anisotropyEnable = (pDesc->maxAnisotropy > 0.0f) ? VK_TRUE : VK_FALSE;
	createInfo.maxAnisotropy = pDesc->maxAnisotropy;
	createInfo.compareEnable = (gVkComparisonFuncTranslator[pDesc->compareFunc] != VK_COMPARE_OP_NEVER) ? VK_TRUE : VK_FALSE;
	createInfo.compareOp = gVkComparisonFuncTranslator[pDesc->compareFunc];

	createInfo.minLod = 0.0f;
	createInfo.maxLod = ((pDesc->mipMapMode == SG_MIPMAP_MODE_LINEAR) ? FLT_MAX : 0.0f);
	createInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	createInfo.unnormalizedCoordinates = VK_FALSE;

	if (TinyImageFormat_IsPlanar(pDesc->mSamplerConversionDesc.format))
	{
		auto& conversionDesc = pDesc->mSamplerConversionDesc;
		VkFormat format = (VkFormat)TinyImageFormat_ToVkFormat(conversionDesc.format);

		// Check format props
		{
			ASSERT(gYCbCrExtension);

			SG_DECLARE_ZERO(VkFormatProperties, formatProps);
			vkGetPhysicalDeviceFormatProperties(pRenderer->pVkActiveGPU, format, &formatProps);
			if (conversionDesc.chromaOffsetX == SG_SAMPLE_LOCATION_MIDPOINT)
			{
				ASSERT(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT);
			}
			else if (conversionDesc.chromaOffsetX == SG_SAMPLE_LOCATION_COSITED)
			{
				ASSERT(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT);
			}
		}

		SG_DECLARE_ZERO(VkSamplerYcbcrConversionCreateInfo, conversionInfo);
		conversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
		conversionInfo.pNext = nullptr;

		conversionInfo.format = format;
		conversionInfo.ycbcrModel = (VkSamplerYcbcrModelConversion)conversionDesc.model;
		conversionInfo.ycbcrRange = (VkSamplerYcbcrRange)conversionDesc.range;
		conversionInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		conversionInfo.xChromaOffset = (VkChromaLocation)conversionDesc.chromaOffsetX;
		conversionInfo.yChromaOffset = (VkChromaLocation)conversionDesc.chromaOffsetY;

		conversionInfo.chromaFilter = util_to_vk_filter(conversionDesc.chromaFilter);
		conversionInfo.forceExplicitReconstruction = conversionDesc.forceExplicitReconstruction ? VK_TRUE : VK_FALSE;
		SG_CHECK_VKRESULT(vkCreateSamplerYcbcrConversion(pRenderer->pVkDevice, &conversionInfo, nullptr, &pSampler->pVkSamplerYcbcrConversion));

		pSampler->mVkSamplerYcbcrConversionInfo = {};
		pSampler->mVkSamplerYcbcrConversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO;
		pSampler->mVkSamplerYcbcrConversionInfo.pNext = nullptr;
		pSampler->mVkSamplerYcbcrConversionInfo.conversion = pSampler->pVkSamplerYcbcrConversion;

		createInfo.pNext = &pSampler->mVkSamplerYcbcrConversionInfo; // set extra data
	}

	SG_CHECK_VKRESULT(vkCreateSampler(pRenderer->pVkDevice, &createInfo, nullptr, &(pSampler->pVkSampler)));

	*ppSampler = pSampler;
}

void remove_sampler(Renderer* pRenderer, Sampler* pSampler)
{
	ASSERT(pRenderer);
	ASSERT(pSampler);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	ASSERT(VK_NULL_HANDLE != pSampler->pVkSampler);

	vkDestroySampler(pRenderer->pVkDevice, pSampler->pVkSampler, nullptr);

	if (nullptr != pSampler->pVkSamplerYcbcrConversion)
	{
		vkDestroySamplerYcbcrConversion(pRenderer->pVkDevice, pSampler->pVkSamplerYcbcrConversion, nullptr);
	}

	SG_SAFE_FREE(pSampler);
}

#pragma endregion (Sampler)

#pragma region (Command Pool)

void add_command_pool(Renderer* pRenderer, const CmdPoolCreateDesc* pDesc, CmdPool** ppCmdPool)
{
	ASSERT(pRenderer);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	ASSERT(ppCmdPool);

	auto* pCmdPool = (CmdPool*)sg_calloc(1, sizeof(CmdPool));
	ASSERT(pCmdPool);

	pCmdPool->pQueue = pDesc->pQueue;

	SG_DECLARE_ZERO(VkCommandPoolCreateInfo, createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.queueFamilyIndex = pDesc->pQueue->vkQueueFamilyIndex;
	if (pDesc->transient)
	{
		createInfo.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	}

	SG_CHECK_VKRESULT(vkCreateCommandPool(pRenderer->pVkDevice, &createInfo, nullptr, &(pCmdPool->pVkCmdPool)));

	*ppCmdPool = pCmdPool;
}

void remove_command_pool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	ASSERT(pRenderer);
	ASSERT(pCmdPool);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	ASSERT(VK_NULL_HANDLE != pCmdPool->pVkCmdPool);

	vkDestroyCommandPool(pRenderer->pVkDevice, pCmdPool->pVkCmdPool, nullptr);

	SG_SAFE_FREE(pCmdPool);
}

#pragma endregion (Command Pool)

#pragma region (Cmd Allocation)

void add_cmd(Renderer* pRenderer, const CmdCreateDesc* pDesc, Cmd** ppCmd)
{
	ASSERT(pRenderer);
	ASSERT(VK_NULL_HANDLE != pDesc->pPool);
	ASSERT(ppCmd);

	auto* pCmd = (Cmd*)sg_calloc_memalign(1, alignof(Cmd), sizeof(Cmd));
	ASSERT(pCmd);

	pCmd->pRenderer = pRenderer;
	pCmd->pQueue = pDesc->pPool->pQueue;
	pCmd->pCmdPool = pDesc->pPool;
	pCmd->type = pDesc->pPool->pQueue->type;
	pCmd->nodeIndex = pDesc->pPool->pQueue->nodeIndex;

	SG_DECLARE_ZERO(VkCommandBufferAllocateInfo, allocInfo);
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.pNext = NULL;
	allocInfo.commandPool = pDesc->pPool->pVkCmdPool;
	allocInfo.level = pDesc->secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;
	SG_CHECK_VKRESULT(vkAllocateCommandBuffers(pRenderer->pVkDevice, &allocInfo, &(pCmd->pVkCmdBuf)));

	*ppCmd = pCmd;
}

void remove_cmd(Renderer* pRenderer, Cmd* pCmd)
{
	ASSERT(pRenderer);
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pRenderer->pVkDevice);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	vkFreeCommandBuffers(pRenderer->pVkDevice, pCmd->pCmdPool->pVkCmdPool, 1, &(pCmd->pVkCmdBuf));

	SG_SAFE_FREE(pCmd);
}

void add_cmd_n(Renderer* pRenderer, const CmdCreateDesc* pDesc, uint32_t cmdCount, Cmd*** pCmds)
{
	// verify that ***cmd is valid
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(cmdCount);
	ASSERT(pCmds);

	Cmd** ppCmds = (Cmd**)sg_calloc(cmdCount, sizeof(Cmd*));
	ASSERT(ppCmds);

	//add n new cmds to given pool
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		add_cmd(pRenderer, pDesc, &ppCmds[i]);
	}

	*pCmds = ppCmds;
}

void remove_cmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds)
{
	//verify that given command list is valid
	ASSERT(ppCmds);

	//remove every given cmd in array
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		remove_cmd(pRenderer, ppCmds[i]);
	}

	SG_SAFE_FREE(ppCmds);
}

#pragma endregion (Cmd Allocation)

#pragma region (Cmds)

// end current render pass ,create new render pass and begin
void cmd_bind_render_targets(Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil,
	const LoadActionsDesc* pLoadActions, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice, uint32_t depthMipSlice)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	if (pCmd->pVkActiveRenderPass)
	{
		vkCmdEndRenderPass(pCmd->pVkCmdBuf);
		pCmd->pVkActiveRenderPass = VK_NULL_HANDLE;
	}

	if (!renderTargetCount && !pDepthStencil)
		return;

	size_t renderPassHash = 0;
	size_t frameBufferHash = 0;

	RenderTargetBarrier barriers[SG_MAX_RENDER_TARGET_ATTACHMENTS + 1] = {};
	uint32_t barrierCount = 0;
	// generate hash for render pass and frame buffer
	// NOTE:
	// Render pass does not care about underlying VkImageView. It only cares about the format and sample count of the attachments.
	// We hash those two values to generate render pass hash
	// Frame buffer is the actual array of all the VkImageViews
	// We hash the texture id associated with the render target to generate frame buffer hash
	for (uint32_t i = 0; i < renderTargetCount; ++i)
	{
		uint32_t hashValues[] = {
			(uint32_t)ppRenderTargets[i]->format,
			(uint32_t)ppRenderTargets[i]->sampleCount,
			pLoadActions ? (uint32_t)pLoadActions->loadActionsColor[i] : 0,
		};
		// get the hash code
		renderPassHash = sg_mem_hash<uint32_t>(hashValues, 3, renderPassHash, 0);
		frameBufferHash = sg_mem_hash<uint32_t>(&ppRenderTargets[i]->id, 1, frameBufferHash, 0);

		if (0 == ppRenderTargets[i]->used++)
		{
			barriers[barrierCount++] = { ppRenderTargets[i], SG_RESOURCE_STATE_UNDEFINED, SG_RESOURCE_STATE_RENDER_TARGET };
		}
	}

	if (pDepthStencil)
	{
		uint32_t hashValues[] = {
			(uint32_t)pDepthStencil->format,
			(uint32_t)pDepthStencil->sampleCount,
			pLoadActions ? (uint32_t)pLoadActions->loadActionDepth : 0,
			pLoadActions ? (uint32_t)pLoadActions->loadActionStencil : 0,
		};
		renderPassHash = sg_mem_hash<uint32_t>(hashValues, 4, renderPassHash, 0);
		frameBufferHash = sg_mem_hash<uint32_t>(&pDepthStencil->id, 1, frameBufferHash, 0);

		if (0 == pDepthStencil->used++)
		{
			barriers[barrierCount++] = { pDepthStencil, SG_RESOURCE_STATE_UNDEFINED, SG_RESOURCE_STATE_DEPTH_WRITE };
		}
	}

	if (pColorArraySlices)
		frameBufferHash = sg_mem_hash<uint32_t>(pColorArraySlices, renderTargetCount, frameBufferHash, 0);
	if (pColorMipSlices)
		frameBufferHash = sg_mem_hash<uint32_t>(pColorMipSlices, renderTargetCount, frameBufferHash, 0);
	if (depthArraySlice != -1)
		frameBufferHash = sg_mem_hash<uint32_t>(&depthArraySlice, 1, frameBufferHash, 0);
	if (depthMipSlice != -1)
		frameBufferHash = sg_mem_hash<uint32_t>(&depthMipSlice, 1, frameBufferHash, 0);

	SampleCount sampleCount = SG_SAMPLE_COUNT_1;

	// get current thread's data
	RenderPassMap& renderPassMap = get_render_pass_map();
	FrameBufferMap& frameBufferMap = get_frame_buffer_map();

	const RenderPassMapIter pNode = renderPassMap.find(renderPassHash);
	const FrameBufferMapIt pFrameBufferNode = frameBufferMap.find(frameBufferHash);

	RenderPass* pRenderPass = nullptr;
	FrameBuffer* pFrameBuffer = nullptr;

	// If a render pass of this combination already exists just use it or create a new one
	if (pNode != renderPassMap.end())
	{
		pRenderPass = pNode->second;
	}
	else
	{
		TinyImageFormat colorFormats[SG_MAX_RENDER_TARGET_ATTACHMENTS] = {};
		TinyImageFormat depthStencilFormat = TinyImageFormat_UNDEFINED;
		for (uint32_t i = 0; i < renderTargetCount; ++i)
		{
			colorFormats[i] = ppRenderTargets[i]->format;
		}
		if (pDepthStencil)
		{
			depthStencilFormat = pDepthStencil->format;
			sampleCount = pDepthStencil->sampleCount;
		}
		else if (renderTargetCount)
		{
			sampleCount = ppRenderTargets[0]->sampleCount;
		}

		SG_DECLARE_ZERO(RenderPassCreateDesc, renderPassDesc);
		renderPassDesc.renderTargetCount = renderTargetCount;
		renderPassDesc.sampleCount = sampleCount;
		renderPassDesc.pColorFormats = colorFormats;
		renderPassDesc.depthStencilFormat = depthStencilFormat;

		renderPassDesc.pLoadActionsColor = pLoadActions ? pLoadActions->loadActionsColor : nullptr;
		renderPassDesc.loadActionDepth = pLoadActions ? pLoadActions->loadActionDepth : SG_LOAD_ACTION_DONTCARE;
		renderPassDesc.loadActionStencil = pLoadActions ? pLoadActions->loadActionStencil : SG_LOAD_ACTION_DONTCARE;
		add_render_pass(pCmd->pRenderer, &renderPassDesc, &pRenderPass);

		// no need of a lock here since this map is per thread
		renderPassMap.insert({ { renderPassHash, pRenderPass } });
	}

	// if a frame buffer of this combination already exists just use it or create a new one
	if (pFrameBufferNode != frameBufferMap.end())
	{
		pFrameBuffer = pFrameBufferNode->second;
	}
	else
	{
		SG_DECLARE_ZERO(FrameBufferCreateDesc, desc);
		desc.renderTargetCount = renderTargetCount;
		desc.pDepthStencil = pDepthStencil;
		desc.ppRenderTargets = ppRenderTargets;
		desc.pRenderPass = pRenderPass;
		desc.pColorArraySlices = pColorArraySlices;
		desc.pColorMipSlices = pColorMipSlices;
		desc.depthArraySlice = depthArraySlice;
		desc.depthMipSlice = depthMipSlice;
		add_framebuffer(pCmd->pRenderer, &desc, &pFrameBuffer);

		// No need of a lock here since this map is per thread
		frameBufferMap.insert({ { frameBufferHash, pFrameBuffer } });
	}

	SG_DECLARE_ZERO(VkRect2D, renderArea);
	renderArea.offset.x = 0;
	renderArea.offset.y = 0;
	renderArea.extent.width = pFrameBuffer->width;
	renderArea.extent.height = pFrameBuffer->height;

	uint32_t clearValueCount = renderTargetCount;
	VkClearValue clearValues[SG_MAX_RENDER_TARGET_ATTACHMENTS + 1] = {};
	if (pLoadActions)
	{
		for (uint32_t i = 0; i < renderTargetCount; ++i)
		{
			ClearValue clearValue = pLoadActions->clearColorValues[i];
			clearValues[i].color = { { clearValue.r, clearValue.g, clearValue.b, clearValue.a } };
		}
		if (pDepthStencil)
		{
			clearValues[renderTargetCount].depthStencil = { pLoadActions->clearDepth.depth, pLoadActions->clearDepth.stencil };
			++clearValueCount;
		}
	}

	if (barrierCount)
		// buffer, texture and render target
		cmd_resource_barrier(pCmd, 0, nullptr, 0, nullptr, barrierCount, barriers);

	SG_DECLARE_ZERO(VkRenderPassBeginInfo, beginInfo);
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.renderPass = pRenderPass->pRenderPass;
	beginInfo.framebuffer = pFrameBuffer->pFramebuffer;
	beginInfo.renderArea = renderArea;
	beginInfo.clearValueCount = clearValueCount;
	beginInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(pCmd->pVkCmdBuf, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
	pCmd->pVkActiveRenderPass = pRenderPass->pRenderPass; // reserve the render pass we used now
}

void cmd_set_viewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	SG_DECLARE_ZERO(VkViewport, viewport);
	viewport.x = x;
	viewport.y = y + height;
	viewport.width = width;
	viewport.height = -height;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;
	vkCmdSetViewport(pCmd->pVkCmdBuf, 0, 1, &viewport);
}

void cmd_set_scissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	SG_DECLARE_ZERO(VkRect2D, rect);
	rect.offset.x = x;
	rect.offset.y = y;
	rect.extent.width = width;
	rect.extent.height = height;
	vkCmdSetScissor(pCmd->pVkCmdBuf, 0, 1, &rect);
}

void cmd_set_stencil_reference_value(Cmd* pCmd, uint32_t val)
{
	ASSERT(pCmd);
	vkCmdSetStencilReference(pCmd->pVkCmdBuf, VK_STENCIL_FRONT_AND_BACK, val);
}

void cmd_bind_pipeline(Cmd* pCmd, Pipeline* pPipeline)
{
	ASSERT(pCmd);
	ASSERT(pPipeline);
	ASSERT(pCmd->pVkCmdBuf != VK_NULL_HANDLE);

	VkPipelineBindPoint pipelineBindPoint = gPipelineBindPoint[pPipeline->type];
	vkCmdBindPipeline(pCmd->pVkCmdBuf, pipelineBindPoint, pPipeline->pVkPipeline);
}

void cmd_bind_index_buffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
{
	ASSERT(pCmd);
	ASSERT(pBuffer);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	VkIndexType vkIndexType = (SG_INDEX_TYPE_UINT16 == indexType) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
	vkCmdBindIndexBuffer(pCmd->pVkCmdBuf, pBuffer->pVkBuffer, offset, vkIndexType);
}

void cmd_bind_vertex_buffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets)
{
	UNREF_PARAM(pStrides);

	ASSERT(pCmd);
	ASSERT(0 != bufferCount);
	ASSERT(ppBuffers);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);
	ASSERT(pStrides);

	const uint32_t maxBuffers = pCmd->pRenderer->pVkActiveGPUProperties->properties.limits.maxVertexInputBindings;
	uint32_t cappedBufferCount = bufferCount > maxBuffers ? maxBuffers : bufferCount;

	// no upper bound for this, so use 64 for now
	ASSERT(cappedBufferCount < 64);

	SG_DECLARE_ZERO(VkBuffer, buffers[64]);
	SG_DECLARE_ZERO(VkDeviceSize, offsets[64]);

	for (uint32_t i = 0; i < cappedBufferCount; ++i)
	{
		buffers[i] = ppBuffers[i]->pVkBuffer;
		offsets[i] = (pOffsets ? pOffsets[i] : 0);
	}

	vkCmdBindVertexBuffers(pCmd->pVkCmdBuf, 0, cappedBufferCount, buffers, offsets);
}

void cmd_draw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	vkCmdDraw(pCmd->pVkCmdBuf, vertexCount, 1, firstVertex, 0);
}

void cmd_draw_instanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	vkCmdDraw(pCmd->pVkCmdBuf, vertexCount, instanceCount, firstVertex, firstInstance);
}

void cmd_draw_indexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	vkCmdDrawIndexed(pCmd->pVkCmdBuf, indexCount, 1, firstIndex, firstVertex, 0);
}

void cmd_draw_indexed_instanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstInstance, uint32_t firstVertex)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	vkCmdDrawIndexed(pCmd->pVkCmdBuf, indexCount, instanceCount, firstIndex, firstVertex, firstInstance);
}

void cmd_dispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	ASSERT(pCmd);
	ASSERT(pCmd->pVkCmdBuf != VK_NULL_HANDLE);

	vkCmdDispatch(pCmd->pVkCmdBuf, groupCountX, groupCountY, groupCountZ);
}

// copy new buffer to override old buffer
void cmd_update_buffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
{
	ASSERT(pCmd);
	ASSERT(pSrcBuffer);
	ASSERT(pSrcBuffer->pVkBuffer);
	ASSERT(pBuffer);
	ASSERT(pBuffer->pVkBuffer);
	ASSERT(srcOffset + size <= pSrcBuffer->size);
	ASSERT(dstOffset + size <= pBuffer->size);

	SG_DECLARE_ZERO(VkBufferCopy, region);
	region.srcOffset = srcOffset;
	region.dstOffset = dstOffset;
	region.size = (VkDeviceSize)size;
	vkCmdCopyBuffer(pCmd->pVkCmdBuf, pSrcBuffer->pVkBuffer, pBuffer->pVkBuffer, 1, &region);
}

typedef struct SubresourceDataDesc
{
	uint64_t srcOffset;
	uint32_t mipLevel;
	uint32_t arrayLayer;
	uint32_t rowPitch;
	uint32_t slicePitch;
} SubresourceDataDesc;

// copy read-in buffer to image
void cmd_update_subresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const SubresourceDataDesc* pSubresourceDesc)
{
	const TinyImageFormat fmt = (TinyImageFormat)pTexture->format;
	const bool isSinglePlane = TinyImageFormat_IsSinglePlane(fmt);

	if (isSinglePlane)
	{
		const uint32_t width = eastl::max<uint32_t>(1, pTexture->width >> pSubresourceDesc->mipLevel);
		const uint32_t height = eastl::max<uint32_t>(1, pTexture->height >> pSubresourceDesc->mipLevel);
		const uint32_t depth = eastl::max<uint32_t>(1, pTexture->depth >> pSubresourceDesc->mipLevel);

		const uint32_t numBlocksWide = pSubresourceDesc->rowPitch / (TinyImageFormat_BitSizeOfBlock(fmt) >> 3);
		const uint32_t numBlocksHigh = (pSubresourceDesc->slicePitch / pSubresourceDesc->rowPitch);

		VkBufferImageCopy copy = {};
		copy.bufferOffset = pSubresourceDesc->srcOffset;
		copy.bufferRowLength = numBlocksWide * TinyImageFormat_WidthOfBlock(fmt);
		copy.bufferImageHeight = numBlocksHigh * TinyImageFormat_HeightOfBlock(fmt);
		copy.imageSubresource.aspectMask = (VkImageAspectFlags)pTexture->aspectMask;
		copy.imageSubresource.mipLevel = pSubresourceDesc->mipLevel;
		copy.imageSubresource.baseArrayLayer = pSubresourceDesc->arrayLayer;
		copy.imageSubresource.layerCount = 1;

		copy.imageOffset.x = 0;
		copy.imageOffset.y = 0;
		copy.imageOffset.z = 0;

		copy.imageExtent.width = width;
		copy.imageExtent.height = height;
		copy.imageExtent.depth = depth;

		vkCmdCopyBufferToImage(pCmd->pVkCmdBuf, pSrcBuffer->pVkBuffer, pTexture->pVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	}
	else
	{
		const uint32_t width = pTexture->width;
		const uint32_t height = pTexture->height;
		const uint32_t depth = pTexture->depth;
		const uint32_t numOfPlanes = TinyImageFormat_NumOfPlanes(fmt);

		uint64_t offset = pSubresourceDesc->srcOffset;
		VkBufferImageCopy bufferImagesCopy[MAX_PLANE_COUNT];

		// if it is not single plane image, we copy for each plane
		for (uint32_t i = 0; i < numOfPlanes; ++i)
		{
			VkBufferImageCopy& copy = bufferImagesCopy[i];
			copy.bufferOffset = offset;
			copy.bufferRowLength = 0;
			copy.bufferImageHeight = 0;
			copy.imageSubresource.aspectMask = (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_PLANE_0_BIT << i);
			copy.imageSubresource.mipLevel = pSubresourceDesc->mipLevel;
			copy.imageSubresource.baseArrayLayer = pSubresourceDesc->arrayLayer;
			copy.imageSubresource.layerCount = 1;

			copy.imageOffset.x = 0;
			copy.imageOffset.y = 0;
			copy.imageOffset.z = 0;

			copy.imageExtent.width = TinyImageFormat_PlaneWidth(fmt, i, width);
			copy.imageExtent.height = TinyImageFormat_PlaneHeight(fmt, i, height);
			copy.imageExtent.depth = depth;
			offset += copy.imageExtent.width * copy.imageExtent.height * TinyImageFormat_PlaneSizeOfBlock(fmt, i);
		}

		vkCmdCopyBufferToImage(pCmd->pVkCmdBuf, pSrcBuffer->pVkBuffer, pTexture->pVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, numOfPlanes, bufferImagesCopy);
	}
}

void cmd_bind_descriptor_set(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet)
{
	ASSERT(pCmd);
	ASSERT(pDescriptorSet);
	ASSERT(pDescriptorSet->pHandles);
	ASSERT(index < pDescriptorSet->maxSets);
	// which root signature it belongs to
	const RootSignature* pRootSignature = pDescriptorSet->pRootSignature;

	if (pCmd->pBoundPipelineLayout != pRootSignature->pPipelineLayout)
	{
		pCmd->pBoundPipelineLayout = pRootSignature->pPipelineLayout;

		// vulkan requires to bind all descriptor sets up to the highest set number even if they are empty
		// example: If shader uses only set 2, we still have to bind empty sets for set=0 and set=1
		for (uint32_t setIndex = 0; setIndex < SG_DESCRIPTOR_UPDATE_FREQ_COUNT; ++setIndex)
		{
			if (pRootSignature->vkEmptyDescriptorSets[setIndex] != VK_NULL_HANDLE)
			{
				vkCmdBindDescriptorSets(pCmd->pVkCmdBuf, gPipelineBindPoint[pRootSignature->pipelineType], pRootSignature->pPipelineLayout, setIndex, 1,
					&pRootSignature->vkEmptyDescriptorSets[setIndex], 0, nullptr); // no dynamic offset
			}
		}
	}

	// bind the final descriptor set
	vkCmdBindDescriptorSets(pCmd->pVkCmdBuf, gPipelineBindPoint[pRootSignature->pipelineType], pRootSignature->pPipelineLayout, pDescriptorSet->updateFrequency, 1, &pDescriptorSet->pHandles[index],
		pDescriptorSet->dynamicOffsetCount, pDescriptorSet->dynamicOffsetCount ? &pDescriptorSet->pDynamicSizeOffsets[index].offset : nullptr);
}

void cmd_bind_push_constants(Cmd* pCmd, RootSignature* pRootSignature, const char* name, const void* pConstants)
{
	ASSERT(pCmd);
	ASSERT(pConstants);
	ASSERT(pRootSignature);
	ASSERT(name);

	const DescriptorInfo* pDesc = get_descriptor(pRootSignature, name);
	ASSERT(pDesc);
	ASSERT(SG_DESCRIPTOR_TYPE_ROOT_CONSTANT == pDesc->type);

	vkCmdPushConstants(pCmd->pVkCmdBuf, pRootSignature->pPipelineLayout,
		pDesc->vkStages, 0, pDesc->size, pConstants);
}

void cmd_bind_push_constants_by_index(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants)
{
	ASSERT(pCmd);
	ASSERT(pConstants);
	ASSERT(pRootSignature);
	ASSERT(paramIndex >= 0 && paramIndex < pRootSignature->descriptorCount);

	const DescriptorInfo* pDesc = pRootSignature->descriptors + paramIndex;
	ASSERT(pDesc);
	ASSERT(SG_DESCRIPTOR_TYPE_ROOT_CONSTANT == pDesc->type);

	vkCmdPushConstants(pCmd->pVkCmdBuf, pRootSignature->pPipelineLayout,
		pDesc->vkStages, 0, pDesc->size, pConstants);
}

#pragma endregion (Cmds)

#pragma region (Render Synchronization)

void acquire_next_image(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pImageIndex)
{
	ASSERT(pRenderer);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	ASSERT(VK_NULL_HANDLE != pSwapChain->pSwapChain);
	ASSERT(pSignalSemaphore || pFence);

	SG_DECLARE_ZERO(VkResult, vkRes);
	if (pFence != nullptr) // we use semaphore or fence to synchronize
	{
		vkRes = vkAcquireNextImageKHR(pRenderer->pVkDevice, pSwapChain->pSwapChain, UINT64_MAX, VK_NULL_HANDLE, pFence->pVkFence, pImageIndex);

		// If swapchain is out of date, let caller know by setting image index to -1
		if (vkRes == VK_ERROR_OUT_OF_DATE_KHR)
		{
			*pImageIndex = -1;
			vkResetFences(pRenderer->pVkDevice, 1, &pFence->pVkFence);
			pFence->submitted = false;
			return;
		}
		pFence->submitted = true; // acquire successfully
	}
	else
	{
		vkRes = vkAcquireNextImageKHR(pRenderer->pVkDevice, pSwapChain->pSwapChain, UINT64_MAX, pSignalSemaphore->pVkSemaphore, VK_NULL_HANDLE, pImageIndex);

		// If swapchain is out of date, let caller know by setting image index to -1
		if (vkRes == VK_ERROR_OUT_OF_DATE_KHR)
		{
			*pImageIndex = -1;
			pSignalSemaphore->signaled = false;
			return;
		}
		ASSERT(VK_SUCCESS == vkRes);
		pSignalSemaphore->signaled = true;
	}
}

void queue_submit(Queue* pQueue, const QueueSubmitDesc* pDesc)
{
	ASSERT(pQueue);
	ASSERT(pDesc);

	uint32_t cmdCount = pDesc->cmdCount;
	Cmd** ppCmds = pDesc->ppCmds;
	Fence* pFence = pDesc->pSignalFence;

	uint32_t waitSemaphoreCount = pDesc->waitSemaphoreCount;
	Semaphore** ppWaitSemaphores = pDesc->ppWaitSemaphores;

	uint32_t signalSemaphoreCount = pDesc->signalSemaphoreCount;
	Semaphore** ppSignalSemaphores = pDesc->ppSignalSemaphores;

	ASSERT(cmdCount > 0);
	ASSERT(ppCmds);
	if (waitSemaphoreCount > 0)
	{
		ASSERT(ppWaitSemaphores);
	}
	if (signalSemaphoreCount > 0)
	{
		ASSERT(ppSignalSemaphores);
	}

	ASSERT(VK_NULL_HANDLE != pQueue->pVkQueue);
	auto* cmds = (VkCommandBuffer*)alloca(cmdCount * sizeof(VkCommandBuffer));
	for (uint32_t i = 0; i < cmdCount; ++i)
		cmds[i] = ppCmds[i]->pVkCmdBuf;

	auto* waitSemaphores = waitSemaphoreCount ? (VkSemaphore*)alloca(waitSemaphoreCount * sizeof(VkSemaphore)) : nullptr;
	auto* waitMasks = (VkPipelineStageFlags*)alloca(waitSemaphoreCount * sizeof(VkPipelineStageFlags));
	uint32_t waitCount = 0;
	for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
	{
		if (ppWaitSemaphores[i]->signaled)
		{
			waitSemaphores[waitCount] = ppWaitSemaphores[i]->pVkSemaphore;
			waitMasks[waitCount] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			++waitCount;

			ppWaitSemaphores[i]->signaled = false;
		}
	}

	VkSemaphore* signalSemaphores = signalSemaphoreCount ? (VkSemaphore*)alloca(signalSemaphoreCount * sizeof(VkSemaphore)) : nullptr;
	uint32_t signalCount = 0;
	for (uint32_t i = 0; i < signalSemaphoreCount; ++i)
	{
		if (!ppSignalSemaphores[i]->signaled)
		{
			signalSemaphores[signalCount] = ppSignalSemaphores[i]->pVkSemaphore;
			ppSignalSemaphores[i]->currentNodeIndex = pQueue->nodeIndex;
			ppSignalSemaphores[signalCount]->signaled = true;
			++signalCount;
		}
	}

	SG_DECLARE_ZERO(VkSubmitInfo, submitInfo);
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;

	submitInfo.waitSemaphoreCount = waitCount;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitMasks;

	submitInfo.commandBufferCount = cmdCount;
	submitInfo.pCommandBuffers = cmds;

	submitInfo.signalSemaphoreCount = signalCount;
	submitInfo.pSignalSemaphores = signalSemaphores;

	VkDeviceGroupSubmitInfo deviceGroupSubmitInfo = { VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO_KHR, nullptr };
	if (pQueue->gpuMode == SG_GPU_MODE_LINKED)
	{
		uint32_t* pVkDeviceMasks = nullptr;
		uint32_t* pSignalIndices = nullptr;
		uint32_t* pWaitIndices = nullptr;

		deviceGroupSubmitInfo.pNext = nullptr;
		deviceGroupSubmitInfo.commandBufferCount = submitInfo.commandBufferCount;
		deviceGroupSubmitInfo.signalSemaphoreCount = submitInfo.signalSemaphoreCount;
		deviceGroupSubmitInfo.waitSemaphoreCount = submitInfo.waitSemaphoreCount;

		pVkDeviceMasks = (uint32_t*)alloca(deviceGroupSubmitInfo.commandBufferCount * sizeof(uint32_t));
		pSignalIndices = (uint32_t*)alloca(deviceGroupSubmitInfo.signalSemaphoreCount * sizeof(uint32_t));
		pWaitIndices = (uint32_t*)alloca(deviceGroupSubmitInfo.waitSemaphoreCount * sizeof(uint32_t));

		for (uint32_t i = 0; i < deviceGroupSubmitInfo.commandBufferCount; ++i)
		{
			pVkDeviceMasks[i] = (1 << ppCmds[i]->nodeIndex);
		}
		for (uint32_t i = 0; i < deviceGroupSubmitInfo.signalSemaphoreCount; ++i)
		{
			pSignalIndices[i] = pQueue->nodeIndex;
		}
		for (uint32_t i = 0; i < deviceGroupSubmitInfo.waitSemaphoreCount; ++i)
		{
			pWaitIndices[i] = ppWaitSemaphores[i]->currentNodeIndex;
		}

		deviceGroupSubmitInfo.pCommandBufferDeviceMasks = pVkDeviceMasks;
		deviceGroupSubmitInfo.pSignalSemaphoreDeviceIndices = pSignalIndices;
		deviceGroupSubmitInfo.pWaitSemaphoreDeviceIndices = pWaitIndices;
		submitInfo.pNext = &deviceGroupSubmitInfo;
	}

	// Lightweight lock to make sure multiple threads don't use the same queue simultaneously
	// Many setups have just one queue family and one queue. In this case, async compute, async transfer doesn't exist and we end up using
	// the same queue for all three operations
	{
		MutexLock lock(*pQueue->pSubmitMutex);
		VkResult vkRes = vkQueueSubmit(pQueue->pVkQueue, 1, &submitInfo, pFence ? pFence->pVkFence : VK_NULL_HANDLE);
		ASSERT(VK_SUCCESS == vkRes);
	}

	if (pFence)
		pFence->submitted = true;
}

PresentStatus queue_present(Queue* pQueue, const QueuePresentDesc* pDesc)
{
	ASSERT(pQueue);
	ASSERT(pDesc);

	uint32_t waitSemaphoreCount = pDesc->waitSemaphoreCount;
	Semaphore** ppWaitSemaphores = pDesc->ppWaitSemaphores;

	PresentStatus presentStatus = SG_PRESENT_STATUS_FAILED;
	if (pDesc->pSwapChain)
	{
		SwapChain* pSwapChain = pDesc->pSwapChain;
		ASSERT(pQueue);
		if (waitSemaphoreCount > 0)
			ASSERT(ppWaitSemaphores);

		ASSERT(VK_NULL_HANDLE != pQueue->pVkQueue);
		// wait for image is available
		VkSemaphore* waitSemaphores = waitSemaphoreCount ? (VkSemaphore*)alloca(waitSemaphoreCount * sizeof(VkSemaphore)) : nullptr;
		uint32_t waitCount = 0;
		for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
		{
			if (ppWaitSemaphores[i]->signaled) // reset the state
			{
				waitSemaphores[waitCount] = ppWaitSemaphores[i]->pVkSemaphore;
				ppWaitSemaphores[i]->signaled = false;
				++waitCount;
			}
		}

		// which image to present
		uint32_t presentIndex = pDesc->index;

		SG_DECLARE_ZERO(VkPresentInfoKHR, presentInfo);
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.waitSemaphoreCount = waitCount;
		presentInfo.pWaitSemaphores = waitSemaphores;

		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &(pSwapChain->pSwapChain);
		presentInfo.pImageIndices = &(presentIndex);
		presentInfo.pResults = nullptr;

		// Lightweight lock to make sure multiple threads don't use the same queue simultaneously
		{
			MutexLock lock(*pQueue->pSubmitMutex);
			VkResult vkRes = vkQueuePresentKHR(pSwapChain->pPresentQueue ? pSwapChain->pPresentQueue : pQueue->pVkQueue, &presentInfo);

			if (vkRes == VK_SUCCESS)
			{
				presentStatus = SG_PRESENT_STATUS_SUCCESS;
			}
			else if (vkRes == VK_ERROR_DEVICE_LOST)
			{
				presentStatus = SG_PRESENT_STATUS_DEVICE_RESET;
			}
			else if (vkRes == VK_ERROR_OUT_OF_DATE_KHR)
			{
				// TODO : Fix bug where we get this error if window is closed before able to present queue.
				// may be we need to recreate the swap chain and the related resources
			}
			else
			{
				ASSERT(0);
			}
		}
	}
	return presentStatus;
}

void wait_for_fences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences)
{
	ASSERT(pRenderer);
	ASSERT(fenceCount);
	ASSERT(ppFences);

	auto* fences = (VkFence*)alloca(fenceCount * sizeof(VkFence));
	uint32_t numValidFences = 0;
	for (uint32_t i = 0; i < fenceCount; ++i)
	{
		if (ppFences[i]->submitted)
			fences[numValidFences++] = ppFences[i]->pVkFence;
	}

	if (numValidFences)
	{
		VkResult result = vkWaitForFences(pRenderer->pVkDevice, numValidFences, fences, VK_TRUE, UINT64_MAX);

#if defined(USE_NSIGHT_AFTERMATH)
		if (pRenderer->aftermathSupport)
		{
			if (VK_ERROR_DEVICE_LOST == result)
			{
				// Device lost notification is asynchronous to the NVIDIA display
				// driver's GPU crash handling. Give the Nsight Aftermath GPU crash dump
				// thread some time to do its work before terminating the process.
				SG::sleep(3000);
			}
		}
#endif
		// reset the fences to unsignal state
		vkResetFences(pRenderer->pVkDevice, numValidFences, fences);
	}

	for (uint32_t i = 0; i < fenceCount; ++i)
		ppFences[i]->submitted = false; // reset our own fences to unsignal state
}

void wait_queue_idle(Queue* pQueue)
{
	vkQueueWaitIdle(pQueue->pVkQueue);
}

void get_fence_status(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
	*pFenceStatus = SG_FENCE_STATUS_COMPLETE;

	if (pFence->submitted)
	{
		VkResult vkRes = vkGetFenceStatus(pRenderer->pVkDevice, pFence->pVkFence);
		if (vkRes == VK_SUCCESS)
		{
			vkResetFences(pRenderer->pVkDevice, 1, &pFence->pVkFence);
			pFence->submitted = false;
		}
		*pFenceStatus = vkRes == VK_SUCCESS ? SG_FENCE_STATUS_COMPLETE : SG_FENCE_STATUS_INCOMPLETE;
	}
	else
	{
		*pFenceStatus = SG_FENCE_STATUS_NOTSUBMITTED;
	}
}

#pragma endregion (Render Synchronization)

#pragma region (Fence)

void add_fence(Renderer* pRenderer, Fence** ppFence)
{
	ASSERT(pRenderer);
	ASSERT(ppFence);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

	auto* pFence = (Fence*)sg_calloc(1, sizeof(Fence));
	ASSERT(pFence);

	SG_DECLARE_ZERO(VkFenceCreateInfo, createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	SG_CHECK_VKRESULT(vkCreateFence(pRenderer->pVkDevice, &createInfo, nullptr, &pFence->pVkFence));

	pFence->submitted = false;
	*ppFence = pFence;
}

void remove_fence(Renderer* pRenderer, Fence* pFence)
{
	ASSERT(pRenderer);
	ASSERT(pFence);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	ASSERT(VK_NULL_HANDLE != pFence->pVkFence);

	vkDestroyFence(pRenderer->pVkDevice, pFence->pVkFence, nullptr);

	SG_SAFE_FREE(pFence);
}

#pragma endregion (Fence)

#pragma region (Semaphore)

void add_semaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
	ASSERT(pRenderer);
	ASSERT(ppSemaphore);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

	auto* pSemaphore = (Semaphore*)sg_calloc(1, sizeof(Semaphore));
	ASSERT(pSemaphore);

	SG_DECLARE_ZERO(VkSemaphoreCreateInfo, createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	SG_CHECK_VKRESULT(vkCreateSemaphore(pRenderer->pVkDevice, &createInfo, nullptr, &(pSemaphore->pVkSemaphore)));
	// Set signal inital state.
	pSemaphore->signaled = false;

	*ppSemaphore = pSemaphore;
}

void remove_semaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
	ASSERT(pRenderer);
	ASSERT(pSemaphore);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	ASSERT(VK_NULL_HANDLE != pSemaphore->pVkSemaphore);

	vkDestroySemaphore(pRenderer->pVkDevice, pSemaphore->pVkSemaphore, nullptr);

	SG_SAFE_FREE(pSemaphore);
}

#pragma endregion (Semaphore)

#pragma region (Render Target)

static sg_atomic32_t gRenderTargetIds = 1; // thread-free

void add_render_target(Renderer* pRenderer, const RenderTargetCreateDesc* pDesc, RenderTarget** ppRenderTarget)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppRenderTarget);

	bool const isDepth = TinyImageFormat_IsDepthOnly(pDesc->format) || TinyImageFormat_IsDepthAndStencil(pDesc->format);

	ASSERT(!((isDepth) && (pDesc->descriptors & SG_DESCRIPTOR_TYPE_RW_TEXTURE)) && "Cannot use depth stencil as UAV");

	((RenderTargetCreateDesc*)pDesc)->mipLevels = eastl::max(1U, pDesc->mipLevels);

	uint32_t depthOrArraySize = pDesc->arraySize * pDesc->depth;
	uint32_t numRTVs = pDesc->mipLevels;
	if ((pDesc->descriptors & SG_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
		(pDesc->descriptors & SG_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		numRTVs *= depthOrArraySize;

	// memory management ( RenderTarget + VkImageView )
	size_t totalSize = sizeof(RenderTarget);
	totalSize += numRTVs * sizeof(VkImageView);
	auto* pRenderTarget = (RenderTarget*)sg_calloc_memalign(1, alignof(RenderTarget), totalSize);
	ASSERT(pRenderTarget);

	pRenderTarget->pVkSliceDescriptors = (VkImageView*)(pRenderTarget + 1);

	// monotonically(µ¥µ÷µØ) increasing thread safe id generation
	pRenderTarget->id = sg_atomic32_add_relaxed(&gRenderTargetIds, 1); // it returns a initial value

	// create a texture for RTV
	TextureCreateDesc textureDesc = {};
	textureDesc.arraySize = pDesc->arraySize;
	textureDesc.clearValue = pDesc->clearValue;
	textureDesc.depth = pDesc->depth;
	textureDesc.flags = pDesc->flags;
	textureDesc.format = pDesc->format;

	textureDesc.mipLevels = pDesc->mipLevels;
	textureDesc.sampleCount = pDesc->sampleCount;
	textureDesc.sampleQuality = pDesc->sampleQuality;

	textureDesc.width = pDesc->width;
	textureDesc.height = pDesc->height;
	textureDesc.nativeHandle = pDesc->pNativeHandle;
	textureDesc.nodeIndex = pDesc->nodeIndex;
	textureDesc.pSharedNodeIndices = pDesc->pSharedNodeIndices;
	textureDesc.sharedNodeIndexCount = pDesc->sharedNodeIndexCount;

	if (!isDepth)
		textureDesc.startState |= SG_RESOURCE_STATE_RENDER_TARGET;
	else
		textureDesc.startState |= SG_RESOURCE_STATE_DEPTH_WRITE;

	// Set this by default to be able to sample the render target in shader
	textureDesc.descriptors = pDesc->descriptors;
	// Create SRV by default for a render target
	textureDesc.descriptors |= SG_DESCRIPTOR_TYPE_TEXTURE;

	if (isDepth)
	{
		// Make sure depth/stencil format is supported - fall back to VK_FORMAT_D16_UNORM if not
		VkFormat vk_depth_stencil_format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->format);
		if (VK_FORMAT_UNDEFINED != vk_depth_stencil_format)
		{
			SG_DECLARE_ZERO(VkImageFormatProperties, properties);
			VkResult vkRes = vkGetPhysicalDeviceImageFormatProperties(
				pRenderer->pVkActiveGPU, vk_depth_stencil_format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &properties);
			// Fall back to something that's guaranteed to work
			if (VK_SUCCESS != vkRes)
			{
				textureDesc.format = TinyImageFormat_D16_UNORM;
				SG_LOG_WARNING("Depth stencil format (%u) not supported. Falling back to D16 format", pDesc->format);
			}
		}
	}

	textureDesc.name = pDesc->name;

	add_texture(pRenderer, &textureDesc, &pRenderTarget->pTexture);

	VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
	if (pDesc->height > 1)
		viewType = depthOrArraySize > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
	else
		viewType = depthOrArraySize > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;

	VkImageViewCreateInfo rtvDesc = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr };
	rtvDesc.flags = 0;
	rtvDesc.image = pRenderTarget->pTexture->pVkImage;
	rtvDesc.viewType = viewType;
	rtvDesc.format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->format);
	rtvDesc.components.r = VK_COMPONENT_SWIZZLE_R;
	rtvDesc.components.g = VK_COMPONENT_SWIZZLE_G;
	rtvDesc.components.b = VK_COMPONENT_SWIZZLE_B;
	rtvDesc.components.a = VK_COMPONENT_SWIZZLE_A;
	rtvDesc.subresourceRange.aspectMask = util_vk_determine_aspect_mask(rtvDesc.format, true);
	rtvDesc.subresourceRange.baseMipLevel = 0;
	rtvDesc.subresourceRange.levelCount = 1;
	rtvDesc.subresourceRange.baseArrayLayer = 0;
	rtvDesc.subresourceRange.layerCount = depthOrArraySize;

	SG_CHECK_VKRESULT(vkCreateImageView(pRenderer->pVkDevice, &rtvDesc, nullptr, &pRenderTarget->pVkDescriptor));

	for (uint32_t i = 0; i < pDesc->mipLevels; ++i)
	{
		rtvDesc.subresourceRange.baseMipLevel = i;
		if ((pDesc->descriptors & SG_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
			(pDesc->descriptors & SG_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		{
			for (uint32_t j = 0; j < depthOrArraySize; ++j)
			{
				rtvDesc.subresourceRange.layerCount = 1;
				rtvDesc.subresourceRange.baseArrayLayer = j;
				SG_CHECK_VKRESULT(vkCreateImageView(pRenderer->pVkDevice, &rtvDesc, nullptr, &pRenderTarget->pVkSliceDescriptors[i * depthOrArraySize + j]));
			}
		}
		else
		{
			SG_CHECK_VKRESULT(vkCreateImageView(pRenderer->pVkDevice, &rtvDesc, nullptr, &pRenderTarget->pVkSliceDescriptors[i]));
		}
	}

	pRenderTarget->width = pDesc->width;
	pRenderTarget->height = pDesc->height;
	pRenderTarget->depth = pDesc->depth;

	//SG_LOG_DEBUG("Width: %d", pRenderTarget->width);
	//SG_LOG_DEBUG("Height: %d", pRenderTarget->height);

	pRenderTarget->arraySize = pDesc->arraySize;
	pRenderTarget->mipLevels = pDesc->mipLevels;
	pRenderTarget->sampleCount = pDesc->sampleCount;
	pRenderTarget->sampleQuality = pDesc->sampleQuality;
	pRenderTarget->format = pDesc->format;
	pRenderTarget->clearValue = pDesc->clearValue;

	*ppRenderTarget = pRenderTarget;
}

void remove_render_target(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
	remove_texture(pRenderer, pRenderTarget->pTexture);

	vkDestroyImageView(pRenderer->pVkDevice, pRenderTarget->pVkDescriptor, nullptr);

	const uint32_t depthOrArraySize = pRenderTarget->arraySize * pRenderTarget->depth;
	if ((pRenderTarget->descriptors & SG_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
		(pRenderTarget->descriptors & SG_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
	{
		for (uint32_t i = 0; i < pRenderTarget->mipLevels; ++i)
			for (uint32_t j = 0; j < depthOrArraySize; ++j)
				vkDestroyImageView(pRenderer->pVkDevice, pRenderTarget->pVkSliceDescriptors[i * depthOrArraySize + j], nullptr);
	}
	else
	{
		for (uint32_t i = 0; i < pRenderTarget->mipLevels; ++i)
			vkDestroyImageView(pRenderer->pVkDevice, pRenderTarget->pVkSliceDescriptors[i], nullptr);
	}

	SG_SAFE_FREE(pRenderTarget);
}

#pragma endregion (Render Target)

#pragma region (SwapChain)

void add_swapchain(Renderer* pRenderer, const SwapChainCreateDesc* pDesc, SwapChain** ppSwapChain)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppSwapChain);
	ASSERT(pDesc->imageCount <= SG_MAX_SWAPCHAIN_IMAGES);

	// Create surface
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkInstance);
	VkSurfaceKHR vkSurface;
	// Create a WSI surface for the window:
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	SG_DECLARE_ZERO(VkWin32SurfaceCreateInfoKHR, createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.hinstance = ::GetModuleHandle(nullptr); // get current module handle
	createInfo.hwnd = (HWND)pDesc->windowHandle.window;
	SG_CHECK_VKRESULT(vkCreateWin32SurfaceKHR(pRenderer->pVkInstance, &createInfo, nullptr, &vkSurface)); // surface to present
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
	SG_DECLARE_ZERO(VkXlibSurfaceCreateInfoKHR, createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_createInfo_KHR;
	createInfo.pNext = NULL;
	createInfo.flags = 0;
	createInfo.dpy = pDesc->windowHandle.display;      //TODO
	createInfo.window = pDesc->windowHandle.window;    //TODO
	CHECK_VKRESULT(vkCreateXlibSurfaceKHR(pRenderer->pVkInstance, &createInfo, nullptr, &vkSurface));
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	SG_DECLARE_ZERO(VkXcbSurfaceCreateInfoKHR, createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_createInfo_KHR;
	createInfo.pNext = NULL;
	createInfo.flags = 0;
	createInfo.connection = pDesc->windowHandle.connection;    //TODO
	createInfo.window = pDesc->windowHandle.window;        //TODO
	SG_CHECK_VKRESULT(vkCreateXcbSurfaceKHR(pRenderer->pVkInstance, &createInfo, nullptr, &vkSurface));
#elif defined(VK_USE_PLATFORM_IOS_MVK)
	// Add IOS support here
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
	// Add MacOS support here
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
	SG_DECLARE_ZERO(VkAndroidSurfaceCreateInfoKHR, createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.window = pDesc->windowHandle.window;
	SG_CHECK_VKRESULT(vkCreateAndroidSurfaceKHR(pRenderer->pVkInstance, &createInfo, nullptr, &vkSurface));
#elif defined(VK_USE_PLATFORM_GGP)
	extern VkResult ggpCreateSurface(VkInstance, VkSurfaceKHR * surface);
	SG_CHECK_VKRESULT(ggpCreateSurface(pRenderer->pVkInstance, &vkSurface));
#elif defined(VK_USE_PLATFORM_VI_NN)
	extern VkResult nxCreateSurface(VkInstance, VkSurfaceKHR * surface);
	SG_CHECK_VKRESULT(nxCreateSurface(pRenderer->pVkInstance, &vkSurface));
#else
#error PLATFORM NOT SUPPORTED
#endif

	/// Create swap chain
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkActiveGPU);
	// Image count
	if (0 == pDesc->imageCount) // default is set to 2
	{
		((SwapChainCreateDesc*)pDesc)->imageCount = 2;
	}

	SG_DECLARE_ZERO(VkSurfaceCapabilitiesKHR, caps);
	SG_CHECK_VKRESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pRenderer->pVkActiveGPU, vkSurface, &caps)); // if the device can present the image to the surface

	if ((caps.maxImageCount > 0) && (pDesc->imageCount > caps.maxImageCount))
	{
		SG_LOG_WARNING("Changed requested SwapChain images {%u} to maximum allowed SwapChain images {%u}", pDesc->imageCount, caps.maxImageCount);
		((SwapChainCreateDesc*)pDesc)->imageCount = caps.maxImageCount;
	}
	if (pDesc->imageCount < caps.minImageCount)
	{
		SG_LOG_WARNING("Changed requested SwapChain images {%u} to minimum required SwapChain images {%u}", pDesc->imageCount, caps.minImageCount);
		((SwapChainCreateDesc*)pDesc)->imageCount = caps.minImageCount;
	}

	// surface format
	// select a surface format, depending on whether HDR is available.
	VkSurfaceFormatKHR hdrSurfaceFormat = { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT };

	SG_DECLARE_ZERO(VkSurfaceFormatKHR, surfaceFormat);
	surfaceFormat.format = VK_FORMAT_UNDEFINED;
	uint32_t surfaceFormatCount = 0;
	VkSurfaceFormatKHR* formats = nullptr;

	// get surface formats count
	SG_CHECK_VKRESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(pRenderer->pVkActiveGPU, vkSurface, &surfaceFormatCount, nullptr));

	// allocate and get surface formats
	formats = (VkSurfaceFormatKHR*)sg_calloc(surfaceFormatCount, sizeof(*formats));
	SG_CHECK_VKRESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(pRenderer->pVkActiveGPU, vkSurface, &surfaceFormatCount, formats));

	if ((1 == surfaceFormatCount) && (VK_FORMAT_UNDEFINED == formats[0].format)) // no hdr format
	{
		surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
		surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	}
	else
	{
		auto requestedFormat = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->colorFormat);
		VkColorSpaceKHR requestedColorSpace = requestedFormat == hdrSurfaceFormat.format ? hdrSurfaceFormat.colorSpace : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		for (uint32_t i = 0; i < surfaceFormatCount; ++i)
		{
			if ((requestedFormat == formats[i].format) && (requestedColorSpace == formats[i].colorSpace))
			{
				surfaceFormat.format = requestedFormat;
				surfaceFormat.colorSpace = requestedColorSpace;
				break;
			}
		}

		// default to VK_FORMAT_B8G8R8A8_UNORM if requested format isn't found
		if (VK_FORMAT_UNDEFINED == surfaceFormat.format)
		{
			surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
			surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		}
	}

	ASSERT(VK_FORMAT_UNDEFINED != surfaceFormat.format);

	// free formats
	SG_SAFE_FREE(formats);

	// The VK_presentMode_FIFO_KHR mode must always be present as per spec
	// This mode waits for the vertical blank ("v-sync")
	VkPresentModeKHR  presentMode = VK_PRESENT_MODE_FIFO_KHR;
	uint32_t swapChainImageCount = 0;
	VkPresentModeKHR* modes = nullptr;
	// Get present mode count
	SG_CHECK_VKRESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(pRenderer->pVkActiveGPU, vkSurface, &swapChainImageCount, nullptr));

	// Allocate and get present modes
	modes = (VkPresentModeKHR*)alloca(swapChainImageCount * sizeof(*modes));
	SG_CHECK_VKRESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(pRenderer->pVkActiveGPU, vkSurface, &swapChainImageCount, modes));

	const uint32_t preferredModeCount = 4;
	VkPresentModeKHR preferredModeList[preferredModeCount] = { VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR /* triple buffer */,
		VK_PRESENT_MODE_FIFO_RELAXED_KHR, VK_PRESENT_MODE_FIFO_KHR }; // for VSync
	uint32_t preferredModeStartIndex = pDesc->enableVsync ? 2 : 0;

	for (uint32_t j = preferredModeStartIndex; j < preferredModeCount; ++j)
	{
		VkPresentModeKHR mode = preferredModeList[j];
		uint32_t i = 0;
		for (; i < swapChainImageCount; ++i)
		{
			if (modes[i] == mode)
			{
				break;
			}
		}
		if (i < swapChainImageCount)
		{
			presentMode = mode;
			break;
		}
	}

	// swapchain
	VkExtent2D extent = { 0 };
	extent.width = eastl::clamp(pDesc->width, caps.minImageExtent.width, caps.maxImageExtent.width);
	extent.height = eastl::clamp(pDesc->height, caps.minImageExtent.height, caps.maxImageExtent.height);

	VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	uint32_t queueFamilyIndexCount = 0;
	uint32_t queueFamilyIndices[2] = { pDesc->ppPresentQueues[0]->vkQueueFamilyIndex, 0 };
	uint32_t presentQueueFamilyIndex = -1;

	// get queue family properties
	uint32_t queueFamilyPropertyCount = 0;
	VkQueueFamilyProperties* queueFamilyProperties = nullptr;
	vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->pVkActiveGPU, &queueFamilyPropertyCount, nullptr);
	queueFamilyProperties = (VkQueueFamilyProperties*)alloca(queueFamilyPropertyCount * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->pVkActiveGPU, &queueFamilyPropertyCount, queueFamilyProperties);

	// check if hardware provides dedicated present queue
	if (queueFamilyPropertyCount)
	{
		for (uint32_t index = 0; index < queueFamilyPropertyCount; ++index)
		{
			VkBool32 supports_present = VK_FALSE;
			VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(pRenderer->pVkActiveGPU, index, vkSurface, &supports_present);
			if ((VK_SUCCESS == res) && (VK_TRUE == supports_present) && pDesc->ppPresentQueues[0]->vkQueueFamilyIndex != index)
			{
				presentQueueFamilyIndex = index;
				break;
			}
		}

		// If there is no dedicated present queue, just find the first available queue which supports present
		if (presentQueueFamilyIndex == -1)
		{
			for (uint32_t index = 0; index < queueFamilyPropertyCount; ++index)
			{
				VkBool32 supports_present = VK_FALSE;
				VkResult res =
					vkGetPhysicalDeviceSurfaceSupportKHR(pRenderer->pVkActiveGPU, index, vkSurface, &supports_present);
				if ((VK_SUCCESS == res) && (VK_TRUE == supports_present))
				{
					presentQueueFamilyIndex = index;
					break;
				}
				else
				{
					// No present queue family available. something goes wrong.
					ASSERT(0);
				}
			}
		}
	}

	// find if gpu has a dedicated present queue
	VkQueue presentQueue;
	uint32_t finalPresentQueueFamilyIndex;
	if (presentQueueFamilyIndex != -1 && queueFamilyIndices[0] != presentQueueFamilyIndex)
	{
		queueFamilyIndices[0] = presentQueueFamilyIndex;
		vkGetDeviceQueue(pRenderer->pVkDevice, queueFamilyIndices[0], 0, &presentQueue);
		queueFamilyIndexCount = 1;
		finalPresentQueueFamilyIndex = presentQueueFamilyIndex;
	}
	else
	{
		finalPresentQueueFamilyIndex = queueFamilyIndices[0];
		presentQueue = VK_NULL_HANDLE;
	}

	VkSurfaceTransformFlagBitsKHR preTransform;
	// #TODO: Add more if necessary but identity should be enough for now
	if (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		preTransform = caps.currentTransform;
	}

	VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[] =
	{
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
	};
	VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR;
	for (VkCompositeAlphaFlagBitsKHR flag : compositeAlphaFlags)
	{
		if (caps.supportedCompositeAlpha & flag)
		{
			compositeAlpha = flag;
			break;
		}
	}

	ASSERT(compositeAlpha != VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR);

	VkSwapchainKHR vkSwapchain;
	SG_DECLARE_ZERO(VkSwapchainCreateInfoKHR, swapChainCreateInfo);
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.pNext = nullptr;
	swapChainCreateInfo.flags = 0;
	swapChainCreateInfo.surface = vkSurface;
	swapChainCreateInfo.minImageCount = pDesc->imageCount;
	swapChainCreateInfo.imageFormat = surfaceFormat.format;
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainCreateInfo.imageExtent = extent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainCreateInfo.imageSharingMode = sharingMode;
	swapChainCreateInfo.queueFamilyIndexCount = queueFamilyIndexCount;
	swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	swapChainCreateInfo.preTransform = preTransform;
	swapChainCreateInfo.compositeAlpha = compositeAlpha;
	swapChainCreateInfo.presentMode = presentMode;
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.oldSwapchain = nullptr;
	SG_CHECK_VKRESULT(vkCreateSwapchainKHR(pRenderer->pVkDevice, &swapChainCreateInfo, nullptr, &vkSwapchain));

	((SwapChainCreateDesc*)pDesc)->colorFormat = TinyImageFormat_FromVkFormat((TinyImageFormat_VkFormat)surfaceFormat.format);

	// create render targets from swapchain
	// get the images from swap chain
	uint32_t imageCount = 0;
	SG_CHECK_VKRESULT(vkGetSwapchainImagesKHR(pRenderer->pVkDevice, vkSwapchain, &imageCount, nullptr));
	ASSERT(imageCount == pDesc->imageCount);
	auto* images = (VkImage*)alloca(imageCount * sizeof(VkImage));
	SG_CHECK_VKRESULT(vkGetSwapchainImagesKHR(pRenderer->pVkDevice, vkSwapchain, &imageCount, images));

	auto* pSwapChain = (SwapChain*)sg_calloc(1, sizeof(SwapChain) + imageCount * sizeof(RenderTarget*) + sizeof(SwapChainCreateDesc));
	pSwapChain->ppRenderTargets = (RenderTarget**)(pSwapChain + 1);
	pSwapChain->pDesc = (SwapChainCreateDesc*)(pSwapChain->ppRenderTargets + imageCount);
	ASSERT(pSwapChain);

	RenderTargetCreateDesc descColor = {};
	descColor.width = pDesc->width;
	descColor.height = pDesc->height;
	descColor.depth = 1;
	descColor.arraySize = 1;
	descColor.format = pDesc->colorFormat;
	descColor.clearValue = pDesc->colorClearValue;
	descColor.sampleCount = SG_SAMPLE_COUNT_1;
	descColor.sampleQuality = 0;

	RenderTargetBarrier barriers[SG_MAX_SWAPCHAIN_IMAGES] = {};

	// populate the vk_image field and add the Vulkan texture objects
	for (uint32_t i = 0; i < imageCount; ++i)
	{
		descColor.pNativeHandle = (void*)images[i];
		add_render_target(pRenderer, &descColor, &pSwapChain->ppRenderTargets[i]);
		barriers[i] = { pSwapChain->ppRenderTargets[i], SG_RESOURCE_STATE_UNDEFINED, SG_RESOURCE_STATE_PRESENT }; // for present
	}

	Queue* queue = nullptr;
	CmdPool* cmdPool = nullptr;
	Cmd* cmd = nullptr;
	Fence* fence = nullptr;

	QueueCreateDesc queueDesc = {};
	queueDesc.type = SG_QUEUE_TYPE_GRAPHICS;
	add_queue(pRenderer, &queueDesc, &queue);

	CmdPoolCreateDesc cmdPoolDesc = {};
	cmdPoolDesc.pQueue = queue;
	add_command_pool(pRenderer, &cmdPoolDesc, &cmdPool);

	CmdCreateDesc cmdDesc = {};
	cmdDesc.pPool = cmdPool;
	add_cmd(pRenderer, &cmdDesc, &cmd);
	add_fence(pRenderer, &fence);
	// just for resource barrier
	begin_cmd(cmd);
	cmd_resource_barrier(cmd, 0, nullptr, 0, nullptr, imageCount, barriers);
	end_cmd(cmd);

	QueueSubmitDesc submitDesc = {};
	submitDesc.cmdCount = 1;
	submitDesc.ppCmds = &cmd;
	submitDesc.pSignalFence = fence;

	queue_submit(queue, &submitDesc);
	wait_for_fences(pRenderer, 1, &fence);

	remove_fence(pRenderer, fence);
	remove_cmd(pRenderer, cmd);
	remove_command_pool(pRenderer, cmdPool);
	remove_queue(pRenderer, queue);

	*pSwapChain->pDesc = *pDesc;
	pSwapChain->enableVsync = pDesc->enableVsync;
	pSwapChain->imageCount = imageCount;
	pSwapChain->pVkSurface = vkSurface;
	pSwapChain->presentQueueFamilyIndex = finalPresentQueueFamilyIndex;
	pSwapChain->pPresentQueue = presentQueue;
	pSwapChain->pSwapChain = vkSwapchain;

	*ppSwapChain = pSwapChain;
}

void remove_swapchain(Renderer* pRenderer, SwapChain* pSwapChain)
{
	ASSERT(pRenderer);
	ASSERT(pSwapChain);

	for (uint32_t i = 0; i < pSwapChain->imageCount; ++i)
	{
		remove_render_target(pRenderer, pSwapChain->ppRenderTargets[i]);
	}

	vkDestroySwapchainKHR(pRenderer->pVkDevice, pSwapChain->pSwapChain, nullptr);
	vkDestroySurfaceKHR(pRenderer->pVkInstance, pSwapChain->pVkSurface, nullptr);

	SG_SAFE_FREE(pSwapChain);
}

void toggle_VSync(Renderer* pRenderer, SwapChain** ppSwapChain)
{
	SwapChain* pSwapChain = *ppSwapChain;

	Queue queue = {};
	queue.vkQueueFamilyIndex = pSwapChain->presentQueueFamilyIndex;
	Queue* queues[] = { &queue };

	SwapChainCreateDesc desc = *pSwapChain->pDesc;
	desc.enableVsync = !desc.enableVsync;
	desc.presentQueueCount = 1;
	desc.ppPresentQueues = queues;

	// toggle vsync on or off
	// for Vulkan we need to remove the SwapChain and recreate it with correct vsync option
	remove_swapchain(pRenderer, pSwapChain);
	add_swapchain(pRenderer, &desc, ppSwapChain);
}

#pragma endregion (SwapChain)

#pragma region (Command Buffer Function)

void reset_command_pool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	ASSERT(pRenderer);
	ASSERT(pCmdPool);

	VkResult vkRes = vkResetCommandPool(pRenderer->pVkDevice, pCmdPool->pVkCmdPool, 0);
	ASSERT(VK_SUCCESS == vkRes);
}

// begin command buffer
void begin_cmd(Cmd* pCmd)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	SG_DECLARE_ZERO(VkCommandBufferBeginInfo, beginInfo);
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	VkDeviceGroupCommandBufferBeginInfoKHR deviceGroupBeginInfo = { VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO_KHR, nullptr };
	if (pCmd->pRenderer->gpuMode == SG_GPU_MODE_LINKED)
	{
		deviceGroupBeginInfo.deviceMask = (1 << pCmd->nodeIndex);
		beginInfo.pNext = &deviceGroupBeginInfo;
	}

	VkResult vkRes = vkBeginCommandBuffer(pCmd->pVkCmdBuf, &beginInfo);
	ASSERT(VK_SUCCESS == vkRes);

	// reset CPU side data
	pCmd->pBoundPipelineLayout = nullptr;
}

// end render pass and command buffer
void end_cmd(Cmd* pCmd)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	if (pCmd->pVkActiveRenderPass)
		vkCmdEndRenderPass(pCmd->pVkCmdBuf);

	pCmd->pVkActiveRenderPass = VK_NULL_HANDLE;

	VkResult vkRes = vkEndCommandBuffer(pCmd->pVkCmdBuf);
	ASSERT(VK_SUCCESS == vkRes);
}

void cmd_resource_barrier(Cmd* pCmd,
	uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers,
	uint32_t numTextureBarriers, TextureBarrier* pTextureBarriers,
	uint32_t numRtBarriers, RenderTargetBarrier* pRtBarriers)
{
	VkImageMemoryBarrier* imageBarriers =
		(numTextureBarriers + numRtBarriers) ? (VkImageMemoryBarrier*)alloca((numTextureBarriers + numRtBarriers) * sizeof(VkImageMemoryBarrier)) : nullptr;
	uint32_t imageBarrierCount = 0;

	VkBufferMemoryBarrier* bufferBarriers =
		numBufferBarriers ? (VkBufferMemoryBarrier*)alloca(numBufferBarriers * sizeof(VkBufferMemoryBarrier)) : nullptr;
	uint32_t bufferBarrierCount = 0;

	VkAccessFlags srcAccessFlags = 0;
	VkAccessFlags dstAccessFlags = 0;

	for (uint32_t i = 0; i < numBufferBarriers; ++i)
	{
		BufferBarrier* pTrans = &pBufferBarriers[i];
		Buffer* pBuffer = pTrans->pBuffer;
		VkBufferMemoryBarrier* pBufferBarrier = nullptr;

		if (SG_RESOURCE_STATE_UNORDERED_ACCESS == pTrans->currentState &&
			SG_RESOURCE_STATE_UNORDERED_ACCESS == pTrans->newState)
		{
			pBufferBarrier = &bufferBarriers[bufferBarrierCount++];
			pBufferBarrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			pBufferBarrier->pNext = nullptr;

			pBufferBarrier->srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			pBufferBarrier->dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
		}
		else
		{
			pBufferBarrier = &bufferBarriers[bufferBarrierCount++];
			pBufferBarrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			pBufferBarrier->pNext = nullptr;

			pBufferBarrier->srcAccessMask = util_to_vk_access_flags(pTrans->currentState);
			pBufferBarrier->dstAccessMask = util_to_vk_access_flags(pTrans->newState);
		}

		if (pBufferBarrier)
		{
			pBufferBarrier->buffer = pBuffer->pVkBuffer;
			pBufferBarrier->size = VK_WHOLE_SIZE;
			pBufferBarrier->offset = 0;

			if (pTrans->acquire) // current queue to the dst queue
			{
				pBufferBarrier->srcQueueFamilyIndex = pCmd->pRenderer->queueFamilyIndices[pTrans->queueType];
				pBufferBarrier->dstQueueFamilyIndex = pCmd->pQueue->vkQueueFamilyIndex;
			}
			else if (pTrans->release)
			{
				pBufferBarrier->srcQueueFamilyIndex = pCmd->pQueue->vkQueueFamilyIndex;
				pBufferBarrier->dstQueueFamilyIndex = pCmd->pRenderer->queueFamilyIndices[pTrans->queueType];
			}
			else
			{
				pBufferBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				pBufferBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			}

			srcAccessFlags |= pBufferBarrier->srcAccessMask;
			dstAccessFlags |= pBufferBarrier->dstAccessMask;
		}
	}

	for (uint32_t i = 0; i < numTextureBarriers; ++i)
	{
		TextureBarrier* pTrans = &pTextureBarriers[i];
		Texture* pTexture = pTrans->pTexture;
		VkImageMemoryBarrier* pImageBarrier = nullptr;

		if (SG_RESOURCE_STATE_UNORDERED_ACCESS == pTrans->currentState &&
			SG_RESOURCE_STATE_UNORDERED_ACCESS == pTrans->newState)
		{
			pImageBarrier = &imageBarriers[imageBarrierCount++];
			pImageBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			pImageBarrier->pNext = nullptr;

			pImageBarrier->srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			pImageBarrier->dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			pImageBarrier->oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			pImageBarrier->newLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		else
		{
			pImageBarrier = &imageBarriers[imageBarrierCount++];
			pImageBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			pImageBarrier->pNext = nullptr;

			pImageBarrier->srcAccessMask = util_to_vk_access_flags(pTrans->currentState);
			pImageBarrier->dstAccessMask = util_to_vk_access_flags(pTrans->newState);
			pImageBarrier->oldLayout = util_to_vk_image_layout(pTrans->currentState);
			pImageBarrier->newLayout = util_to_vk_image_layout(pTrans->newState);
		}

		if (pImageBarrier)
		{
			pImageBarrier->image = pTexture->pVkImage;
			pImageBarrier->subresourceRange.aspectMask = (VkImageAspectFlags)pTexture->aspectMask;
			pImageBarrier->subresourceRange.baseMipLevel = pTrans->subresourceBarrier ? pTrans->mipLevel : 0;
			pImageBarrier->subresourceRange.levelCount = pTrans->subresourceBarrier ? 1 : VK_REMAINING_MIP_LEVELS;

			pImageBarrier->subresourceRange.baseArrayLayer = pTrans->subresourceBarrier ? pTrans->arrayLayer : 0;
			pImageBarrier->subresourceRange.layerCount = pTrans->subresourceBarrier ? 1 : VK_REMAINING_ARRAY_LAYERS;

			if (pTrans->acquire)
			{
				pImageBarrier->srcQueueFamilyIndex = pCmd->pRenderer->queueFamilyIndices[pTrans->queueType];
				pImageBarrier->dstQueueFamilyIndex = pCmd->pQueue->vkQueueFamilyIndex;
			}
			else if (pTrans->release)
			{
				pImageBarrier->srcQueueFamilyIndex = pCmd->pQueue->vkQueueFamilyIndex;
				pImageBarrier->dstQueueFamilyIndex = pCmd->pRenderer->queueFamilyIndices[pTrans->queueType];
			}
			else
			{
				pImageBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				pImageBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			}

			srcAccessFlags |= pImageBarrier->srcAccessMask;
			dstAccessFlags |= pImageBarrier->dstAccessMask;
		}
	}

	for (uint32_t i = 0; i < numRtBarriers; ++i)
	{
		RenderTargetBarrier* pTrans = &pRtBarriers[i];
		Texture* pTexture = pTrans->pRenderTarget->pTexture;
		VkImageMemoryBarrier* pImageBarrier = nullptr;

		if (SG_RESOURCE_STATE_UNORDERED_ACCESS == pTrans->currentState &&
			SG_RESOURCE_STATE_UNORDERED_ACCESS == pTrans->newState)
		{
			pImageBarrier = &imageBarriers[imageBarrierCount++];
			pImageBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			pImageBarrier->pNext = nullptr;

			pImageBarrier->srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			pImageBarrier->dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			pImageBarrier->oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			pImageBarrier->newLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		else
		{
			pImageBarrier = &imageBarriers[imageBarrierCount++];
			pImageBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			pImageBarrier->pNext = nullptr;

			pImageBarrier->srcAccessMask = util_to_vk_access_flags(pTrans->currentState);
			pImageBarrier->dstAccessMask = util_to_vk_access_flags(pTrans->newState);
			// if it is the first one, we use VK_IMAGE_LAYOUT_UNDEFINED
			pImageBarrier->oldLayout = pTrans->pRenderTarget->used++ ? util_to_vk_image_layout(pTrans->currentState) : VK_IMAGE_LAYOUT_UNDEFINED;
			pImageBarrier->newLayout = util_to_vk_image_layout(pTrans->newState);
		}

		if (pImageBarrier)
		{
			pImageBarrier->image = pTexture->pVkImage;
			pImageBarrier->subresourceRange.aspectMask = (VkImageAspectFlags)pTexture->aspectMask;
			pImageBarrier->subresourceRange.baseMipLevel = pTrans->subresourceBarrier ? pTrans->mipLevel : 0;
			pImageBarrier->subresourceRange.levelCount = pTrans->subresourceBarrier ? 1 : VK_REMAINING_MIP_LEVELS;

			pImageBarrier->subresourceRange.baseArrayLayer = pTrans->subresourceBarrier ? pTrans->arrayLayer : 0;
			pImageBarrier->subresourceRange.layerCount = pTrans->subresourceBarrier ? 1 : VK_REMAINING_ARRAY_LAYERS;

			if (pTrans->acquire)
			{
				pImageBarrier->srcQueueFamilyIndex = pCmd->pRenderer->queueFamilyIndices[pTrans->queueType];
				pImageBarrier->dstQueueFamilyIndex = pCmd->pQueue->vkQueueFamilyIndex;
			}
			else if (pTrans->release)
			{
				pImageBarrier->srcQueueFamilyIndex = pCmd->pQueue->vkQueueFamilyIndex;
				pImageBarrier->dstQueueFamilyIndex = pCmd->pRenderer->queueFamilyIndices[pTrans->queueType];
			}
			else
			{
				pImageBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				pImageBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			}

			srcAccessFlags |= pImageBarrier->srcAccessMask;
			dstAccessFlags |= pImageBarrier->dstAccessMask;
		}
	}

	VkPipelineStageFlags srcStageMask = util_determine_pipeline_stage_flags(pCmd->pRenderer, srcAccessFlags, (QueueType)pCmd->type);
	VkPipelineStageFlags dstStageMask = util_determine_pipeline_stage_flags(pCmd->pRenderer, dstAccessFlags, (QueueType)pCmd->type);

	if (bufferBarrierCount || imageBarrierCount)
	{
		vkCmdPipelineBarrier(pCmd->pVkCmdBuf, srcStageMask, dstStageMask, 0,
			0, nullptr, // we don't use memory barrier now
			bufferBarrierCount, bufferBarriers,
			imageBarrierCount, imageBarriers);
	}
}

#pragma endregion (Command Buffer Function)

#pragma region (Default Resourece)

	// create default resources to be used a null descriptors in case user does not specify some descriptors
	static VkPipelineRasterizationStateCreateInfo gDefaultRasterizerDesc = {};
	static VkPipelineDepthStencilStateCreateInfo gDefaultDepthDesc = {};
	static VkPipelineColorBlendStateCreateInfo gDefaultBlendDesc = {};
	static VkPipelineColorBlendAttachmentState gDefaultBlendAttachments[SG_MAX_RENDER_TARGET_ATTACHMENTS] = {};

	typedef struct NullDescriptors
	{
		Texture* pDefaultTextureSRV[SG_MAX_LINKED_GPUS][SG_TEXTURE_DIM_COUNT];
		Texture* pDefaultTextureUAV[SG_MAX_LINKED_GPUS][SG_TEXTURE_DIM_COUNT];
		Buffer*  pDefaultBufferSRV[SG_MAX_LINKED_GPUS];
		Buffer*  pDefaultBufferUAV[SG_MAX_LINKED_GPUS];
		Sampler* pDefaultSampler;
		Mutex    mSubmitMutex;
	} NullDescriptors;

	static void add_default_resources(Renderer* pRenderer)
	{
		pRenderer->pNullDescriptors = (NullDescriptors*)sg_calloc(1, sizeof(NullDescriptors));
		pRenderer->pNullDescriptors->mSubmitMutex.Init();

		for (uint32_t i = 0; i < pRenderer->linkedNodeCount; ++i) // the count of GPU device
		{
			// 1D texture
			TextureCreateDesc textureDesc = {};
			textureDesc.nodeIndex = i;
			textureDesc.arraySize = 1;
			textureDesc.depth = 1;
			textureDesc.format = TinyImageFormat_R8G8B8A8_UNORM;
			textureDesc.height = 1;
			textureDesc.mipLevels = 1;
			textureDesc.sampleCount = SG_SAMPLE_COUNT_1;
			textureDesc.startState = SG_RESOURCE_STATE_COMMON;
			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE;
			textureDesc.width = 1;
			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_1D]);
			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_RW_TEXTURE;
			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][SG_TEXTURE_DIM_1D]);

			// 1D texture array
			textureDesc.arraySize = 2;
			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE;
			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_1D_ARRAY]);
			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_RW_TEXTURE;
			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][SG_TEXTURE_DIM_1D_ARRAY]);

			// 2D texture
			textureDesc.width = 2;
			textureDesc.height = 2;
			textureDesc.arraySize = 1;
			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE;
			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_2D]);
			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_RW_TEXTURE;
			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][SG_TEXTURE_DIM_2D]);

			// 2D MS texture
			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE;
			textureDesc.sampleCount = SG_SAMPLE_COUNT_2;
			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_2DMS]);
			textureDesc.sampleCount = SG_SAMPLE_COUNT_1;

			// 2D texture array
			textureDesc.arraySize = 2;
			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_2D_ARRAY]);
			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_RW_TEXTURE;
			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][SG_TEXTURE_DIM_2D_ARRAY]);

			// 2D MS texture array
			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE;
			textureDesc.sampleCount = SG_SAMPLE_COUNT_2;
			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_2DMS_ARRAY]);
			textureDesc.sampleCount = SG_SAMPLE_COUNT_1;

			// 3D texture
			textureDesc.depth = 2;
			textureDesc.arraySize = 1;
			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_3D]);
			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_RW_TEXTURE;
			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][SG_TEXTURE_DIM_3D]);

			// Cube texture
			textureDesc.depth = 1;
			textureDesc.arraySize = 6;
			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE_CUBE;
			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_CUBE]);
			textureDesc.arraySize = 6 * 2;
			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_CUBE_ARRAY]);

			BufferCreateDesc bufferDesc = {};
			bufferDesc.nodeIndex = i;
			bufferDesc.descriptors = SG_DESCRIPTOR_TYPE_BUFFER | SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			bufferDesc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_GPU_ONLY;
			bufferDesc.startState = SG_RESOURCE_STATE_COMMON;
			bufferDesc.size = sizeof(uint32_t);
			bufferDesc.firstElement = 0;
			bufferDesc.elementCount = 1;
			bufferDesc.structStride = sizeof(uint32_t);
			bufferDesc.format = TinyImageFormat_R32_UINT;
			add_buffer(pRenderer, &bufferDesc, &pRenderer->pNullDescriptors->pDefaultBufferSRV[i]);
			bufferDesc.descriptors = SG_DESCRIPTOR_TYPE_RW_BUFFER;
			add_buffer(pRenderer, &bufferDesc, &pRenderer->pNullDescriptors->pDefaultBufferUAV[i]);
		}

		SamplerCreateDesc samplerDesc = {};
		samplerDesc.addressU = SG_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerDesc.addressV = SG_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerDesc.addressW = SG_ADDRESS_MODE_CLAMP_TO_BORDER;
		add_sampler(pRenderer, &samplerDesc, &pRenderer->pNullDescriptors->pDefaultSampler);

		BlendStateDesc blendStateDesc = {};
		// No blending
		blendStateDesc.dstAlphaFactors[0] = SG_BLEND_CONST_ZERO;
		blendStateDesc.dstFactors[0] = SG_BLEND_CONST_ZERO;
		blendStateDesc.srcAlphaFactors[0] = SG_BLEND_CONST_ONE;
		blendStateDesc.srcFactors[0] = SG_BLEND_CONST_ONE;

		blendStateDesc.masks[0] = SG_BLEND_COLOR_MASK_ALL;
		blendStateDesc.renderTargetMask = SG_BLEND_STATE_TARGET_ALL; // apply to all the render targets
		blendStateDesc.independentBlend = false;
		gDefaultBlendDesc = util_to_blend_desc(&blendStateDesc, gDefaultBlendAttachments);

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.depthFunc = SG_COMPARE_MODE_LEQUAL;
		depthStateDesc.depthTest = false;
		depthStateDesc.depthWrite = false;
		depthStateDesc.stencilBackFunc = SG_COMPARE_MODE_ALWAYS;
		depthStateDesc.stencilFrontFunc = SG_COMPARE_MODE_ALWAYS;
		depthStateDesc.stencilReadMask = 0xFF;
		depthStateDesc.stencilWriteMask = 0xFF;
		gDefaultDepthDesc = util_to_depth_desc(&depthStateDesc);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.cullMode = SG_CULL_MODE_BACK;
		gDefaultRasterizerDesc = util_to_rasterizer_desc(&rasterizerStateDesc);

		// Create command buffer to transition resources to the correct state
		Queue* graphicsQueue = {};
		CmdPool* cmdPool = {};
		Cmd* cmd = {};

		QueueCreateDesc queueDesc = {};
		queueDesc.type = SG_QUEUE_TYPE_GRAPHICS;
		add_queue(pRenderer, &queueDesc, &graphicsQueue);

		CmdPoolCreateDesc cmdPoolDesc = {};
		cmdPoolDesc.pQueue = graphicsQueue;
		cmdPoolDesc.transient = true; // for temporary use
		add_command_pool(pRenderer, &cmdPoolDesc, &cmdPool);

		CmdCreateDesc cmdDesc = {};
		cmdDesc.pPool = cmdPool;
		add_cmd(pRenderer, &cmdDesc, &cmd);

		// Transition resources
		begin_cmd(cmd);

		BufferBarrier bufferBarriers[SG_MAX_LINKED_GPUS * 2] = {};
		uint32_t bufferBarrierCount = 0;
		TextureBarrier textureBarriers[SG_MAX_LINKED_GPUS * SG_TEXTURE_DIM_COUNT * 2] = {};
		uint32_t textureBarrierCount = 0;

		for (uint32_t i = 0; i < pRenderer->linkedNodeCount; ++i)
		{
			for (uint32_t dim = 0; dim < SG_TEXTURE_DIM_COUNT; ++dim)
			{
				if (pRenderer->pNullDescriptors->pDefaultTextureSRV[i][dim])
					textureBarriers[textureBarrierCount++] = { pRenderer->pNullDescriptors->pDefaultTextureSRV[i][dim], SG_RESOURCE_STATE_UNDEFINED, SG_RESOURCE_STATE_SHADER_RESOURCE };

				if (pRenderer->pNullDescriptors->pDefaultTextureUAV[i][dim])
					textureBarriers[textureBarrierCount++] = { pRenderer->pNullDescriptors->pDefaultTextureUAV[i][dim], SG_RESOURCE_STATE_UNDEFINED, SG_RESOURCE_STATE_UNORDERED_ACCESS };
			}

			bufferBarriers[bufferBarrierCount++] = { pRenderer->pNullDescriptors->pDefaultBufferSRV[i], SG_RESOURCE_STATE_UNDEFINED, SG_RESOURCE_STATE_SHADER_RESOURCE };
			bufferBarriers[bufferBarrierCount++] = { pRenderer->pNullDescriptors->pDefaultBufferUAV[i], SG_RESOURCE_STATE_UNDEFINED, SG_RESOURCE_STATE_UNORDERED_ACCESS };
		}

		cmd_resource_barrier(cmd, bufferBarrierCount, bufferBarriers, textureBarrierCount, textureBarriers, 0, nullptr);
		end_cmd(cmd);

		QueueSubmitDesc submitDesc = {};
		submitDesc.cmdCount = 1;
		submitDesc.ppCmds = &cmd;
		queue_submit(graphicsQueue, &submitDesc);
		wait_queue_idle(graphicsQueue);

		// delete temporary resources
		remove_cmd(pRenderer, cmd);
		remove_command_pool(pRenderer, cmdPool);
		remove_queue(pRenderer, graphicsQueue);
	}

	static void remove_default_resources(Renderer* pRenderer)
	{
		for (uint32_t i = 0; i < pRenderer->linkedNodeCount; ++i)
		{
			for (uint32_t dim = 0; dim < SG_TEXTURE_DIM_COUNT; ++dim)
			{
				if (pRenderer->pNullDescriptors->pDefaultTextureSRV[i][dim])
					remove_texture(pRenderer, pRenderer->pNullDescriptors->pDefaultTextureSRV[i][dim]);

				if (pRenderer->pNullDescriptors->pDefaultTextureUAV[i][dim])
					remove_texture(pRenderer, pRenderer->pNullDescriptors->pDefaultTextureUAV[i][dim]);
			}

			remove_buffer(pRenderer, pRenderer->pNullDescriptors->pDefaultBufferSRV[i]);
			remove_buffer(pRenderer, pRenderer->pNullDescriptors->pDefaultBufferUAV[i]);
		}

		remove_sampler(pRenderer, pRenderer->pNullDescriptors->pDefaultSampler);

		pRenderer->pNullDescriptors->mSubmitMutex.Destroy();
		SG_SAFE_FREE(pRenderer->pNullDescriptors);
	}

#pragma endregion (Default Resourece)

#pragma region (Root Signature)

	typedef struct UpdateFrequencyLayoutInfo
	{
		/// array of all bindings in the descriptor set
		eastl::vector<VkDescriptorSetLayoutBinding> bindings{};
		/// array of all descriptors in this descriptor set
		eastl::vector<DescriptorInfo*> descriptors{};
		/// array of all descriptors marked as dynamic in this descriptor set (applicable to DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		eastl::vector<DescriptorInfo*> dynamicDescriptors{};
		/// hash map to get index of the descriptor in the root signature
		eastl::hash_map<DescriptorInfo*, uint32_t> descriptorIndexMap{};
	} UpdateFrequencyLayoutInfo;

	void add_root_signature(Renderer* pRenderer, const RootSignatureCreateDesc* pRootSignatureDesc, RootSignature** ppRootSignature)
	{
		ASSERT(pRenderer);
		ASSERT(pRootSignatureDesc);
		ASSERT(ppRootSignature);

		static constexpr uint32_t                kMaxLayoutCount = SG_DESCRIPTOR_UPDATE_FREQ_COUNT;
		UpdateFrequencyLayoutInfo                layouts[kMaxLayoutCount] = {};
		VkPushConstantRange                      pushConstants[SG_SHADER_STAGE_COUNT] = {};
		uint32_t                                 pushConstantCount = 0;
		eastl::vector<ShaderResource>            shaderResources;
		eastl::hash_map<eastl::string, Sampler*> staticSamplerMap;

		for (uint32_t i = 0; i < pRootSignatureDesc->staticSamplerCount; ++i)
		{
			ASSERT(pRootSignatureDesc->ppStaticSamplers[i]);
			staticSamplerMap.insert({ { pRootSignatureDesc->ppStaticSamplerNames[i], pRootSignatureDesc->ppStaticSamplers[i] } });
		}

		PipelineType pipelineType = SG_PIPELINE_TYPE_UNDEFINED;
		DescriptorIndexMap indexMap;

		// collect all unique shader resources in the given shaders
		// resources are parsed by name (two resources named "XYZ" in two shaders will be considered the same resource)
		for (uint32_t sh = 0; sh < pRootSignatureDesc->shaderCount; ++sh)
		{
			PipelineReflection const* pReflection = pRootSignatureDesc->ppShaders[sh]->pReflection;

			if (pReflection->shaderStages & SG_SHADER_STAGE_COMP)
				pipelineType = SG_PIPELINE_TYPE_COMPUTE;
#ifdef SG_ENABLE_RAYTRACING
			else if (pReflection->shaderStages & SG_SHADER_STAGE_RAYTRACING)
				pipelineType = SG_PIPELINE_TYPE_RAYTRACING;
#endif
			else
				pipelineType = SG_PIPELINE_TYPE_GRAPHICS;

			for (uint32_t i = 0; i < pReflection->shaderResourceCount; ++i)
			{
				ShaderResource const* pRes = &pReflection->pShaderResources[i];
				uint32_t              setIndex = pRes->set;

				if (pRes->type == SG_DESCRIPTOR_TYPE_ROOT_CONSTANT)
					setIndex = 0;

				eastl::string_hash_map<uint32_t>::iterator it = indexMap.mMap.find(pRes->name);
				if (it == indexMap.mMap.end())
				{
					decltype(shaderResources)::iterator it = eastl::find(shaderResources.begin(), shaderResources.end(), *pRes,
						[](const ShaderResource& a, const ShaderResource& b)
						{
							return (a.type == b.type) && (a.usedStages == b.usedStages) && (((a.reg ^ b.reg) | (a.set ^ b.set)) == 0);
						});
					if (it == shaderResources.end())
					{
						indexMap.mMap.insert(pRes->name, (uint32_t)shaderResources.size());
						shaderResources.push_back(*pRes);
					}
					else
					{
						ASSERT(pRes->type == it->type);
						if (pRes->type != it->type)
						{
							SG_LOG_ERROR("\nFailed to create root signature\n"
								"Shared shader resources %s and %s have mismatching types (%u) and (%u). All shader resources "
								"sharing the same register and space add_root_signature "
								"must have the same type",
								pRes->name, it->name, (uint32_t)pRes->type, (uint32_t)it->type);
							return;
						}

						indexMap.mMap.insert(pRes->name, indexMap.mMap[it->name]);
						it->usedStages |= pRes->usedStages;
					}
				}
				else
				{
					if (shaderResources[it->second].reg != pRes->reg)
					{
						SG_LOG_ERROR("\nFailed to create root signature\n"
							"Shared shader resource %s has mismatching binding. All shader resources "
							"shared by multiple shaders specified in addRootSignature "
							"must have the same binding and set",
							pRes->name);
						return;
					}
					if (shaderResources[it->second].set != pRes->set)
					{
						SG_LOG_ERROR("\nFailed to create root signature\n"
							"Shared shader resource %s has mismatching set. All shader resources "
							"shared by multiple shaders specified in addRootSignature "
							"must have the same binding and set",
							pRes->name);
						return;
					}

					for (ShaderResource& res : shaderResources)
					{
						if (strcmp(res.name, it->first) == 0)
						{
							res.usedStages |= pRes->usedStages;
							break;
						}
					}
				}
			}
		}

		// memory management ( RootSignature + NumDescriptorInfo + DescriptorIndexMap )
		size_t totalSize = sizeof(RootSignature);
		totalSize += shaderResources.size() * sizeof(DescriptorInfo);
		totalSize += sizeof(DescriptorIndexMap);
		RootSignature* pRootSignature = (RootSignature*)sg_calloc_memalign(1, alignof(RootSignature), totalSize);
		ASSERT(pRootSignature);

		pRootSignature->descriptors = (DescriptorInfo*)(pRootSignature + 1); // offset to the first descriptor info
		pRootSignature->descriptorNameToIndexMap = (DescriptorIndexMap*)(pRootSignature->descriptors + shaderResources.size());
		ASSERT(pRootSignature->descriptorNameToIndexMap);
		sg_placement_new<DescriptorIndexMap>(pRootSignature->descriptorNameToIndexMap);

		if ((uint32_t)shaderResources.size())
		{
			pRootSignature->descriptorCount = (uint32_t)shaderResources.size();
		}

		pRootSignature->pipelineType = pipelineType;
		pRootSignature->descriptorNameToIndexMap->mMap = indexMap.mMap;

		// Fill the descriptor array to be stored in the root signature
		for (uint32_t i = 0; i < (uint32_t)shaderResources.size(); ++i)
		{
			DescriptorInfo* pDesc = &pRootSignature->descriptors[i];
			ShaderResource const* pRes = &shaderResources[i];
			uint32_t                  setIndex = pRes->set;
			DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)setIndex;

			// Copy the binding information generated from the shader reflection into the descriptor
			pDesc->reg = pRes->reg;
			pDesc->size = pRes->size;
			pDesc->type = pRes->type;
			pDesc->name = pRes->name;
			pDesc->dim = pRes->dim;

			// If descriptor is not a root constant create a new layout binding for this descriptor and add it to the binding array
			if (pDesc->type != SG_DESCRIPTOR_TYPE_ROOT_CONSTANT)
			{
				VkDescriptorSetLayoutBinding binding = {};
				binding.binding = pRes->reg;
				binding.descriptorCount = pDesc->size;
				binding.descriptorType = util_to_vk_descriptor_type((DescriptorType)pDesc->type);

				eastl::string name = pRes->name;
				name.make_lower();

				// If a user specified a uniform buffer to be used as a dynamic uniform buffer change its type to VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
				// Also log a message for debugging purpose
				if (name.find("rootcbv") != eastl::string::npos)
				{
					if (pDesc->size == 1)
					{
						SG_LOG_INFO("Descriptor (%s) : User specified VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC", pDesc->name);
						binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
					}
					else
					{
						SG_LOG_WARNING("Descriptor (%s) : Cannot use VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC for arrays",
							pDesc->name);
					}
				}

				binding.stageFlags = util_to_vk_shader_stage_flags(pRes->usedStages);

				// Store the vulkan related info in the descriptor to avoid constantly calling the util_to_vk mapping functions
				pDesc->vkType = binding.descriptorType;
				pDesc->vkStages = binding.stageFlags;
				pDesc->updateFrequency = updateFreq;

				if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
				{
					layouts[setIndex].dynamicDescriptors.emplace_back(pDesc);
				}

				// Find if the given descriptor is a static sampler
				decltype(staticSamplerMap)::iterator it = staticSamplerMap.find(pDesc->name);
				bool hasStaticSampler = it != staticSamplerMap.end();
				if (hasStaticSampler)
				{
					SG_LOG_INFO("Descriptor (%s) : User specified Static Sampler", pDesc->name);
					binding.pImmutableSamplers = &it->second->pVkSampler;
				}

				// Set the index to an invalid value so we can use this later for error checking if user tries to update a static sampler
				// In case of Combined Image Samplers, skip invalidating the index
				// because we do not to introduce new ways to update the descriptor in the Interface
				if (hasStaticSampler && pDesc->type != SG_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
				{
					pDesc->indexInParent = -1;
				}
				else
				{
					layouts[setIndex].descriptors.emplace_back(pDesc);
				}

				layouts[setIndex].bindings.push_back(binding);
			}
			// If descriptor is a root constant, add it to the root constant array
			else
			{
				SG_LOG_INFO("Descriptor (%s) : User specified Push Constant", pDesc->name);

				pDesc->vkStages = util_to_vk_shader_stage_flags(pRes->usedStages);
				setIndex = 0;
				pDesc->indexInParent = pushConstantCount++;

				pushConstants[pDesc->indexInParent] = {};
				pushConstants[pDesc->indexInParent].offset = 0;
				pushConstants[pDesc->indexInParent].size = pDesc->size;
				pushConstants[pDesc->indexInParent].stageFlags = pDesc->vkStages;
			}

			layouts[setIndex].descriptorIndexMap[pDesc] = i;
		}

		pRootSignature->vkPushConstantCount = pushConstantCount;

		// create descriptor layouts
		// put most frequently changed params first
		for (uint32_t i = kMaxLayoutCount; i-- > 0U;) // the number is bigger, the frequency is higher
		{
			UpdateFrequencyLayoutInfo& layout = layouts[i];

			if (layouts[i].bindings.size())
			{
				// sort table by type (CBV/SRV/UAV) by register
				eastl::stable_sort(layout.bindings.begin(), layout.bindings.end(), [](const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs) {
					return lhs.binding > rhs.binding;
					});
				eastl::stable_sort(layout.bindings.begin(), layout.bindings.end(), [](const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs) {
					return lhs.descriptorType > rhs.descriptorType;
					});
			}

			bool createLayout = layout.bindings.size() > 0;
			// check if we need to create an empty layout in case there is an empty set between two used sets
			// Example: set = 0 is used, set = 2 is used. In this case, set = 1 needs to exist even if it is empty
			if (!createLayout && i < kMaxLayoutCount - 1)
			{
				createLayout = pRootSignature->vkDescriptorSetLayouts[i + 1] != VK_NULL_HANDLE;
			}

			if (createLayout)
			{
				VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr };
				layoutInfo.bindingCount = (uint32_t)layout.bindings.size();
				layoutInfo.pBindings = layout.bindings.data();
				layoutInfo.flags = 0;

				SG_CHECK_VKRESULT(vkCreateDescriptorSetLayout(pRenderer->pVkDevice, &layoutInfo, nullptr, &pRootSignature->vkDescriptorSetLayouts[i]));
			}

			if (!layouts[i].bindings.size())
			{
				continue;
			}

			pRootSignature->vkDescriptorCounts[i] = (uint32_t)layout.descriptors.size();

			// Loop through descriptors belonging to this update frequency and increment the cumulative descriptor count
			for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.descriptors.size(); ++descIndex)
			{
				DescriptorInfo* pDesc = layout.descriptors[descIndex];
				pDesc->indexInParent = descIndex;
				pDesc->handleIndex = pRootSignature->vkCumulativeDescriptorCounts[i];
				pRootSignature->vkCumulativeDescriptorCounts[i] += pDesc->size;
			}

			pRootSignature->vkDynamicDescriptorCounts[i] = (uint32_t)layout.dynamicDescriptors.size();
			for (uint32_t descIndex = 0; descIndex < pRootSignature->vkDynamicDescriptorCounts[i]; ++descIndex)
			{
				DescriptorInfo* pDesc = layout.dynamicDescriptors[descIndex];
				pDesc->rootDescriptorIndex = descIndex;
			}
		}

		/// pipeline layout
		VkDescriptorSetLayout descriptorSetLayouts[kMaxLayoutCount] = {};
		uint32_t              descriptorSetLayoutCount = 0;
		for (uint32_t i = 0; i < SG_DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
		{
			if (pRootSignature->vkDescriptorSetLayouts[i])
			{
				descriptorSetLayouts[descriptorSetLayoutCount++] = pRootSignature->vkDescriptorSetLayouts[i];
			}
		}

		SG_DECLARE_ZERO(VkPipelineLayoutCreateInfo, addInfo);
		addInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		addInfo.pNext = nullptr;
		addInfo.flags = 0;
		addInfo.setLayoutCount = descriptorSetLayoutCount;
		addInfo.pSetLayouts = descriptorSetLayouts;
		addInfo.pushConstantRangeCount = pRootSignature->vkPushConstantCount;
		addInfo.pPushConstantRanges = pushConstants;
		SG_CHECK_VKRESULT(vkCreatePipelineLayout(pRenderer->pVkDevice, &addInfo, nullptr, &(pRootSignature->pPipelineLayout)));

		/// update templates
		for (uint32_t setIndex = 0; setIndex < SG_DESCRIPTOR_UPDATE_FREQ_COUNT; ++setIndex)
		{
			if (pRootSignature->vkDescriptorCounts[setIndex])
			{
				pRootSignature->pUpdateTemplateData[setIndex] = (void**)sg_calloc(pRenderer->linkedNodeCount, sizeof(DescriptorUpdateData*));

				const UpdateFrequencyLayoutInfo& layout = layouts[setIndex];
				auto* pEntries = (VkDescriptorUpdateTemplateEntry*)sg_malloc(pRootSignature->vkDescriptorCounts[setIndex] * sizeof(VkDescriptorUpdateTemplateEntry));
				uint32_t entryCount = 0;

				for (uint32_t nodeIndex = 0; nodeIndex < pRenderer->linkedNodeCount; ++nodeIndex)
				{
					pRootSignature->pUpdateTemplateData[setIndex][nodeIndex] = sg_calloc(pRootSignature->vkCumulativeDescriptorCounts[setIndex],
						sizeof(DescriptorUpdateData));
				}

				// fill the write descriptors with default values during initialize so the only thing we change in cmdBindDescriptors is the the VkBuffer / VkImageView objects
				for (uint32_t i = 0; i < (uint32_t)layout.descriptors.size(); ++i)
				{
					const DescriptorInfo* pDesc = layout.descriptors[i];
					const uint64_t offset = pDesc->handleIndex * sizeof(DescriptorUpdateData);

#ifdef VK_NV_RAY_TRACING_SPEC_VERSION
					// Raytracing descriptors don't support update template so we ignore them
					if (pDesc->type == SG_DESCRIPTOR_TYPE_RAY_TRACING)
					{
						pRootSignature->vkRaytracingDescriptorCounts[setIndex] += pDesc->size;
						continue;
					}
#endif

					pEntries[entryCount].descriptorCount = pDesc->size;
					pEntries[entryCount].descriptorType = (VkDescriptorType)pDesc->vkType;
					pEntries[entryCount].dstArrayElement = 0;
					pEntries[entryCount].dstBinding = pDesc->reg;
					pEntries[entryCount].offset = offset;
					pEntries[entryCount].stride = sizeof(DescriptorUpdateData);

					for (uint32_t nodeIndex = 0; nodeIndex < pRenderer->linkedNodeCount; ++nodeIndex)
					{
						DescriptorUpdateData* pUpdateData = (DescriptorUpdateData*)pRootSignature->pUpdateTemplateData[setIndex][nodeIndex];

						const DescriptorType type = (DescriptorType)pDesc->type;
						const uint32_t arrayCount = pDesc->size;

						switch (type)
						{
						case SG_DESCRIPTOR_TYPE_SAMPLER:
						{
							for (uint32_t arr = 0; arr < arrayCount; ++arr)
								pUpdateData[pDesc->handleIndex + arr].imageInfo = { pRenderer->pNullDescriptors->pDefaultSampler->pVkSampler, VK_NULL_HANDLE };
							break;
						}
						case SG_DESCRIPTOR_TYPE_TEXTURE:
						{
							for (uint32_t arr = 0; arr < arrayCount; ++arr)
								pUpdateData[pDesc->handleIndex + arr].imageInfo = { VK_NULL_HANDLE, pRenderer->pNullDescriptors->pDefaultTextureSRV[nodeIndex][pDesc->dim]->pVkSRVDescriptor, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
							break;
						}
						case SG_DESCRIPTOR_TYPE_RW_TEXTURE:
						{
							for (uint32_t arr = 0; arr < arrayCount; ++arr)
								pUpdateData[pDesc->handleIndex + arr].imageInfo = { VK_NULL_HANDLE, pRenderer->pNullDescriptors->pDefaultTextureUAV[nodeIndex][pDesc->dim]->pVkUAVDescriptors[0], VK_IMAGE_LAYOUT_GENERAL };
							break;
						}
						case SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
						case SG_DESCRIPTOR_TYPE_BUFFER:
						case SG_DESCRIPTOR_TYPE_BUFFER_RAW:
						{
							for (uint32_t arr = 0; arr < arrayCount; ++arr)
								pUpdateData[pDesc->handleIndex + arr].bufferInfo =
							{
								pRenderer->pNullDescriptors->pDefaultBufferSRV[nodeIndex]->pVkBuffer,
								pRenderer->pNullDescriptors->pDefaultBufferSRV[nodeIndex]->offset,
								VK_WHOLE_SIZE
							};
							break;
						}
						case SG_DESCRIPTOR_TYPE_RW_BUFFER:
						case SG_DESCRIPTOR_TYPE_RW_BUFFER_RAW:
						{
							for (uint32_t arr = 0; arr < arrayCount; ++arr)
								pUpdateData[pDesc->handleIndex + arr].bufferInfo =
							{
								pRenderer->pNullDescriptors->pDefaultBufferUAV[nodeIndex]->pVkBuffer,
								pRenderer->pNullDescriptors->pDefaultBufferUAV[nodeIndex]->offset,
								VK_WHOLE_SIZE
							};
							break;
						}
						case SG_DESCRIPTOR_TYPE_TEXEL_BUFFER:
						{
							for (uint32_t arr = 0; arr < arrayCount; ++arr)
								pUpdateData[pDesc->handleIndex + arr].bufferView = pRenderer->pNullDescriptors->pDefaultBufferSRV[nodeIndex]->pVkUniformTexelView;
							break;
						}
						case SG_DESCRIPTOR_TYPE_RW_TEXEL_BUFFER:
						{
							for (uint32_t arr = 0; arr < arrayCount; ++arr)
								pUpdateData[pDesc->handleIndex + arr].bufferView = pRenderer->pNullDescriptors->pDefaultBufferUAV[nodeIndex]->pVkStorageTexelView;
							break;
						}
						default:
							break;
						}
					}
					++entryCount;
				}

				VkDescriptorUpdateTemplateCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO, nullptr };
				createInfo.descriptorSetLayout = pRootSignature->vkDescriptorSetLayouts[setIndex];
				createInfo.descriptorUpdateEntryCount = entryCount;
				createInfo.pDescriptorUpdateEntries = pEntries;
				createInfo.pipelineBindPoint = gPipelineBindPoint[pRootSignature->pipelineType];
				createInfo.pipelineLayout = pRootSignature->pPipelineLayout;
				createInfo.set = setIndex;
				createInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR;
				SG_CHECK_VKRESULT(vkCreateDescriptorUpdateTemplate(pRenderer->pVkDevice, &createInfo, nullptr, &pRootSignature->updateTemplates[setIndex]));

				sg_free(pEntries);
			}
			else if (VK_NULL_HANDLE != pRootSignature->vkDescriptorSetLayouts[setIndex])
			{
				// consume empty descriptor sets from empty descriptor set pool
				VkDescriptorSet* pSets[] = { &pRootSignature->vkEmptyDescriptorSets[setIndex] };
				allocate_descriptor_sets(pRenderer->pDescriptorPool, &pRootSignature->vkDescriptorSetLayouts[setIndex], pSets, 1);
			}
		}

		*ppRootSignature = pRootSignature;
	}

	void remove_root_signature(Renderer* pRenderer, RootSignature* pRootSignature)
	{
		for (uint32_t i = 0; i < SG_DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
		{
			vkDestroyDescriptorSetLayout(pRenderer->pVkDevice, pRootSignature->vkDescriptorSetLayouts[i], nullptr);
			if (VK_NULL_HANDLE != pRootSignature->updateTemplates[i])
				vkDestroyDescriptorUpdateTemplate(pRenderer->pVkDevice, pRootSignature->updateTemplates[i], nullptr);

			if (pRootSignature->vkDescriptorCounts[i])
				for (uint32_t nodeIndex = 0; nodeIndex < pRenderer->linkedNodeCount; ++nodeIndex)
					SG_SAFE_FREE(pRootSignature->pUpdateTemplateData[i][nodeIndex]);

			SG_SAFE_FREE(pRootSignature->pUpdateTemplateData[i]);
		}

		// need to delete since the destructor frees allocated memory
		pRootSignature->descriptorNameToIndexMap->mMap.clear(true);

		vkDestroyPipelineLayout(pRenderer->pVkDevice, pRootSignature->pPipelineLayout, nullptr);
		SG_SAFE_FREE(pRootSignature);
	}

#pragma endregion (Root Signature)

#pragma region (Pipeline)

	static void add_graphic_pipeline(Renderer* pRenderer, const PipelineCreateDesc* pMainDesc, Pipeline** ppPipeline)
	{
		ASSERT(pRenderer);
		ASSERT(ppPipeline);
		ASSERT(pMainDesc);

		const GraphicsPipelineDesc* pDesc = &pMainDesc->graphicsDesc;
		VkPipelineCache psoCache = pMainDesc->pCache ? pMainDesc->pCache->cache : VK_NULL_HANDLE;

		ASSERT(pDesc->pShaderProgram);
		ASSERT(pDesc->pRootSignature);

		auto* pPipeline = (Pipeline*)sg_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
		ASSERT(pPipeline);

		const Shader* pShaderProgram = pDesc->pShaderProgram;
		const VertexLayout* pVertexLayout = pDesc->pVertexLayout;

		pPipeline->type = SG_PIPELINE_TYPE_GRAPHICS;

		// Create temporary renderpass for pipeline creation
		SG_DECLARE_ZERO(RenderPassCreateDesc, renderPassDesc);
		RenderPass* pRenderPass = nullptr;
		renderPassDesc.renderTargetCount = pDesc->renderTargetCount;
		renderPassDesc.pColorFormats = pDesc->pColorFormats;
		renderPassDesc.sampleCount = pDesc->sampleCount;
		renderPassDesc.depthStencilFormat = pDesc->depthStencilFormat;
		add_render_pass(pRenderer, &renderPassDesc, &pRenderPass);

		ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
		for (uint32_t i = 0; i < pShaderProgram->pReflection->stageReflectionCount; ++i)
			ASSERT(VK_NULL_HANDLE != pShaderProgram->pShaderModules[i]);

		// Pipeline
		{
			uint32_t stageCount = 0;
			SG_DECLARE_ZERO(VkPipelineShaderStageCreateInfo, stages[5]);
			for (uint32_t i = 0; i < 5; ++i)
			{
				auto stageMask = (ShaderStage)(1 << i);
				if (stageMask == (pShaderProgram->stages & stageMask))
				{
					stages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
					stages[stageCount].pNext = nullptr;
					stages[stageCount].flags = 0;
					stages[stageCount].pSpecializationInfo = nullptr;
					switch (stageMask)
					{
					case SG_SHADER_STAGE_VERT:
					{
						stages[stageCount].pName =
							pShaderProgram->pReflection->stageReflections[pShaderProgram->pReflection->vertexStageIndex].pEntryPoint;
						stages[stageCount].stage = VK_SHADER_STAGE_VERTEX_BIT;
						stages[stageCount].module = pShaderProgram->pShaderModules[pShaderProgram->pReflection->vertexStageIndex];
					}
					break;
					case SG_SHADER_STAGE_TESC:
					{
						stages[stageCount].pName =
							pShaderProgram->pReflection->stageReflections[pShaderProgram->pReflection->hullStageIndex].pEntryPoint;
						stages[stageCount].stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
						stages[stageCount].module = pShaderProgram->pShaderModules[pShaderProgram->pReflection->hullStageIndex];
					}
					break;
					case SG_SHADER_STAGE_TESE:
					{
						stages[stageCount].pName =
							pShaderProgram->pReflection->stageReflections[pShaderProgram->pReflection->domainStageIndex].pEntryPoint;
						stages[stageCount].stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
						stages[stageCount].module = pShaderProgram->pShaderModules[pShaderProgram->pReflection->domainStageIndex];
					}
					break;
					case SG_SHADER_STAGE_GEOM:
					{
						stages[stageCount].pName =
							pShaderProgram->pReflection->stageReflections[pShaderProgram->pReflection->geometryStageIndex].pEntryPoint;
						stages[stageCount].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
						stages[stageCount].module = pShaderProgram->pShaderModules[pShaderProgram->pReflection->geometryStageIndex];
					}
					break;
					case SG_SHADER_STAGE_FRAG:
					{
						stages[stageCount].pName =
							pShaderProgram->pReflection->stageReflections[pShaderProgram->pReflection->pixelStageIndex].pEntryPoint;
						stages[stageCount].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
						stages[stageCount].module = pShaderProgram->pShaderModules[pShaderProgram->pReflection->pixelStageIndex];
					}
					break;
					default: ASSERT(false && "Shader Stage not supported!"); break;
					}
					++stageCount;
				}
			}

			// make sure there's a shader
			ASSERT(0 != stageCount);

			uint32_t                          inputBindingCount = 0;
			VkVertexInputBindingDescription   inputBindings[SG_MAX_VERTEX_BINDINGS] = { { 0 } };
			uint32_t                          inputAttributeCount = 0;
			VkVertexInputAttributeDescription inputAttributes[SG_MAX_VERTEX_ATTRIBS] = { { 0 } };

			// make sure there's attributes
			if (pVertexLayout != nullptr)
			{
				// Ignore everything that's beyond max_vertex_attribs
				uint32_t attribCount = pVertexLayout->attribCount > SG_MAX_VERTEX_ATTRIBS ? SG_MAX_VERTEX_ATTRIBS : pVertexLayout->attribCount;
				uint32_t bindingValue = UINT32_MAX;

				// Initial values
				for (uint32_t i = 0; i < attribCount; ++i)
				{
					const VertexAttrib* attrib = &(pVertexLayout->attribs[i]);

					if (bindingValue != attrib->binding)
					{
						bindingValue = attrib->binding;
						++inputBindingCount;
					}

					inputBindings[inputBindingCount - 1].binding = bindingValue;
					if (attrib->rate == SG_VERTEX_ATTRIB_RATE_INSTANCE)
					{
						inputBindings[inputBindingCount - 1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
					}
					else
					{
						inputBindings[inputBindingCount - 1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
					}
					inputBindings[inputBindingCount - 1].stride += TinyImageFormat_BitSizeOfBlock(attrib->format) / 8;

					inputAttributes[inputAttributeCount].location = attrib->location;
					inputAttributes[inputAttributeCount].binding = attrib->binding;
					inputAttributes[inputAttributeCount].format = (VkFormat)TinyImageFormat_ToVkFormat(attrib->format);
					inputAttributes[inputAttributeCount].offset = attrib->offset;
					++inputAttributeCount;
				}
			}

			SG_DECLARE_ZERO(VkPipelineVertexInputStateCreateInfo, vi);
			vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vi.pNext = nullptr;
			vi.flags = 0;
			vi.vertexBindingDescriptionCount = inputBindingCount;
			vi.pVertexBindingDescriptions = inputBindings;
			vi.vertexAttributeDescriptionCount = inputAttributeCount;
			vi.pVertexAttributeDescriptions = inputAttributes;

			VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			switch (pDesc->primitiveTopo)
			{
			case SG_PRIMITIVE_TOPO_POINT_LIST: topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
			case SG_PRIMITIVE_TOPO_LINE_LIST:  topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
			case SG_PRIMITIVE_TOPO_LINE_STRIP: topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
			case SG_PRIMITIVE_TOPO_TRI_STRIP:  topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
			case SG_PRIMITIVE_TOPO_PATCH_LIST: topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; break;
			case SG_PRIMITIVE_TOPO_TRI_LIST:   topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
			default: ASSERT(false && "Primitive Topo not supported!"); break;
			}

			SG_DECLARE_ZERO(VkPipelineInputAssemblyStateCreateInfo, ia);
			ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			ia.pNext = nullptr;
			ia.flags = 0;
			ia.topology = topology;
			ia.primitiveRestartEnable = VK_FALSE;

			SG_DECLARE_ZERO(VkPipelineTessellationStateCreateInfo, ts);
			if ((pShaderProgram->stages & SG_SHADER_STAGE_TESC) && (pShaderProgram->stages & SG_SHADER_STAGE_TESE))
			{
				ts.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
				ts.pNext = nullptr;
				ts.flags = 0;
				ts.patchControlPoints = pShaderProgram->pReflection->stageReflections[pShaderProgram->pReflection->hullStageIndex].numControlPoint;
			}

			SG_DECLARE_ZERO(VkPipelineViewportStateCreateInfo, vs);
			vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			vs.pNext = nullptr;
			vs.flags = 0;
			// we are using dynamic viewports but we must set the count to 1
			vs.viewportCount = 1;
			vs.pViewports = nullptr;
			vs.scissorCount = 1;
			vs.pScissors = nullptr;

			SG_DECLARE_ZERO(VkPipelineMultisampleStateCreateInfo, ms);
			ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			ms.pNext = nullptr;
			ms.flags = 0;
			ms.rasterizationSamples = util_to_vk_sample_count(pDesc->sampleCount);
			ms.sampleShadingEnable = VK_FALSE;
			ms.minSampleShading = 0.0f;
			ms.pSampleMask = 0;
			ms.alphaToCoverageEnable = VK_FALSE;
			ms.alphaToOneEnable = VK_FALSE;

			SG_DECLARE_ZERO(VkPipelineRasterizationStateCreateInfo, rs);
			rs = pDesc->pRasterizerState ? util_to_rasterizer_desc(pDesc->pRasterizerState) : gDefaultRasterizerDesc;

			/// TODO: don't create depth state if no depth stencil bound
			SG_DECLARE_ZERO(VkPipelineDepthStencilStateCreateInfo, ds);
			ds = pDesc->pDepthState ? util_to_depth_desc(pDesc->pDepthState) : gDefaultDepthDesc;

			SG_DECLARE_ZERO(VkPipelineColorBlendStateCreateInfo, cb);
			SG_DECLARE_ZERO(VkPipelineColorBlendAttachmentState, cbAtt[SG_MAX_RENDER_TARGET_ATTACHMENTS]);
			auto test = util_to_blend_desc(pDesc->pBlendState, cbAtt);
			cb = pDesc->pBlendState ? util_to_blend_desc(pDesc->pBlendState, cbAtt) : gDefaultBlendDesc;
			cb.attachmentCount = pDesc->renderTargetCount;

			SG_DECLARE_ZERO(VkDynamicState, dynamicStates[6]);
			dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
			dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;
			dynamicStates[2] = VK_DYNAMIC_STATE_DEPTH_BIAS;
			dynamicStates[3] = VK_DYNAMIC_STATE_BLEND_CONSTANTS;
			dynamicStates[4] = VK_DYNAMIC_STATE_DEPTH_BOUNDS;
			dynamicStates[5] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;

			SG_DECLARE_ZERO(VkPipelineDynamicStateCreateInfo, dy);
			dy.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dy.pNext = nullptr;
			dy.flags = 0;
			dy.dynamicStateCount = 6;
			dy.pDynamicStates = dynamicStates;

			SG_DECLARE_ZERO(VkGraphicsPipelineCreateInfo, createInfo);
			createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			createInfo.pNext = nullptr;
			createInfo.flags = 0;
			createInfo.stageCount = stageCount;
			createInfo.pStages = stages;
			createInfo.pVertexInputState = &vi;
			createInfo.pInputAssemblyState = &ia;

			if ((pShaderProgram->stages & SG_SHADER_STAGE_TESC) && (pShaderProgram->stages & SG_SHADER_STAGE_TESE))
				createInfo.pTessellationState = &ts;
			else
				createInfo.pTessellationState = nullptr; // set tessellation state to null if we have no tessellation

			createInfo.pViewportState = &vs;
			createInfo.pRasterizationState = &rs;
			createInfo.pMultisampleState = &ms;
			createInfo.pDepthStencilState = &ds;
			createInfo.pColorBlendState = &cb;
			createInfo.pDynamicState = &dy;
			createInfo.layout = pDesc->pRootSignature->pPipelineLayout;
			createInfo.renderPass = pRenderPass->pRenderPass;
			createInfo.subpass = 0;
			createInfo.basePipelineHandle = VK_NULL_HANDLE;
			createInfo.basePipelineIndex = -1;
			SG_CHECK_VKRESULT(vkCreateGraphicsPipelines(pRenderer->pVkDevice, psoCache, 1, &createInfo, nullptr, &(pPipeline->pVkPipeline)));

			remove_render_pass(pRenderer, pRenderPass);
		}
		*ppPipeline = pPipeline;
	}

	static void add_compute_pipeline(Renderer* pRenderer, const PipelineCreateDesc* pMainDesc, Pipeline** ppPipeline)
	{
		ASSERT(pRenderer);
		ASSERT(ppPipeline);
		ASSERT(pMainDesc);

		const ComputePipelineDesc* pDesc = &pMainDesc->computeDesc;
		VkPipelineCache psoCache = pMainDesc->pCache ? pMainDesc->pCache->cache : VK_NULL_HANDLE;

		ASSERT(pDesc->pShaderProgram);
		ASSERT(pDesc->pRootSignature);
		ASSERT(pRenderer->pVkDevice != VK_NULL_HANDLE);
		ASSERT(pDesc->pShaderProgram->pShaderModules[0] != VK_NULL_HANDLE);

		Pipeline* pPipeline = (Pipeline*)sg_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
		ASSERT(pPipeline);
		pPipeline->type = SG_PIPELINE_TYPE_COMPUTE;

		{
			SG_DECLARE_ZERO(VkPipelineShaderStageCreateInfo, stage);
			stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stage.pNext = nullptr;
			stage.flags = 0;
			stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
			stage.module = pDesc->pShaderProgram->pShaderModules[0];
			stage.pName = pDesc->pShaderProgram->pReflection->stageReflections[0].pEntryPoint;
			stage.pSpecializationInfo = nullptr;

			SG_DECLARE_ZERO(VkComputePipelineCreateInfo, createInfo);
			createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			createInfo.pNext = nullptr;
			createInfo.flags = 0;
			createInfo.stage = stage;
			createInfo.layout = pDesc->pRootSignature->pPipelineLayout;
			createInfo.basePipelineHandle = 0;
			createInfo.basePipelineIndex = 0;
			SG_CHECK_VKRESULT(vkCreateComputePipelines(pRenderer->pVkDevice, psoCache, 1, &createInfo, nullptr, &(pPipeline->pVkPipeline)));
		}

		*ppPipeline = pPipeline;
	}

	void add_pipeline(Renderer* pRenderer, const PipelineCreateDesc* pDesc, Pipeline** ppPipeline)
	{
		switch (pDesc->type)
		{
		case(SG_PIPELINE_TYPE_COMPUTE):
		{
			add_compute_pipeline(pRenderer, pDesc, ppPipeline);
			break;
		}
		case(SG_PIPELINE_TYPE_GRAPHICS):
		{
			add_graphic_pipeline(pRenderer, pDesc, ppPipeline);
			break;
		}
#ifdef SG_ENABLE_RAYTRACING
		case(SG_PIPELINE_TYPE_RAYTRACING):
		{
			add_raytracing_pipeline(pDesc, ppPipeline);
			break;
		}
#endif
		default:
		{
			ASSERT(false);
			*ppPipeline = {};
			break;
		}
		}

#ifdef SG_ENABLE_GRAPHICS_DEBUG
		if (*ppPipeline && pDesc->name)
		{
			set_pipeline_name(pRenderer, *ppPipeline, pDesc->name);
		}
#endif
	}

	void remove_pipeline(Renderer* pRenderer, Pipeline* pPipeline)
	{
		ASSERT(pRenderer);
		ASSERT(pPipeline);
		ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
		ASSERT(VK_NULL_HANDLE != pPipeline->pVkPipeline);

#ifdef SG_ENABLE_RAYTRACING
		SG_SAFE_FREE(pPipeline->ppShaderStageNames);
#endif

		vkDestroyPipeline(pRenderer->pVkDevice, pPipeline->pVkPipeline, nullptr);
		SG_SAFE_FREE(pPipeline);
	}

	void add_pipeline_cache(Renderer* pRenderer, const PipelineCacheDesc* pDesc, PipelineCache** ppPipelineCache)
	{
		ASSERT(pRenderer);
		ASSERT(pDesc);
		ASSERT(ppPipelineCache);

		auto* pPipelineCache = (PipelineCache*)sg_calloc(1, sizeof(PipelineCache));
		ASSERT(pPipelineCache);

		VkPipelineCacheCreateInfo psoCacheCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, nullptr };
		psoCacheCreateInfo.initialDataSize = pDesc->size;
		psoCacheCreateInfo.pInitialData = pDesc->pData;
		psoCacheCreateInfo.flags = util_to_pipeline_cache_flags(pDesc->flags);
		SG_CHECK_VKRESULT(vkCreatePipelineCache(pRenderer->pVkDevice, &psoCacheCreateInfo, nullptr, &pPipelineCache->cache));

		*ppPipelineCache = pPipelineCache;
	}

	void remove_pipeline_cache(Renderer* pRenderer, PipelineCache* pPipelineCache)
	{
		ASSERT(pRenderer);
		ASSERT(pPipelineCache);

		if (pPipelineCache->cache)
		{
			vkDestroyPipelineCache(pRenderer->pVkDevice, pPipelineCache->cache, nullptr);
		}
		SG_SAFE_FREE(pPipelineCache);
	}

	void get_pipeline_cache_data(Renderer* pRenderer, PipelineCache* pPipelineCache, size_t* pSize, void* pData)
	{
		ASSERT(pRenderer);
		ASSERT(pPipelineCache);
		ASSERT(pSize);

		if (pPipelineCache->cache)
		{
			SG_CHECK_VKRESULT(vkGetPipelineCacheData(pRenderer->pVkDevice, pPipelineCache->cache, pSize, pData));
		}
	}

#pragma endregion (Pipeline)

#pragma region (Queue)

	void add_queue(Renderer* pRenderer, QueueCreateDesc* pDesc, Queue** ppQueue)
	{
		ASSERT(pDesc != nullptr);

		const uint32_t nodeIndex = pDesc->nodeIndex;
		VkQueueFamilyProperties queueProps = {};
		uint8_t queueFamilyIndex = UINT8_MAX;
		uint8_t queueIndex = UINT8_MAX;

		util_find_queue_family_index(pRenderer, nodeIndex, pDesc->type, &queueProps, &queueFamilyIndex, &queueIndex);
		++pRenderer->pUsedQueueCount[nodeIndex][queueProps.queueFlags];

		auto* pQueue = (Queue*)sg_calloc(1, sizeof(Queue));
		ASSERT(pQueue);

		pQueue->vkQueueFamilyIndex = queueFamilyIndex;
		pQueue->nodeIndex = pDesc->nodeIndex;
		pQueue->type = pDesc->type;
		pQueue->vkQueueIndex = queueIndex;
		pQueue->gpuMode = pRenderer->gpuMode;
		pQueue->timestampPeriod = pRenderer->pVkActiveGPUProperties->properties.limits.timestampPeriod;
		pQueue->flags = queueProps.queueFlags;
		pQueue->pSubmitMutex = &pRenderer->pNullDescriptors->mSubmitMutex;

		// Get queue handle
		vkGetDeviceQueue(pRenderer->pVkDevice, pQueue->vkQueueFamilyIndex, pQueue->vkQueueIndex, &pQueue->pVkQueue);
		ASSERT(VK_NULL_HANDLE != pQueue->pVkQueue);

		*ppQueue = pQueue;
	}

	void remove_queue(Renderer* pRenderer, Queue* pQueue)
	{
		ASSERT(pRenderer);
		ASSERT(pQueue);

		const uint32_t nodeIndex = pQueue->nodeIndex;
		const VkQueueFlags queueFlags = pQueue->flags;
		--pRenderer->pUsedQueueCount[nodeIndex][queueFlags];

		SG_SAFE_FREE(pQueue);
	}

#pragma endregion (Queue)

#pragma region (Renderer)

	void init_renderer(const char* apname, const RendererCreateDesc* pDesc, Renderer** ppRenderer)
	{
		ASSERT(apname);
		ASSERT(pDesc);
		ASSERT(ppRenderer);

		auto* pRenderer = (Renderer*)sg_calloc_memalign(1, alignof(Renderer), sizeof(Renderer));
		ASSERT(pRenderer);

		pRenderer->gpuMode = pDesc->gpuMode;
		pRenderer->shaderTarget = pDesc->shaderTarget;
		pRenderer->enableGpuBasedValidation = pDesc->enableGPUBasedValidation;
		pRenderer->api = SG_RENDERER_API_VULKAN;

		pRenderer->name = (char*)sg_calloc(strlen(apname) + 1, sizeof(char));
		strcpy(pRenderer->name, apname);

		// Initialize the vulkan internal bits
		{
			//AGSReturnCode agsRet = agsInit();
			//if (AGSReturnCode::AGS_SUCCESS == agsRet)
			//{
			//	agsPrintDriverInfo();
			//}

			//// Display NVIDIA driver version using nvapi
			//NvAPI_Status nvStatus = nvapiInit();
			//if (NvAPI_Status::NVAPI_OK == nvStatus)
			//{
			//	nvapiPrintDriverInfo();
			//}

//#if defined(VK_USE_DISPATCH_TABLES)
//			VkResult vkRes = volkInitializeWithDispatchTables(pRenderer);
//			if (vkRes != VK_SUCCESS)
//			{
//				SG_LOG_ERROR("Failed to initialize vulkan");
//				return;
//			}
//#else
			const char** instanceLayers = (const char**)alloca((2 + pDesc->instanceLayerCount) * sizeof(char*));
			uint32_t instanceLayerCount = 0;

#if defined(SG_ENABLE_GRAPHICS_DEBUG)
			// this turns on all validation layers
			instanceLayers[instanceLayerCount++] = "VK_LAYER_KHRONOS_validation";
#endif

			// this turns on render doc layer for gpu capture
#ifdef SG_USE_RENDER_DOC
			instanceLayers[instanceLayerCount++] = "VK_LAYER_RENDERDOC_Capture";
#endif

			// Add user specified instance layers for instance creation
			for (uint32_t i = 0; i < (uint32_t)pDesc->instanceLayerCount; ++i)
				instanceLayers[instanceLayerCount++] = pDesc->ppInstanceLayers[i];

			//#if !defined(NX64)
			//		VkResult vkRes = volkInitialize();
			//		if (vkRes != VK_SUCCESS)
			//		{
			//			LOGF(LogLevel::eERROR, "Failed to initialize Vulkan");
			//			return;
			//		}
			//#endif

			create_instance(apname, pDesc, instanceLayerCount, instanceLayers, pRenderer);
//#endif
		
			// this include the creations of physical device, device and device queue
			if (!add_device(pDesc, pRenderer))
			{
				*ppRenderer = nullptr;
				return;
			}

			// anything below LOW preset is not supported and we will exit
			if (pRenderer->pActiveGpuSettings->gpuVendorPreset.presetLevel < SG_GPU_PRESET_LOW)
			{
				// have the condition in the assert as well so its cleared when the assert message box appears
				ASSERT(pRenderer->pActiveGpuSettings->gpuVendorPreset.presetLevel >= SG_GPU_PRESET_LOW);

				SG_SAFE_FREE(pRenderer->name);

				// remove device and any memory we allocated in just above as this is the first function called
				// when initializing the forge
#if !defined(VK_USE_DISPATCH_TABLES)
				remove_device(pRenderer);
				remove_instance(pRenderer);
				SG_SAFE_FREE(pRenderer);
				SG_LOG_INFO("Selected GPU has an Office Preset in gpu.cfg");
				SG_LOG_INFO("Office preset is not supported by The Seagull Engine");
#endif

				// return NULL pRenderer so that client can gracefully handle exit
				// this is better than exiting from here in case client has allocated memory or has callbacks
				*ppRenderer = nullptr;
				return;
			}

			/// Vma Memory allocator
			SG_DECLARE_ZERO(VmaAllocatorCreateInfo, createInfo);
			createInfo.device = pRenderer->pVkDevice;
			createInfo.physicalDevice = pRenderer->pVkActiveGPU;
			createInfo.instance = pRenderer->pVkInstance;
#ifdef ANDROID
			createInfo.vulkanApiVersion = VK_API_VERSION_1_0;
#else
			createInfo.vulkanApiVersion = VK_API_VERSION_1_2;
#endif

			// Render Doc Capture currently does not support use of this extension
			if (gDedicatedAllocationExtension && !gRenderDocLayerEnabled)
			{
				createInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
			}

			VmaVulkanFunctions vulkanFunctions = {};
			vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
			vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
			vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
			vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
			vulkanFunctions.vkCreateImage = vkCreateImage;
			vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
			vulkanFunctions.vkDestroyImage = vkDestroyImage;
			vulkanFunctions.vkFreeMemory = vkFreeMemory;
			vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
			vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;
			vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
			vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;
			vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
			vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
			vulkanFunctions.vkMapMemory = vkMapMemory;
			vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
			vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
			vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
			vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;

			createInfo.pVulkanFunctions = &vulkanFunctions;
			createInfo.pAllocationCallbacks = nullptr;
			vmaCreateAllocator(&createInfo, &pRenderer->vmaAllocator);

			SG_LOG_INFO("VmaAllocator initialized successfully!");
		}

		VkDescriptorPoolSize descriptorPoolSizes[SG_DESCRIPTOR_TYPE_RANGE_SIZE] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1024 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 8192 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1024 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1024 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8192 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1 },
		};
#ifdef VK_NV_RAY_TRACING_SPEC_VERSION
		if (gNVRayTracingExtension)
		{
			descriptorPoolSizes[SG_DESCRIPTOR_TYPE_RANGE_SIZE - 1] = { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1024 };
		}
#endif

		add_descriptor_pool(pRenderer, 8192, (VkDescriptorPoolCreateFlags)0, descriptorPoolSizes,
			gDescriptorTypeRangeSize, &(pRenderer->pDescriptorPool));

		pRenderPassMutex = (Mutex*)sg_calloc(1, sizeof(Mutex));
		pRenderPassMutex->Init();
		gRenderPassMap = sg_placement_new<eastl::hash_map<ThreadID, RenderPassMap> >(sg_malloc(sizeof(*gRenderPassMap)));
		gFrameBufferMap = sg_placement_new<eastl::hash_map<ThreadID, FrameBufferMap> >(sg_malloc(sizeof(*gFrameBufferMap)));

		VkPhysicalDeviceFeatures2 gpuFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		vkGetPhysicalDeviceFeatures2(pRenderer->pVkActiveGPU, &gpuFeatures);

		// Set shader macro based on runtime information
		static char descriptorIndexingMacroBuffer[2] = {};
		static char textureArrayDynamicIndexingMacroBuffer[2] = {};
		sprintf(descriptorIndexingMacroBuffer, "%u", (uint32_t)(gDescriptorIndexingExtension));
		sprintf(textureArrayDynamicIndexingMacroBuffer, "%u", (uint32_t)(gpuFeatures.features.shaderSampledImageArrayDynamicIndexing));
		static ShaderMacro rendererShaderDefines[] =
		{
			{ "VK_EXT_DESCRIPTOR_INDEXING_ENABLED", descriptorIndexingMacroBuffer },
			{ "VK_FEATURE_TEXTURE_ARRAY_DYNAMIC_INDEXING_ENABLED", textureArrayDynamicIndexingMacroBuffer },
			// Descriptor set indices
			{ "SG_UPDATE_FREQ_NONE",      "set = 0" },
			{ "SG_UPDATE_FREQ_PER_FRAME", "set = 1" },
			{ "SG_UPDATE_FREQ_PER_BATCH", "set = 2" },
			{ "SG_UPDATE_FREQ_PER_DRAW",  "set = 3" },
		};
		pRenderer->builtinShaderDefinesCount = sizeof(rendererShaderDefines) / sizeof(rendererShaderDefines[0]);
		pRenderer->pBuiltinShaderDefines = rendererShaderDefines;

		util_find_queue_family_index(pRenderer, 0, SG_QUEUE_TYPE_GRAPHICS, nullptr, &pRenderer->graphicsQueueFamilyIndex, nullptr);
		util_find_queue_family_index(pRenderer, 0, SG_QUEUE_TYPE_COMPUTE, nullptr, &pRenderer->computeQueueFamilyIndex, nullptr);
		util_find_queue_family_index(pRenderer, 0, SG_QUEUE_TYPE_TRANSFER, nullptr, &pRenderer->transferQueueFamilyIndex, nullptr);

		//SG_LOG_INFO("Graphic queue family index: %d", pRenderer->graphicsQueueFamilyIndex);
		//SG_LOG_INFO("Transfer queue family index: %d", pRenderer->transferQueueFamilyIndex);
		//SG_LOG_INFO("Compute queue family index: %d", pRenderer->computeQueueFamilyIndex);

		add_default_resources(pRenderer);

		// renderer is good!
		*ppRenderer = pRenderer;
	}

	void remove_renderer(Renderer* pRenderer)
	{
		ASSERT(pRenderer);

		remove_default_resources(pRenderer);
		remove_descriptor_pool(pRenderer, pRenderer->pDescriptorPool);

		// Remove the render passes
		for (eastl::hash_map<ThreadID, RenderPassMap>::value_type& t : *gRenderPassMap)
			for (RenderPassMapNode& it : t.second)
				remove_render_pass(pRenderer, it.second);

		for (eastl::hash_map<ThreadID, FrameBufferMap>::value_type& t : *gFrameBufferMap)
			for (FrameBufferMapNode& it : t.second)
				remove_framebuffer(pRenderer, it.second);

		// Destroy the Vulkan bits
		vmaDestroyAllocator(pRenderer->vmaAllocator);

#if defined(VK_USE_DISPATCH_TABLES)
#else
		remove_device(pRenderer);
		remove_instance(pRenderer);
#endif

		//nvapiExit();
		//agsExit();

		pRenderPassMutex->Destroy();
		gRenderPassMap->clear(true);
		gFrameBufferMap->clear(true);

		SG_SAFE_FREE(pRenderPassMutex);
		SG_SAFE_FREE(gRenderPassMap);
		SG_SAFE_FREE(gFrameBufferMap);

		for (uint32_t i = 0; i < pRenderer->linkedNodeCount; ++i)
		{
			SG_SAFE_FREE(pRenderer->pAvailableQueueCount[i]);
			SG_SAFE_FREE(pRenderer->pUsedQueueCount[i]);
		}

		// free all the renderer components!
		SG_SAFE_FREE(pRenderer->pAvailableQueueCount);
		SG_SAFE_FREE(pRenderer->pUsedQueueCount);
		SG_SAFE_FREE(pRenderer->pCapBits);
		SG_SAFE_FREE(pRenderer->name);
		SG_SAFE_FREE(pRenderer);
	}

#pragma endregion (Renderer)

#pragma region (GPU Query)

	VkQueryType util_to_vk_query_type(QueryType type)
	{
		switch (type)
		{
			case SG_QUERY_TYPE_TIMESTAMP:			return VK_QUERY_TYPE_TIMESTAMP;
			case SG_QUERY_TYPE_PIPELINE_STATISTICS: return VK_QUERY_TYPE_PIPELINE_STATISTICS;
			case SG_QUERY_TYPE_OCCLUSION:			return VK_QUERY_TYPE_OCCLUSION;
			default: ASSERT(false && "Invalid query heap type"); return VK_QUERY_TYPE_MAX_ENUM;
		}
	}

	void get_timestamp_frequency(Queue* pQueue, double* pFrequency)
	{
		ASSERT(pQueue);
		ASSERT(pFrequency);

		// The framework is using ticks per sec as frequency. Vulkan is nano sec per tick.
		// Handle the conversion logic here.
		*pFrequency = 1.0f / ((double)pQueue->timestampPeriod /*ns/tick number of nanoseconds required for a timestamp query to be incremented by 1*/
			* 1e-9);             // convert to ticks/sec (DX12 standard)
	}

	void add_query_pool(Renderer* pRenderer, const QueryPoolCreateDesc* pDesc, QueryPool** ppQueryPool)
	{
		ASSERT(pRenderer);
		ASSERT(pDesc);
		ASSERT(ppQueryPool);

		auto* pQueryPool = (QueryPool*)sg_calloc(1, sizeof(QueryPool));
		ASSERT(ppQueryPool);

		pQueryPool->type = util_to_vk_query_type(pDesc->type);
		pQueryPool->count = pDesc->queryCount;

		VkQueryPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, nullptr };
		createInfo.queryCount = pDesc->queryCount;
		createInfo.queryType = util_to_vk_query_type(pDesc->type);

		createInfo.flags = 0;
		createInfo.pipelineStatistics = 0;
		SG_CHECK_VKRESULT(vkCreateQueryPool(pRenderer->pVkDevice, &createInfo, nullptr, &pQueryPool->pVkQueryPool));

		*ppQueryPool = pQueryPool;
	}

	void remove_query_pool(Renderer* pRenderer, QueryPool* pQueryPool)
	{
		ASSERT(pRenderer);
		ASSERT(pQueryPool);
		vkDestroyQueryPool(pRenderer->pVkDevice, pQueryPool->pVkQueryPool, nullptr);

		SG_SAFE_FREE(pQueryPool);
	}

	void cmd_reset_query_pool(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
	{
		vkCmdResetQueryPool(pCmd->pVkCmdBuf, pQueryPool->pVkQueryPool, startQuery, queryCount);
	}

	void cmd_begin_query(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
	{
		VkQueryType type = pQueryPool->type;
		switch (type)
		{
		case VK_QUERY_TYPE_TIMESTAMP:
			vkCmdWriteTimestamp(pCmd->pVkCmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pQueryPool->pVkQueryPool, pQuery->index);
			break;
		case VK_QUERY_TYPE_PIPELINE_STATISTICS: break;
		case VK_QUERY_TYPE_OCCLUSION: break;
		default: break;
		}
	}

	void cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
	{
		cmd_begin_query(pCmd, pQueryPool, pQuery);
	}

	void cmd_resolve_query(Cmd* pCmd, QueryPool* pQueryPool, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount)
	{
		VkQueryResultFlags flags = VK_QUERY_RESULT_64_BIT;
#ifdef ANDROID
		flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
#else
		flags |= VK_QUERY_RESULT_WAIT_BIT;
#endif
		vkCmdCopyQueryPoolResults(pCmd->pVkCmdBuf, pQueryPool->pVkQueryPool, startQuery, queryCount, pReadbackBuffer->pVkBuffer, 0, sizeof(uint64_t), flags);
	}

#pragma endregion (GPU Query)

#pragma region (Memory Stats)

	void calculate_memory_stats(Renderer* pRenderer, char** stats)
	{
		vmaBuildStatsString(pRenderer->vmaAllocator, stats, VK_TRUE);
	}

	void free_memory_stats(Renderer* pRenderer, char* stats)
	{
		vmaFreeStatsString(pRenderer->vmaAllocator, stats);
	}

	void calculate_memory_use(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes)
	{
		VmaStats stats;
		pRenderer->vmaAllocator->CalculateStats(&stats);
		*usedBytes = stats.total.usedBytes;
		*totalAllocatedBytes = *usedBytes + stats.total.unusedBytes;
	}

#pragma endregion (Memory Stats)

#pragma region (Debug Marker)

	void cmd_begin_debug_marker(Renderer* pRenderer, Cmd* pCmd, float r, float g, float b, const char* name)
	{
		if (gDebugMarkerSupport)
		{
#ifdef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
			VkDebugUtilsLabelEXT markerInfo = {};
			markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
			markerInfo.color[0] = r;
			markerInfo.color[1] = g;
			markerInfo.color[2] = b;
			markerInfo.color[3] = 1.0f;
			markerInfo.pLabelName = name;

			auto func = (PFN_vkCmdBeginDebugUtilsLabelEXT)
				vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkCmdBeginDebugUtilsLabelEXT");
			if (func != nullptr)
			{
				func(pCmd->pVkCmdBuf, &markerInfo);
			}
			//vkCmdBeginDebugUtilsLabelEXT(pCmd->pVkCmdBuf, &markerInfo);
#elif !defined(NX64) || !defined(SG_USE_RENDER_DOC)
			VkDebugMarkerMarkerInfoEXT markerInfo = {};
			markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
			markerInfo.color[0] = r;
			markerInfo.color[1] = g;
			markerInfo.color[2] = b;
			markerInfo.color[3] = 1.0f;
			markerInfo.pMarkerName = name;

			auto func = (PFN_vkCmdDebugMarkerBeginEXT)
				vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkCmdDebugMarkerBeginEXT");
			if (func != nullptr)
			{
				func(pCmd->pVkCmdBuf, &markerInfo);
			}
			//vkCmdDebugMarkerBeginEXT(pCmd->pVkCmdBuf, &markerInfo);
#endif
		}
	}

	void cmd_end_debug_marker(Renderer* pRenderer, Cmd* pCmd)
	{
		if (gDebugMarkerSupport)
		{
#ifdef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
			auto func = (PFN_vkCmdEndDebugUtilsLabelEXT)
				vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkCmdEndDebugUtilsLabelEXT");
			if (func != nullptr)
			{
				func(pCmd->pVkCmdBuf);
			}
			//vkCmdEndDebugUtilsLabelEXT(pCmd->pVkCmdBuf);
#elif !defined(NX64) || !defined(SG_USE_RENDER_DOC)
			auto func = (PFN_vkCmdDebugMarkerEndEXT)
				vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkCmdDebugMarkerEndEXT");
			if (func != nullptr)
			{
				func(pCmd->pVkCmdBuf);
			}
			//vkCmdDebugMarkerEndEXT(pCmd->pVkCmdBuf);
#endif
		}
	}

	void cmd_add_debug_marker(Renderer* pRenderer, Cmd* pCmd, float r, float g, float b, const char* name)
	{
		if (gDebugMarkerSupport)
		{
#ifdef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
			VkDebugUtilsLabelEXT markerInfo = {};
			markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
			markerInfo.color[0] = r;
			markerInfo.color[1] = g;
			markerInfo.color[2] = b;
			markerInfo.color[3] = 1.0f;
			markerInfo.pLabelName = name;

			auto func = (PFN_vkCmdInsertDebugUtilsLabelEXT)
				vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkCmdInsertDebugUtilsLabelEXT");
			if (func != nullptr)
			{
				func(pCmd->pVkCmdBuf, &markerInfo);
			}
			//vkCmdInsertDebugUtilsLabelEXT(pCmd->pVkCmdBuf, &markerInfo);
#else
			VkDebugMarkerMarkerInfoEXT markerInfo = {};
			markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
			markerInfo.color[0] = r;
			markerInfo.color[1] = g;
			markerInfo.color[2] = b;
			markerInfo.color[3] = 1.0f;
			markerInfo.pMarkerName = name;

			auto func = (PFN_vkCmdDebugMarkerInsertEXT)
				vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkCmdDebugMarkerInsertEXT");
			if (func != nullptr)
			{
				func(pCmd->pVkCmdBuf, &markerInfo);
			}
			//vkCmdDebugMarkerInsertEXT(pCmd->pVkCmdBuf, &markerInfo);
#endif
		}

#if defined(USE_NSIGHT_AFTERMATH)
		if (pCmd->pRenderer->mAftermathSupport)
		{
			vkCmdSetCheckpointNV(pCmd->pVkCmdBuf, name);
		}
#endif
	}

#pragma endregion (Debug Marker)

} // namespace SG

#endif // SG_RENDERER_IMPLEMENTATION