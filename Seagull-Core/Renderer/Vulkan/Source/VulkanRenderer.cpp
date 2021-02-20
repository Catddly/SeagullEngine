#define VMA_IMPLEMENTATION
#include "Core/CompilerConfig.h"

#include "Interface/ILog.h"
#include "Interface/IThread.h"

//#include <include/tinyimageformat_base.h>
//#include <include/tinyimageformat_apis.h>
//#include <include/tinyimageformat_query.h>

//#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_win32.h>

#include "IRenderer.h"

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

// GPUConfig.h still have some issues on <regex>
#include "Core/GPUConfig.h"
#include "VulkanCapsBuilder.h"
//#include "HelperFunc.h"

#include "Interface/IMemory.h"

#define SG_ENABLE_GRAPHICS_DEBUG

#define SG_SAFE_FREE(p_var)      \
	if (p_var)                   \
	{                            \
		sg_free((void*)p_var);   \
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

#define SG_RENDERER_IMPLEMENTATION

namespace SG
{

#pragma region (Predefined Global Variable)

	//VkBlendOp gVkBlendOpTranslator[BlendMode::SG_MAX_BLEND_MODES] =
	//{
	//	VK_BLEND_OP_ADD,
	//	VK_BLEND_OP_SUBTRACT,
	//	VK_BLEND_OP_REVERSE_SUBTRACT,
	//	VK_BLEND_OP_MIN,
	//	VK_BLEND_OP_MAX,
	//};

	//VkBlendFactor gVkBlendConstantTranslator[BlendConstant::SG_MAX_BLEND_CONSTANTS] =
	//{
	//	VK_BLEND_FACTOR_ZERO,
	//	VK_BLEND_FACTOR_ONE,
	//	VK_BLEND_FACTOR_SRC_COLOR,
	//	VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
	//	VK_BLEND_FACTOR_DST_COLOR,
	//	VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
	//	VK_BLEND_FACTOR_SRC_ALPHA,
	//	VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	//	VK_BLEND_FACTOR_DST_ALPHA,
	//	VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
	//	VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
	//	VK_BLEND_FACTOR_CONSTANT_COLOR,
	//	VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
	//};

	//VkCompareOp gVkComparisonFuncTranslator[CompareMode::SG_MAX_COMPARE_MODES] =
	//{
	//	VK_COMPARE_OP_NEVER,
	//	VK_COMPARE_OP_LESS,
	//	VK_COMPARE_OP_EQUAL,
	//	VK_COMPARE_OP_LESS_OR_EQUAL,
	//	VK_COMPARE_OP_GREATER,
	//	VK_COMPARE_OP_NOT_EQUAL,
	//	VK_COMPARE_OP_GREATER_OR_EQUAL,
	//	VK_COMPARE_OP_ALWAYS,
	//};

	//VkStencilOp gVkStencilOpTranslator[StencilOp::SG_MAX_STENCIL_OPS] =
	//{
	//	VK_STENCIL_OP_KEEP,
	//	VK_STENCIL_OP_ZERO,
	//	VK_STENCIL_OP_REPLACE,
	//	VK_STENCIL_OP_INVERT,
	//	VK_STENCIL_OP_INCREMENT_AND_WRAP,
	//	VK_STENCIL_OP_DECREMENT_AND_WRAP,
	//	VK_STENCIL_OP_INCREMENT_AND_CLAMP,
	//	VK_STENCIL_OP_DECREMENT_AND_CLAMP,
	//};

	//VkCullModeFlagBits gVkCullModeTranslator[CullMode::SG_MAX_CULL_MODES] =
	//{
	//	VK_CULL_MODE_NONE,
	//	VK_CULL_MODE_BACK_BIT,
	//	VK_CULL_MODE_FRONT_BIT
	//};

	//VkPolygonMode gVkFillModeTranslator[FillMode::SG_MAX_FILL_MODES] =
	//{
	//	VK_POLYGON_MODE_FILL,
	//	VK_POLYGON_MODE_LINE
	//};

	//VkFrontFace gVkFrontFaceTranslator[] =
	//{
	//	VK_FRONT_FACE_COUNTER_CLOCKWISE,
	//	VK_FRONT_FACE_CLOCKWISE
	//};

	//VkAttachmentLoadOp gVkAttachmentLoadOpTranslator[LoadActionType::SG_MAX_LOAD_ACTION] =
	//{
	//	VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	//	VK_ATTACHMENT_LOAD_OP_LOAD,
	//	VK_ATTACHMENT_LOAD_OP_CLEAR,
	//};

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
//
//	VkFilter util_to_vk_filter(FilterType filter)
//	{
//		switch (filter)
//		{
//		case SG_FILTER_NEAREST: return VK_FILTER_NEAREST;
//		case SG_FILTER_LINEAR: return VK_FILTER_LINEAR;
//		default: return VK_FILTER_LINEAR;
//		}
//	}
//
//	VkSamplerMipmapMode util_to_vk_mip_map_mode(MipMapMode mipMapMode)
//	{
//		switch (mipMapMode)
//		{
//		case SG_MIPMAP_MODE_NEAREST: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
//		case SG_MIPMAP_MODE_LINEAR: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
//		default: ASSERT(false && "Invalid Mip Map Mode"); return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
//		}
//	}
//
//	VkSamplerAddressMode util_to_vk_address_mode(AddressMode addressMode)
//	{
//		switch (addressMode)
//		{
//		case SG_ADDRESS_MODE_MIRROR: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
//		case SG_ADDRESS_MODE_REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
//		case SG_ADDRESS_MODE_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
//		case SG_ADDRESS_MODE_CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
//		default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
//		}
//	}
//
//	VkShaderStageFlags util_to_vk_shader_stages(ShaderStage shader_stages)
//	{
//		VkShaderStageFlags result = 0;
//		if (SG_SHADER_STAGE_ALL_GRAPHICS == (shader_stages & SG_SHADER_STAGE_ALL_GRAPHICS))
//		{
//			result = VK_SHADER_STAGE_ALL_GRAPHICS;
//		}
//		else
//		{
//			if (SG_SHADER_STAGE_VERT == (shader_stages & SG_SHADER_STAGE_VERT))
//			{
//				result |= VK_SHADER_STAGE_VERTEX_BIT;
//			}
//			if (SG_SHADER_STAGE_TESC == (shader_stages & SG_SHADER_STAGE_TESC))
//			{
//				result |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
//			}
//			if (SG_SHADER_STAGE_TESE == (shader_stages & SG_SHADER_STAGE_TESE))
//			{
//				result |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
//			}
//			if (SG_SHADER_STAGE_GEOM == (shader_stages & SG_SHADER_STAGE_GEOM))
//			{
//				result |= VK_SHADER_STAGE_GEOMETRY_BIT;
//			}
//			if (SG_SHADER_STAGE_FRAG == (shader_stages & SG_SHADER_STAGE_FRAG))
//			{
//				result |= VK_SHADER_STAGE_FRAGMENT_BIT;
//			}
//			if (SG_SHADER_STAGE_COMP == (shader_stages & SG_SHADER_STAGE_COMP))
//			{
//				result |= VK_SHADER_STAGE_COMPUTE_BIT;
//			}
//		}
//		return result;
//	}
//
//	VkSampleCountFlagBits util_to_vk_sample_count(SampleCount sampleCount)
//	{
//		VkSampleCountFlagBits result = VK_SAMPLE_COUNT_1_BIT;
//		switch (sampleCount)
//		{
//		case SG_SAMPLE_COUNT_1: result = VK_SAMPLE_COUNT_1_BIT; break;
//		case SG_SAMPLE_COUNT_2: result = VK_SAMPLE_COUNT_2_BIT; break;
//		case SG_SAMPLE_COUNT_4: result = VK_SAMPLE_COUNT_4_BIT; break;
//		case SG_SAMPLE_COUNT_8: result = VK_SAMPLE_COUNT_8_BIT; break;
//		case SG_SAMPLE_COUNT_16: result = VK_SAMPLE_COUNT_16_BIT; break;
//		}
//		return result;
//	}
//
//	VkBufferUsageFlags util_to_vk_buffer_usage(DescriptorType usage, bool typed)
//	{
//		VkBufferUsageFlags result = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
//		if (usage & SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
//		{
//			result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
//		}
//		if (usage & SG_DESCRIPTOR_TYPE_RW_BUFFER)
//		{
//			result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
//			if (typed)
//				result |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
//		}
//		if (usage & SG_DESCRIPTOR_TYPE_BUFFER)
//		{
//			result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
//			if (typed)
//				result |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
//		}
//		if (usage & SG_DESCRIPTOR_TYPE_INDEX_BUFFER)
//		{
//			result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
//		}
//		if (usage & SG_DESCRIPTOR_TYPE_VERTEX_BUFFER)
//		{
//			result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
//		}
//		if (usage & SG_DESCRIPTOR_TYPE_INDIRECT_BUFFER)
//		{
//			result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
//		}
//#ifdef SG_ENABLE_RAYTRACING
//		if (usage & SG_DESCRIPTOR_TYPE_RAY_TRACING)
//		{
//			result |= VK_BUFFER_USAGE_RAY_TRACING_BIT_NV;
//		}
//#endif
//		return result;
//	}
//
//	VkImageUsageFlags util_to_vk_image_usage(DescriptorType usage)
//	{
//		VkImageUsageFlags result = 0;
//		if (SG_DESCRIPTOR_TYPE_TEXTURE == (usage & SG_DESCRIPTOR_TYPE_TEXTURE))
//			result |= VK_IMAGE_USAGE_SAMPLED_BIT;
//		if (SG_DESCRIPTOR_TYPE_RW_TEXTURE == (usage & SG_DESCRIPTOR_TYPE_RW_TEXTURE))
//			result |= VK_IMAGE_USAGE_STORAGE_BIT;
//		return result;
//	}
//
//	VkAccessFlags util_to_vk_access_flags(ResourceState state)
//	{
//		VkAccessFlags ret = 0;
//		if (state & SG_RESOURCE_STATE_COPY_SOURCE)
//		{
//			ret |= VK_ACCESS_TRANSFER_READ_BIT;
//		}
//		if (state & SG_RESOURCE_STATE_COPY_DEST)
//		{
//			ret |= VK_ACCESS_TRANSFER_WRITE_BIT;
//		}
//		if (state & SG_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
//		{
//			ret |= VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
//		}
//		if (state & SG_RESOURCE_STATE_INDEX_BUFFER)
//		{
//			ret |= VK_ACCESS_INDEX_READ_BIT;
//		}
//		if (state & SG_RESOURCE_STATE_UNORDERED_ACCESS)
//		{
//			ret |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
//		}
//		if (state & SG_RESOURCE_STATE_INDIRECT_ARGUMENT)
//		{
//			ret |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
//		}
//		if (state & SG_RESOURCE_STATE_RENDER_TARGET)
//		{
//			ret |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
//		}
//		if (state & SG_RESOURCE_STATE_DEPTH_WRITE)
//		{
//			ret |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
//		}
//		if (state & SG_RESOURCE_STATE_SHADER_RESOURCE)
//		{
//			ret |= VK_ACCESS_SHADER_READ_BIT;
//		}
//		if (state & SG_RESOURCE_STATE_PRESENT)
//		{
//			ret |= VK_ACCESS_MEMORY_READ_BIT;
//		}
//#ifdef SG_ENABLE_RAYTRACING
//		if (state & SG_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
//		{
//			ret |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV;
//		}
//#endif
//		return ret;
//	}
//
//	VkImageLayout util_to_vk_image_layout(ResourceState usage)
//	{
//		if (usage & SG_RESOURCE_STATE_COPY_SOURCE)
//			return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//
//		if (usage & SG_RESOURCE_STATE_COPY_DEST)
//			return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//
//		if (usage & SG_RESOURCE_STATE_RENDER_TARGET)
//			return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
//
//		if (usage & SG_RESOURCE_STATE_DEPTH_WRITE)
//			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
//
//		if (usage & SG_RESOURCE_STATE_UNORDERED_ACCESS)
//			return VK_IMAGE_LAYOUT_GENERAL;
//
//		if (usage & SG_RESOURCE_STATE_SHADER_RESOURCE)
//			return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//
//		if (usage & SG_RESOURCE_STATE_PRESENT)
//			return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
//
//		if (usage == SG_RESOURCE_STATE_COMMON)
//			return VK_IMAGE_LAYOUT_GENERAL;
//
//		return VK_IMAGE_LAYOUT_UNDEFINED;
//	}
//
//	void util_get_planar_vk_image_memory_requirement(VkDevice device, VkImage image, uint32_t planesCount, VkMemoryRequirements* outVkMemReq, uint64_t* outPlanesOffsets)
//	{
//		outVkMemReq->size = 0;
//		outVkMemReq->alignment = 0;
//		outVkMemReq->memoryTypeBits = 0;
//
//		VkImagePlaneMemoryRequirementsInfo imagePlaneMemReqInfo = { VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO, NULL };
//
//		VkImageMemoryRequirementsInfo2 imagePlaneMemReqInfo2 = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2 };
//		imagePlaneMemReqInfo2.pNext = &imagePlaneMemReqInfo;
//		imagePlaneMemReqInfo2.image = image;
//
//		VkMemoryDedicatedRequirements memDedicatedReq = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, NULL };
//		VkMemoryRequirements2 memReq2 = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
//		memReq2.pNext = &memDedicatedReq;
//
//		for (uint32_t i = 0; i < planesCount; ++i)
//		{
//			imagePlaneMemReqInfo.planeAspect = (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_PLANE_0_BIT << i);
//			vkGetImageMemoryRequirements2(device, &imagePlaneMemReqInfo2, &memReq2);
//
//			outPlanesOffsets[i] += outVkMemReq->size;
//			outVkMemReq->alignment = max(memReq2.memoryRequirements.alignment, outVkMemReq->alignment);
//			outVkMemReq->size += round_up_64(memReq2.memoryRequirements.size, memReq2.memoryRequirements.alignment);
//			outVkMemReq->memoryTypeBits |= memReq2.memoryRequirements.memoryTypeBits;
//		}
//	}
//
//	uint32_t util_get_memory_type(uint32_t typeBits, VkPhysicalDeviceMemoryProperties memoryProperties, VkMemoryPropertyFlags properties, VkBool32* memTypeFound = nullptr)
//	{
//		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
//		{
//			if ((typeBits & 1) == 1)
//			{
//				if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
//				{
//					if (memTypeFound)
//					{
//						*memTypeFound = true;
//					}
//					return i;
//				}
//			}
//			typeBits >>= 1;
//		}
//
//		if (memTypeFound)
//		{
//			*memTypeFound = false;
//			return 0;
//		}
//		else
//		{
//			SG_LOG_ERROR("Could not find a matching memory type");
//			ASSERT(false);
//			return 0;
//		}
//	}
//
//	// Determines pipeline stages involved for given accesses
//	VkPipelineStageFlags util_determine_pipeline_stage_flags(Renderer* pRenderer, VkAccessFlags accessFlags, QueueType queueType)
//	{
//		VkPipelineStageFlags flags = 0;
//		switch (queueType)
//		{
//		case SG_QUEUE_TYPE_GRAPHICS:
//		{
//			if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0)
//				flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
//
//			if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0)
//			{
//				flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
//				flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
//				if (pRenderer->pActiveGpuSettings->geometryShaderSupported)
//				{
//					flags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
//				}
//				if (pRenderer->pActiveGpuSettings->tessellationSupported)
//				{
//					flags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
//					flags |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
//				}
//				flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
//#ifdef SG_ENABLE_RAYTRACING
//				if (pRenderer->raytracingExtension)
//				{
//					flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV;
//				}
//#endif
//			}
//
//			if ((accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0)
//				flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
//
//			if ((accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0)
//				flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
//
//			if ((accessFlags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
//				flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
//			break;
//		}
//		case SG_QUEUE_TYPE_COMPUTE:
//		{
//			if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0 ||
//				(accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0 ||
//				(accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0 ||
//				(accessFlags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
//				return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
//
//			if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0)
//				flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
//			break;
//		}
//		case SG_QUEUE_TYPE_TRANSFER:
//			return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
//		default:
//			break;
//		}
//
//		// Compatible with both compute and graphics queues
//		if ((accessFlags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) != 0)
//			flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
//
//		if ((accessFlags & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0)
//			flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
//
//		if ((accessFlags & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT)) != 0)
//			flags |= VK_PIPELINE_STAGE_HOST_BIT;
//
//		if (flags == 0)
//			flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
//
//		return flags;
//	}
//
//	VkImageAspectFlags util_vk_determine_aspect_mask(VkFormat format, bool includeStencilBit)
//	{
//		VkImageAspectFlags result = 0;
//		switch (format)
//		{
//			// Depth
//		case VK_FORMAT_D16_UNORM:
//		case VK_FORMAT_X8_D24_UNORM_PACK32:
//		case VK_FORMAT_D32_SFLOAT:
//			result = VK_IMAGE_ASPECT_DEPTH_BIT;
//			break;
//			// Stencil
//		case VK_FORMAT_S8_UINT:
//			result = VK_IMAGE_ASPECT_STENCIL_BIT;
//			break;
//			// Depth/stencil
//		case VK_FORMAT_D16_UNORM_S8_UINT:
//		case VK_FORMAT_D24_UNORM_S8_UINT:
//		case VK_FORMAT_D32_SFLOAT_S8_UINT:
//			result = VK_IMAGE_ASPECT_DEPTH_BIT;
//			if (includeStencilBit) result |= VK_IMAGE_ASPECT_STENCIL_BIT;
//			break;
//			// Assume everything else is Color
//		default: result = VK_IMAGE_ASPECT_COLOR_BIT; break;
//		}
//		return result;
//	}
//
//	VkFormatFeatureFlags util_vk_image_usage_to_format_features(VkImageUsageFlags usage)
//	{
//		auto result = (VkFormatFeatureFlags)0;
//		if (VK_IMAGE_USAGE_SAMPLED_BIT == (usage & VK_IMAGE_USAGE_SAMPLED_BIT))
//		{
//			result |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
//		}
//		if (VK_IMAGE_USAGE_STORAGE_BIT == (usage & VK_IMAGE_USAGE_STORAGE_BIT))
//		{
//			result |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
//		}
//		if (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT == (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
//		{
//			result |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
//		}
//		if (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT == (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
//		{
//			result |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
//		}
//		return result;
//	}
//
//	VkQueueFlags util_to_vk_queue_flags(QueueType queueType)
//	{
//		switch (queueType)
//		{
//		case SG_QUEUE_TYPE_GRAPHICS: return VK_QUEUE_GRAPHICS_BIT;
//		case SG_QUEUE_TYPE_TRANSFER: return VK_QUEUE_TRANSFER_BIT;
//		case SG_QUEUE_TYPE_COMPUTE: return VK_QUEUE_COMPUTE_BIT;
//		default: ASSERT(false && "Invalid Queue Type"); return VK_QUEUE_FLAG_BITS_MAX_ENUM;
//		}
//	}
//
//	VkDescriptorType util_to_vk_descriptor_type(DescriptorType type)
//	{
//		switch (type)
//		{
//		case SG_DESCRIPTOR_TYPE_UNDEFINED: ASSERT("Invalid DescriptorInfo Type"); return VK_DESCRIPTOR_TYPE_MAX_ENUM;
//		case SG_DESCRIPTOR_TYPE_SAMPLER: return VK_DESCRIPTOR_TYPE_SAMPLER;
//		case SG_DESCRIPTOR_TYPE_TEXTURE: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
//		case SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
//		case SG_DESCRIPTOR_TYPE_RW_TEXTURE: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//		case SG_DESCRIPTOR_TYPE_BUFFER:
//		case SG_DESCRIPTOR_TYPE_RW_BUFFER: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//		case SG_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
//		case SG_DESCRIPTOR_TYPE_TEXEL_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
//		case SG_DESCRIPTOR_TYPE_RW_TEXEL_BUFFER: return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
//		case SG_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//#ifdef SG_ENABLE_RAYTRACING
//		case SG_DESCRIPTOR_TYPE_RAY_TRACING: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
//#endif
//		default:
//			ASSERT("Invalid DescriptorInfo Type");
//			return VK_DESCRIPTOR_TYPE_MAX_ENUM;
//			break;
//		}
//	}
//
//	VkShaderStageFlags util_to_vk_shader_stage_flags(ShaderStage stages)
//	{
//		VkShaderStageFlags res = 0;
//		if (stages & SG_SHADER_STAGE_ALL_GRAPHICS)
//			return VK_SHADER_STAGE_ALL_GRAPHICS;
//
//		if (stages & SG_SHADER_STAGE_VERT)
//			res |= VK_SHADER_STAGE_VERTEX_BIT;
//		if (stages & SG_SHADER_STAGE_GEOM)
//			res |= VK_SHADER_STAGE_GEOMETRY_BIT;
//		if (stages & SG_SHADER_STAGE_TESE)
//			res |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
//		if (stages & SG_SHADER_STAGE_TESC)
//			res |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
//		if (stages & SG_SHADER_STAGE_COMP)
//			res |= VK_SHADER_STAGE_COMPUTE_BIT;
//#ifdef SG_ENABLE_RAYTRACING
//		if (stages & SG_SHADER_STAGE_RAYTRACING)
//			res |= (
//				VK_SHADER_STAGE_RAYGEN_BIT_NV |
//				VK_SHADER_STAGE_ANY_HIT_BIT_NV |
//				VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
//				VK_SHADER_STAGE_MISS_BIT_NV |
//				VK_SHADER_STAGE_INTERSECTION_BIT_NV |
//				VK_SHADER_STAGE_CALLABLE_BIT_NV);
//#endif
//		ASSERT(res != 0);
//		return res;
//	}
//

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

			SG_LOG_WARING("Could not find queue of type %u. Using default queue", (uint32_t)queueType);
		}

		if (pOutProps)
			*pOutProps = queueFamilyProperties[queueFamilyIndex];
		if (pOutFamilyIndex)
			*pOutFamilyIndex = (uint8_t)queueFamilyIndex;
		if (pOutQueueIndex)
			*pOutQueueIndex = (uint8_t)queueIndex;
	}

//	static VkPipelineCacheCreateFlags util_to_pipeline_cache_flags(PipelineCacheFlags flags)
//	{
//		VkPipelineCacheCreateFlags ret = 0;
//#if VK_EXT_pipeline_creation_cache_control
//		if (flags & SG_PIPELINE_CACHE_FLAG_EXTERNALLY_SYNCHRONIZED)
//		{
//			ret |= VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT_EXT;
//		}
//#endif
//
//		return ret;
//	}
//
//	/// Multi GPU Helper Functions
//	uint32_t util_calculate_shared_device_mask(uint32_t gpuCount)
//	{
//		return (1 << gpuCount) - 1;
//	}
//
//	void util_calculate_device_indices(Renderer* pRenderer, uint32_t nodeIndex, uint32_t* pSharedNodeIndices, uint32_t sharedNodeIndexCount, uint32_t* pIndices)
//	{
//		for (uint32_t i = 0; i < pRenderer->linkedNodeCount; ++i)
//			pIndices[i] = i;
//
//		pIndices[nodeIndex] = nodeIndex;
//		/************************************************************************/
//		// Set the node indices which need sharing access to the creation node
//		// Example: Texture created on GPU0 but GPU1 will need to access it, GPU2 does not care
//		//		  pIndices = { 0, 0, 2 }
//		/************************************************************************/
//		for (uint32_t i = 0; i < sharedNodeIndexCount; ++i)
//			pIndices[pSharedNodeIndices[i]] = nodeIndex;
//	}

//	static inline VkPipelineColorBlendStateCreateInfo util_to_blend_desc(const BlendStateDesc* pDesc, VkPipelineColorBlendAttachmentState* pAttachments)
//	{
//		int blendDescIndex = 0;
//#if defined(ENABLE_GRAPHICS_DEBUG)
//
//		for (int i = 0; i < SG_MAX_RENDER_TARGET_ATTACHMENTS; ++i)
//		{
//			if (pDesc->renderTargetMask & (1 << i))
//			{
//				ASSERT(pDesc->srcFactors[blendDescIndex] < BlendConstant::SG_MAX_BLEND_CONSTANTS);
//				ASSERT(pDesc->dstFactors[blendDescIndex] < BlendConstant::SG_MAX_BLEND_CONSTANTS);
//				ASSERT(pDesc->srcAlphaFactors[blendDescIndex] < BlendConstant::SG_MAX_BLEND_CONSTANTS);
//				ASSERT(pDesc->dstAlphaFactors[blendDescIndex] < BlendConstant::SG_MAX_BLEND_CONSTANTS);
//				ASSERT(pDesc->blendModes[blendDescIndex] < BlendMode::SG_MAX_BLEND_MODES);
//				ASSERT(pDesc->blendAlphaModes[blendDescIndex] < BlendMode::SG_MAX_BLEND_MODES);
//			}
//			if (pDesc->independentBlend)
//				++blendDescIndex;
//		}
//		blendDescIndex = 0;
//#endif
//
//		for (int i = 0; i < SG_MAX_RENDER_TARGET_ATTACHMENTS; ++i)
//		{
//			if (pDesc->renderTargetMask & (1 << i))
//			{
//				VkBool32 blendEnable =
//					(gVkBlendConstantTranslator[pDesc->srcFactors[blendDescIndex]] != VK_BLEND_FACTOR_ONE ||
//						gVkBlendConstantTranslator[pDesc->dstFactors[blendDescIndex]] != VK_BLEND_FACTOR_ZERO ||
//						gVkBlendConstantTranslator[pDesc->srcAlphaFactors[blendDescIndex]] != VK_BLEND_FACTOR_ONE ||
//						gVkBlendConstantTranslator[pDesc->dstAlphaFactors[blendDescIndex]] != VK_BLEND_FACTOR_ZERO);
//
//				pAttachments[i].blendEnable = blendEnable;
//				pAttachments[i].colorWriteMask = pDesc->masks[blendDescIndex];
//				pAttachments[i].srcColorBlendFactor = gVkBlendConstantTranslator[pDesc->srcFactors[blendDescIndex]];
//				pAttachments[i].dstColorBlendFactor = gVkBlendConstantTranslator[pDesc->dstFactors[blendDescIndex]];
//				pAttachments[i].colorBlendOp = gVkBlendOpTranslator[pDesc->blendModes[blendDescIndex]];
//				pAttachments[i].srcAlphaBlendFactor = gVkBlendConstantTranslator[pDesc->srcAlphaFactors[blendDescIndex]];
//				pAttachments[i].dstAlphaBlendFactor = gVkBlendConstantTranslator[pDesc->dstAlphaFactors[blendDescIndex]];
//				pAttachments[i].alphaBlendOp = gVkBlendOpTranslator[pDesc->blendAlphaModes[blendDescIndex]];
//			}
//			if (pDesc->independentBlend)
//				++blendDescIndex;
//		}
//
//		VkPipelineColorBlendStateCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO ,nullptr };
//		createInfo.flags = 0;
//		createInfo.logicOpEnable = VK_FALSE;
//		createInfo.logicOp = VK_LOGIC_OP_CLEAR;
//		createInfo.pAttachments = pAttachments;
//		createInfo.blendConstants[0] = 0.0f;
//		createInfo.blendConstants[1] = 0.0f;
//		createInfo.blendConstants[2] = 0.0f;
//		createInfo.blendConstants[3] = 0.0f;
//
//		return createInfo;
//	}
//
//	static inline VkPipelineDepthStencilStateCreateInfo util_to_depth_desc(const DepthStateDesc* pDesc)
//	{
//		ASSERT(pDesc->depthFunc < CompareMode::SG_MAX_COMPARE_MODES);
//		ASSERT(pDesc->stencilFrontFunc < CompareMode::SG_MAX_COMPARE_MODES);
//		ASSERT(pDesc->stencilFrontFail < StencilOp::SG_MAX_STENCIL_OPS);
//		ASSERT(pDesc->depthFrontFail < StencilOp::SG_MAX_STENCIL_OPS);
//		ASSERT(pDesc->stencilFrontPass < StencilOp::SG_MAX_STENCIL_OPS);
//		ASSERT(pDesc->stencilBackFunc < CompareMode::SG_MAX_COMPARE_MODES);
//		ASSERT(pDesc->stencilBackFail < StencilOp::SG_MAX_STENCIL_OPS);
//		ASSERT(pDesc->depthBackFail < StencilOp::SG_MAX_STENCIL_OPS);
//		ASSERT(pDesc->stencilBackPass < StencilOp::SG_MAX_STENCIL_OPS);
//
//		VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr };
//		ds.flags = 0;
//		ds.depthTestEnable = pDesc->depthTest ? VK_TRUE : VK_FALSE;
//		ds.depthWriteEnable = pDesc->depthWrite ? VK_TRUE : VK_FALSE;
//		ds.depthCompareOp = gVkComparisonFuncTranslator[pDesc->depthFunc];
//		ds.depthBoundsTestEnable = VK_FALSE;
//		ds.stencilTestEnable = pDesc->stencilTest ? VK_TRUE : VK_FALSE;
//
//		ds.front.failOp = gVkStencilOpTranslator[pDesc->stencilFrontFail];
//		ds.front.passOp = gVkStencilOpTranslator[pDesc->stencilFrontPass];
//		ds.front.depthFailOp = gVkStencilOpTranslator[pDesc->depthFrontFail];
//		ds.front.compareOp = VkCompareOp(pDesc->stencilFrontFunc);
//		ds.front.compareMask = pDesc->stencilReadMask;
//		ds.front.writeMask = pDesc->stencilWriteMask;
//		ds.front.reference = 0;
//
//		ds.back.failOp = gVkStencilOpTranslator[pDesc->stencilBackFail];
//		ds.back.passOp = gVkStencilOpTranslator[pDesc->stencilBackPass];
//		ds.back.depthFailOp = gVkStencilOpTranslator[pDesc->depthBackFail];
//		ds.back.compareOp = gVkComparisonFuncTranslator[pDesc->stencilBackFunc];
//		ds.back.compareMask = pDesc->stencilReadMask;
//		ds.back.writeMask = pDesc->stencilWriteMask;
//		ds.back.reference = 0;
//
//		ds.minDepthBounds = 0;
//		ds.maxDepthBounds = 1;
//
//		return ds;
//	}
//
//	static inline VkPipelineRasterizationStateCreateInfo util_to_rasterizer_desc(const RasterizerStateDesc* pDesc)
//	{
//		ASSERT(pDesc->fillMode < FillMode::SG_MAX_FILL_MODES);
//		ASSERT(pDesc->cullMode < CullMode::SG_MAX_CULL_MODES);
//		ASSERT(pDesc->frontFace == SG_FRONT_FACE_CCW || pDesc->frontFace == SG_FRONT_FACE_CW);
//
//		VkPipelineRasterizationStateCreateInfo rs = {};
//		rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
//		rs.pNext = nullptr;
//		rs.flags = 0;
//		rs.depthClampEnable = pDesc->depthClampEnable ? VK_TRUE : VK_FALSE;
//		rs.rasterizerDiscardEnable = VK_FALSE;
//		rs.polygonMode = gVkFillModeTranslator[pDesc->fillMode];
//		rs.cullMode = gVkCullModeTranslator[pDesc->cullMode];
//		rs.frontFace = gVkFrontFaceTranslator[pDesc->frontFace];
//		rs.depthBiasEnable = (pDesc->depthBias != 0) ? VK_TRUE : VK_FALSE;
//		rs.depthBiasConstantFactor = float(pDesc->depthBias);
//		rs.depthBiasClamp = 0.0f;
//		rs.depthBiasSlopeFactor = pDesc->slopeScaledDepthBias;
//		rs.lineWidth = 1;
//
//		return rs;
//	}
//
//	static inline VkSampleCountFlagBits util_to_vk_sample_count(SampleCount sampleCount)
//	{
//		VkSampleCountFlagBits result = VK_SAMPLE_COUNT_1_BIT;
//		switch (sampleCount)
//		{
//		case SG_SAMPLE_COUNT_1:  result = VK_SAMPLE_COUNT_1_BIT;  break;
//		case SG_SAMPLE_COUNT_2:  result = VK_SAMPLE_COUNT_2_BIT;  break;
//		case SG_SAMPLE_COUNT_4:  result = VK_SAMPLE_COUNT_4_BIT;  break;
//		case SG_SAMPLE_COUNT_8:  result = VK_SAMPLE_COUNT_8_BIT;  break;
//		case SG_SAMPLE_COUNT_16: result = VK_SAMPLE_COUNT_16_BIT; break;
//		}
//		return result;
//	}
//
#pragma endregion (Utility Function)

#pragma region (Per Thread Render Pass Logic)
//
//	/// Render-passes are not exposed to the app code since they are not available on all apis
//	/// This map takes care of hashing a render pass based on the render targets passed to cmdBeginRender
//	using RenderPassMap = eastl::hash_map<uint64_t, struct RenderPass*>;
//	using RenderPassMapNode = RenderPassMap::value_type;
//	using RenderPassMapIter = RenderPassMap::iterator;
//	using FrameBufferMap = eastl::hash_map<uint64_t, struct FrameBuffer*>;
//	using FrameBufferMapNode = FrameBufferMap::value_type;
//	using FrameBufferMapIt = FrameBufferMap::iterator;
//
//	// RenderPass map per thread (this will make lookups lock free and we only need a lock when inserting a RenderPass Map for the first time)
//	eastl::hash_map<ThreadID, RenderPassMap>* gRenderPassMap;
//	// FrameBuffer map per thread (this will make lookups lock free and we only need a lock when inserting a FrameBuffer map for the first time)
//	eastl::hash_map<ThreadID, FrameBufferMap>* gFrameBufferMap;
//	Mutex* pRenderPassMutex;
//
//	static RenderPassMap& get_render_pass_map()
//	{
//		// Only need a lock when creating a new render pass map for this thread
//		MutexLock lock(*pRenderPassMutex);
//		eastl::hash_map<ThreadID, RenderPassMap>::iterator it = gRenderPassMap->find(Thread::get_curr_thread_id()); // render pass based on thread id
//		if (it == gRenderPassMap->end()) // empty
//		{
//			return gRenderPassMap->insert(Thread::get_curr_thread_id()).first->second;
//		}
//		else
//		{
//			return it->second;
//		}
//	}
//
//	static FrameBufferMap& get_frame_buffer_map()
//	{
//		// Only need a lock when creating a new framebuffer map for this thread
//		MutexLock lock(*pRenderPassMutex);
//		eastl::hash_map<ThreadID, FrameBufferMap>::iterator it = gFrameBufferMap->find(Thread::get_curr_thread_id());
//		if (it == gFrameBufferMap->end())
//		{
//			return gFrameBufferMap->insert(Thread::get_curr_thread_id()).first->second;
//		}
//		else
//		{
//			return it->second;
//		}
//	}
//
#pragma endregion (Per Thread Render Pass Logic)

#pragma region (Vulkan Rewrite Function)

	// Proxy log callback
	static void internal_log(LogType type, const char* msg, const char* component)
	{
#ifndef NX64
		switch (type)
		{
			case SG_LOG_TYPE_INFO:  SG_LOG_INFO("%s ( %s )", component, msg);   break;
			case SG_LOG_TYPE_WARN:  SG_LOG_WARING("%s ( %s )", component, msg); break;
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
			SG_LOG_WARING("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
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
			SG_LOG_WARING("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
		}
		else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		{
			SG_LOG_WARING("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
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

#pragma region (Dummy Function)

	void renderer_test_func(const char* msg)
	{
		Renderer renderer;

		SG_LOG_INFO("Renderer test function: %s", msg);
	}

	void renderer_add_func(int a, size_t num)
	{
		auto* temp = (int*)sg_calloc(1, sizeof(int));
		*temp = 0;

		for (size_t i = 0; i < num; i++)
		{
			*temp += a;
		}

		SG_LOG_DEBUG("Add num is: %d", *temp);
		sg_free(temp);
	}

	void renderer_end_func(const char* msg, bool end)
	{
		if (end)
			SG_LOG_INFO("Renderer end!");
		else
			SG_LOG_INFO("Renderer not end!");
	}

#pragma endregion (Dummy Function)

#pragma region (Internal Init Function)

	void create_instance(const char* appName, const RendererCreateDesc* pDesc, uint32_t userDefinedInstanceLayerCount,
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

		for (uint32_t i = 0; i < layerCount; ++i)
		{
			internal_log(SG_LOG_TYPE_INFO, layers[i].layerName, "vkinstance-layer");
		}

		for (uint32_t i = 0; i < extCount; ++i)
		{
			internal_log(SG_LOG_TYPE_INFO, exts[i].extensionName, "vkinstance-ext");
		}

		SG_DECLARE_ZERO(VkApplicationInfo, appInfo);
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = appName;
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

				//VkResult res = vkCreateDebugUtilsMessengerEXT(pRenderer->pVkInstance, &createInfo, &gVkAllocationCallbacks, &(pRenderer->pVkDebugUtilsMessenger));
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
				VkResult res = vkCreateDebugReportCallbackEXT(pRenderer->pVkInstance, &createInfo, &gVkAllocationCallbacks, &(pRenderer->pVkDebugReport));
				if (VK_SUCCESS != res)
				{
					internal_log(
						SG_LOG_TYPE_ERROR, "vkCreateDebugReportCallbackEXT failed - disabling Vulkan debug callbacks", "internal_vk_init_instance");
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
			//vkDestroyDebugUtilsMessengerEXT(pRenderer->pVkInstance, pRenderer->pVkDebugUtilsMessenger, &gVkAllocationCallbacks);
			pRenderer->pVkDebugUtilsMessenger = nullptr;
		}
#else
		if (pRenderer->pVkDebugReport)
		{
			vkDestroyDebugReportCallbackEXT(pRenderer->pVkInstance, pRenderer->pVkDebugReport, nullptr);
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

#pragma region (Descriptor Pool)
//
//	typedef struct DescriptorPool
//	{
//		VkDevice                        pVkDevice;
//		VkDescriptorPool                pCurrentPool;
//		VkDescriptorPoolSize*		    pPoolSizes;
//		eastl::vector<VkDescriptorPool> descriptorPools;
//		uint32_t                        poolSizeCount;
//		uint32_t                        nudescriptorsets;
//		uint32_t                        usedDescriptorSetCount;
//		VkDescriptorPoolCreateFlags     flags;
//		Mutex*					        pMutex;
//	} DescriptorPool;
//
//	static void add_descriptor_pool(Renderer* pRenderer, uint32_t nudescriptorsets, VkDescriptorPoolCreateFlags flags, VkDescriptorPoolSize* pPoolSizes,
//		uint32_t numPoolSizes, DescriptorPool** ppPool)
//	{
//		auto pPool = (DescriptorPool*)sg_calloc(1, sizeof(*ppPool));
//		pPool->flags = flags;
//		pPool->nudescriptorsets = nudescriptorsets;
//		pPool->usedDescriptorSetCount = 0;
//		pPool->pVkDevice = pRenderer->pVkDevice;
//
//		pPool->pMutex = (Mutex*)sg_calloc(1, sizeof(Mutex));
//		pPool->pMutex->Init();
//
//		pPool->poolSizeCount = numPoolSizes;
//		pPool->pPoolSizes = (VkDescriptorPoolSize*)sg_calloc(numPoolSizes, sizeof(VkDescriptorPoolSize));
//		for (uint32_t i = 0; i < numPoolSizes; ++i)
//			pPool->pPoolSizes[i] = pPoolSizes[i];
//
//		VkDescriptorPoolCreateInfo poolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr };
//		poolCreateInfo.poolSizeCount = numPoolSizes;
//		poolCreateInfo.pPoolSizes = pPoolSizes;
//		poolCreateInfo.flags = flags;
//		poolCreateInfo.maxSets = nudescriptorsets;
//
//		SG_CHECK_VKRESULT(vkCreateDescriptorPool(pPool->pVkDevice, &poolCreateInfo, &gVkAllocationCallbacks,
//			&pPool->pCurrentPool));
//
//		pPool->descriptorPools.emplace_back(pPool->pCurrentPool);
//		*ppPool = pPool;
//	}
//
//#ifdef __clang__
//#pragma clang diagnostic push
//#pragma clang diagnostic ignored "-Wunused-function"
//#endif
//	static void reset_descriptor_pool(DescriptorPool* pPool)
//	{
//		SG_CHECK_VKRESULT(vkResetDescriptorPool(pPool->pVkDevice, pPool->pCurrentPool, pPool->flags));
//		pPool->usedDescriptorSetCount = 0;
//	}
//#ifdef __clang__
//#pragma clang diagnostic pop
//#endif
//
//	static void remove_descriptor_pool(Renderer* pRenderer, DescriptorPool* pPool)
//	{
//		for (uint32_t i = 0; i < (uint32_t)pPool->descriptorPools.size(); ++i)
//			vkDestroyDescriptorPool(pRenderer->pVkDevice, pPool->descriptorPools[i], &gVkAllocationCallbacks);
//
//		pPool->descriptorPools.~vector();
//
//		pPool->pMutex->Destroy();
//		sg_free(pPool->pMutex);
//		SG_SAFE_FREE(pPool->pPoolSizes);
//		SG_SAFE_FREE(pPool);
//	}
//
//	static void allocate_descriptor_sets(DescriptorPool* pPool, const VkDescriptorSetLayout* pLayouts, VkDescriptorSet** pSets, uint32_t nudescriptorsets)
//	{
//		// need a lock since vkAllocateDescriptorSets needs to be externally synchronized
//		// This is fine since this will only happen during Init time
//		MutexLock lock(*pPool->pMutex);
//
//		SG_DECLARE_ZERO(VkDescriptorSetAllocateInfo, allocInfo);
//		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
//		allocInfo.pNext = nullptr;
//		allocInfo.descriptorPool = pPool->pCurrentPool;
//		allocInfo.descriptorSetCount = nudescriptorsets;
//		allocInfo.pSetLayouts = pLayouts;
//
//		VkResult vkRes = vkAllocateDescriptorSets(pPool->pVkDevice, &allocInfo, *pSets);
//		if (VK_SUCCESS != vkRes)
//		{
//			// try to recreate the pool
//			VkDescriptorPool pDescriptorPool = VK_NULL_HANDLE;
//
//			VkDescriptorPoolCreateInfo poolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO ,nullptr };
//			poolCreateInfo.poolSizeCount = pPool->poolSizeCount;
//			poolCreateInfo.pPoolSizes = pPool->pPoolSizes;
//			poolCreateInfo.flags = pPool->flags;
//			poolCreateInfo.maxSets = pPool->nudescriptorsets;
//
//			VkResult res = vkCreateDescriptorPool(pPool->pVkDevice, &poolCreateInfo, &gVkAllocationCallbacks, &pDescriptorPool);
//			ASSERT(VK_SUCCESS == res);
//
//			pPool->descriptorPools.emplace_back(pDescriptorPool);
//
//			pPool->pCurrentPool = pDescriptorPool;
//			pPool->usedDescriptorSetCount = 0;
//
//			allocInfo.descriptorPool = pPool->pCurrentPool;
//			vkRes = vkAllocateDescriptorSets(pPool->pVkDevice, &allocInfo, *pSets);
//		}
//		ASSERT(VK_SUCCESS == vkRes);
//		pPool->usedDescriptorSetCount += nudescriptorsets;
//	}
//
//#pragma endregion (Descriptor Pool)
//
//#pragma region (Texture)
//
//	void add_texture(Renderer* pRenderer, const TextureCreateDesc* pDesc, Texture** ppTexture)
//	{
//		ASSERT(pRenderer);
//		ASSERT(pDesc && pDesc->width && pDesc->height && (pDesc->depth || pDesc->arraySize));
//
//		if (pDesc->sampleCount > SG_SAMPLE_COUNT_1 && pDesc->mipLevels > 1)
//		{
//			SG_LOG_ERROR("Multi-Sampled textures cannot have mip maps");
//			ASSERT(false);
//			return;
//		}
//
//		size_t totalSize = sizeof(Texture);
//		totalSize += (pDesc->descriptors & SG_DESCRIPTOR_TYPE_RW_TEXTURE ? (pDesc->mipLevels * sizeof(VkImageView)) : 0);
//		auto* pTexture = (Texture*)sg_calloc_memalign(1, alignof(Texture), totalSize);
//		ASSERT(pTexture);
//
//		if (pDesc->descriptors & SG_DESCRIPTOR_TYPE_RW_TEXTURE)
//			pTexture->pVkUAVDescriptors = (VkImageView*)(pTexture + 1);
//
//		if (pDesc->pNativeHandle && !(pDesc->flags & SG_TEXTURE_CREATION_FLAG_IMPORT_BIT))
//		{
//			pTexture->ownsImage = false;
//			pTexture->pVkImage = (VkImage)pDesc->pNativeHandle;
//		}
//		else
//		{
//			pTexture->ownsImage = true;
//		}
//
//		VkImageUsageFlags additionalFlags = 0;
//		if (pDesc->startState & SG_RESOURCE_STATE_RENDER_TARGET)
//			additionalFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
//		else if (pDesc->startState & SG_RESOURCE_STATE_DEPTH_WRITE)
//			additionalFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
//
//		VkImageType imageType = VK_IMAGE_TYPE_MAX_ENUM;
//		if (pDesc->flags & SG_TEXTURE_CREATION_FLAG_FORCE_2D)
//		{
//			ASSERT(pDesc->depth == 1);
//			imageType = VK_IMAGE_TYPE_2D;
//		}
//		else if (pDesc->flags & SG_TEXTURE_CREATION_FLAG_FORCE_3D)
//		{
//			imageType = VK_IMAGE_TYPE_3D;
//		}
//		else
//		{
//			if (pDesc->depth > 1)
//				imageType = VK_IMAGE_TYPE_3D;
//			else if (pDesc->height > 1)
//				imageType = VK_IMAGE_TYPE_2D;
//			else
//				imageType = VK_IMAGE_TYPE_1D;
//		}
//
//		DescriptorType descriptors = pDesc->descriptors;
//		bool cubemapRequired = (SG_DESCRIPTOR_TYPE_TEXTURE_CUBE == (descriptors & SG_DESCRIPTOR_TYPE_TEXTURE_CUBE));
//		bool arrayRequired = false;
//
//		const bool isPlanarFormat = TinyImageFormat_IsPlanar(pDesc->format);
//		const uint32_t numOfPlanes = TinyImageFormat_NumOfPlanes(pDesc->format);
//		const bool isSinglePlane = TinyImageFormat_IsSinglePlane(pDesc->format);
//		ASSERT(((isSinglePlane && numOfPlanes == 1) || (!isSinglePlane && numOfPlanes > 1 && numOfPlanes <= MAX_PLANE_COUNT))
//			&& "Number of planes for multi-planar formats must be 2 or 3 and for single-planar formats it must be 1.");
//
//		if (imageType == VK_IMAGE_TYPE_3D)
//			arrayRequired = true;
//
//		if (VK_NULL_HANDLE == pTexture->pVkImage)
//		{
//			SG_DECLARE_ZERO(VkImageCreateInfo, createInfo);
//			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
//			createInfo.pNext = NULL;
//			createInfo.flags = 0;
//			createInfo.imageType = imageType;
//			createInfo.format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->format);
//			createInfo.extent.width = pDesc->width;
//			createInfo.extent.height = pDesc->height;
//			createInfo.extent.depth = pDesc->depth;
//			createInfo.mipLevels = pDesc->mipLevels;
//			createInfo.arrayLayers = pDesc->arraySize;
//			createInfo.samples = util_to_vk_sample_count(pDesc->sampleCount);
//			createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
//			createInfo.usage = util_to_vk_image_usage(descriptors);
//			createInfo.usage |= additionalFlags;
//			createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
//			createInfo.queueFamilyIndexCount = 0;
//			createInfo.pQueueFamilyIndices = nullptr;
//			createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//
//			if (cubemapRequired)
//				createInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
//			if (arrayRequired)
//				createInfo.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR;
//
//			SG_DECLARE_ZERO(VkFormatProperties, formatProps);
//			vkGetPhysicalDeviceFormatProperties(pRenderer->pVkActiveGPU, createInfo.format, &formatProps);
//			if (isPlanarFormat) // multi-planar formats must have each plane separately bound to memory, rather than having a single memory binding for the whole image
//			{
//				ASSERT(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DISJOINT_BIT);
//				createInfo.flags |= VK_IMAGE_CREATE_DISJOINT_BIT;
//			}
//
//			if ((VK_IMAGE_USAGE_SAMPLED_BIT & createInfo.usage) || (VK_IMAGE_USAGE_STORAGE_BIT & createInfo.usage))
//			{
//				// Make it easy to copy to and from textures
//				createInfo.usage |= (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
//			}
//
//			ASSERT(pRenderer->pCapBits->canShaderReadFrom[pDesc->format] && "GPU shader can't read from this format");
//			// verify that GPU supports this format
//			VkFormatFeatureFlags formatFeatures = util_vk_image_usage_to_format_features(createInfo.usage);
//
//			VkFormatFeatureFlags flags = formatProps.optimalTilingFeatures & formatFeatures;
//			ASSERT((0 != flags) && "Format is not supported for GPU local images (i.e. not host visible images)");
//
//			const bool linkedMultiGpu = (pRenderer->gpuMode == SG_GPU_MODE_LINKED) && (pDesc->pSharedNodeIndices || pDesc->nodeIndex);
//
//			VmaAllocationCreateInfo memReqs = { 0 };
//			if (pDesc->flags & SG_TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT)
//				memReqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
//			if (linkedMultiGpu)
//				memReqs.flags |= VMA_ALLOCATION_CREATE_DONT_BIND_BIT;
//			memReqs.usage = (VmaMemoryUsage)VMA_MEMORY_USAGE_GPU_ONLY;
//
//			VkExternalMemoryImageCreateInfoKHR externalInfo = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR, nullptr };
//#if defined(VK_USE_PLATFORM_WIN32_KHR)
//			VkImportMemoryWin32HandleInfoKHR importInfo = { VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR, nullptr };
//#endif
//			VkExportMemoryAllocateInfoKHR exportMemoryInfo = { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR, nullptr };
//			if (gExternalMemoryExtension && pDesc->flags & SG_TEXTURE_CREATION_FLAG_IMPORT_BIT)
//			{
//				createInfo.pNext = &externalInfo;
//
//#if defined(VK_USE_PLATFORM_WIN32_KHR)
//				struct ImportHandleInfo
//				{
//					void* pHandle;
//					VkExternalMemoryHandleTypeFlagBitsKHR mHandleType;
//				};
//
//				ImportHandleInfo* pHandleInfo = (ImportHandleInfo*)pDesc->pNativeHandle;
//				importInfo.handle = pHandleInfo->pHandle;
//				importInfo.handleType = pHandleInfo->mHandleType;
//
//				externalInfo.handleTypes = pHandleInfo->mHandleType;
//
//				memReqs.pUserData = &importInfo;
//				// Allocate external (importable / exportable) memory as dedicated memory to avoid adding unnecessary complexity to the Vulkan Memory Allocator
//				memReqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
//#endif
//			}
//			else if (gExternalMemoryExtension && pDesc->flags & SG_TEXTURE_CREATION_FLAG_EXPORT_BIT)
//			{
//#if defined(VK_USE_PLATFORM_WIN32_KHR)
//				exportMemoryInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
//#endif
//
//				memReqs.pUserData = &exportMemoryInfo;
//				// Allocate external (importable / exportable) memory as dedicated memory to avoid adding unnecessary complexity to the Vulkan Memory Allocator
//				memReqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
//			}
//
//			VmaAllocationInfo allocInfo = {};
//			if (isSinglePlane)
//			{
//				SG_CHECK_VKRESULT(vmaCreateImage(pRenderer->pVmaAllocator, &createInfo, &memReqs,
//					&pTexture->pVkImage, &pTexture->pVkAllocation, &allocInfo));
//			}
//			else // Multi-planar formats
//			{
//				// Create info requires the mutable format flag set for multi planar images
//				// Also pass the format list for mutable formats as per recommendation from the spec
//				// Might help to keep DCC enabled if we ever use this as a output format
//				// DCC gets disabled when we pass mutable format bit to the create info. Passing the format list helps the driver to enable it
//				VkFormat planarFormat = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->format);
//				VkImageFormatListCreateInfoKHR formatList = {};
//				formatList.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT;
//				formatList.pNext = NULL;
//				formatList.pViewFormats = &planarFormat;
//				formatList.viewFormatCount = 1;
//
//				createInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
//				createInfo.pNext = &formatList;
//
//				// Create Image
//				SG_CHECK_VKRESULT(vkCreateImage(pRenderer->pVkDevice, &createInfo, &gVkAllocationCallbacks, &pTexture->pVkImage));
//
//				VkMemoryRequirements vkMemReq = {};
//				uint64_t planesOffsets[MAX_PLANE_COUNT] = { 0 };
//				util_get_planar_vk_image_memory_requirement(pRenderer->pVkDevice, pTexture->pVkImage, numOfPlanes, &vkMemReq, planesOffsets);
//
//				// Allocate image memory
//				VkMemoryAllocateInfo mem_allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
//				mem_allocInfo.allocationSize = vkMemReq.size;
//				VkPhysicalDeviceMemoryProperties memProps = {};
//				vkGetPhysicalDeviceMemoryProperties(pRenderer->pVkActiveGPU, &memProps);
//				mem_allocInfo.memoryTypeIndex = util_get_memory_type(vkMemReq.memoryTypeBits, memProps, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
//				SG_CHECK_VKRESULT(vkAllocateMemory(pRenderer->pVkDevice, &mem_allocInfo, &gVkAllocationCallbacks, &pTexture->pVkDeviceMemory));
//
//				// Bind planes to their memories
//				VkBindImageMemoryInfo bindImagesMemoryInfo[MAX_PLANE_COUNT];
//				VkBindImagePlaneMemoryInfo bindImagePlanesMemoryInfo[MAX_PLANE_COUNT];
//				for (uint32_t i = 0; i < numOfPlanes; ++i)
//				{
//					VkBindImagePlaneMemoryInfo& bindImagePlaneMemoryInfo = bindImagePlanesMemoryInfo[i];
//					bindImagePlaneMemoryInfo.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO;
//					bindImagePlaneMemoryInfo.pNext = NULL;
//					bindImagePlaneMemoryInfo.planeAspect = (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_PLANE_0_BIT << i);
//
//					VkBindImageMemoryInfo& bindImageMemoryInfo = bindImagesMemoryInfo[i];
//					bindImageMemoryInfo.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
//					bindImageMemoryInfo.pNext = &bindImagePlaneMemoryInfo;
//					bindImageMemoryInfo.image = pTexture->pVkImage;
//					bindImageMemoryInfo.memory = pTexture->pVkDeviceMemory;
//					bindImageMemoryInfo.memoryOffset = planesOffsets[i];
//				}
//
//				SG_CHECK_VKRESULT(vkBindImageMemory2(pRenderer->pVkDevice, numOfPlanes, bindImagesMemoryInfo));
//			}
//
//			// Texture to be used on multiple GPUs
//			if (linkedMultiGpu)
//			{
//				VmaAllocationInfo allocInfo = {};
//				vmaGetAllocationInfo(pRenderer->pVmaAllocator, pTexture->pVkAllocation, &allocInfo);
//
//				// Set all the device indices to the index of the device where we will create the texture
//				uint32_t* pIndices = (uint32_t*)alloca(pRenderer->linkedNodeCount * sizeof(uint32_t));
//				util_calculate_device_indices(pRenderer, pDesc->nodeIndex, pDesc->pSharedNodeIndices, pDesc->sharedNodeIndexCount, pIndices);
//				/************************************************************************/
//				// #TODO : Move this to the Vulkan memory allocator
//				/************************************************************************/
//				VkBindImageMemoryInfoKHR            bindInfo = { VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO_KHR };
//				VkBindImageMemoryDeviceGroupInfoKHR bindDeviceGroup = { VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO_KHR };
//				bindDeviceGroup.deviceIndexCount = pRenderer->linkedNodeCount;
//				bindDeviceGroup.pDeviceIndices = pIndices;
//				bindInfo.image = pTexture->pVkImage;
//				bindInfo.memory = allocInfo.deviceMemory;
//				bindInfo.memoryOffset = allocInfo.offset;
//				bindInfo.pNext = &bindDeviceGroup;
//				SG_CHECK_VKRESULT(vkBindImageMemory2KHR(pRenderer->pVkDevice, 1, &bindInfo));
//			}
//		}
//
//		// Create image view
//		VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
//		switch (imageType)
//		{
//		case VK_IMAGE_TYPE_1D: viewType = pDesc->arraySize > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D; break;
//		case VK_IMAGE_TYPE_2D:
//			if (cubemapRequired)
//				viewType = (pDesc->arraySize > 6) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
//			else
//				viewType = pDesc->arraySize > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
//			break;
//		case VK_IMAGE_TYPE_3D:
//			if (pDesc->arraySize > 1)
//			{
//				SG_LOG_ERROR("Cannot support 3D Texture Array in Vulkan");
//				ASSERT(false);
//			}
//			viewType = VK_IMAGE_VIEW_TYPE_3D;
//			break;
//		default: ASSERT(false && "Image Format not supported!"); break;
//		}
//
//		ASSERT(viewType != VK_IMAGE_VIEW_TYPE_MAX_ENUM && "Invalid Image View");
//
//		VkImageViewCreateInfo srvDesc = {};
//		// SRV
//		srvDesc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
//		srvDesc.pNext = NULL;
//		srvDesc.flags = 0;
//		srvDesc.image = pTexture->pVkImage;
//		srvDesc.viewType = viewType;
//		srvDesc.format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->format);
//		srvDesc.components.r = VK_COMPONENT_SWIZZLE_R;
//		srvDesc.components.g = VK_COMPONENT_SWIZZLE_G;
//		srvDesc.components.b = VK_COMPONENT_SWIZZLE_B;
//		srvDesc.components.a = VK_COMPONENT_SWIZZLE_A;
//		srvDesc.subresourceRange.aspectMask = util_vk_determine_aspect_mask(srvDesc.format, false);
//		srvDesc.subresourceRange.baseMipLevel = 0;
//		srvDesc.subresourceRange.levelCount = pDesc->mipLevels;
//		srvDesc.subresourceRange.baseArrayLayer = 0;
//		srvDesc.subresourceRange.layerCount = pDesc->arraySize;
//		pTexture->aspectMask = util_vk_determine_aspect_mask(srvDesc.format, true);
//
//		if (pDesc->pVkSamplerYcbcrConversionInfo)
//		{
//			srvDesc.pNext = pDesc->pVkSamplerYcbcrConversionInfo;
//		}
//
//		if (descriptors & SG_DESCRIPTOR_TYPE_TEXTURE)
//		{
//			SG_CHECK_VKRESULT(vkCreateImageView(pRenderer->pVkDevice, &srvDesc, &gVkAllocationCallbacks, &pTexture->pVkSRVDescriptor));
//		}
//
//		// SRV stencil
//		if ((TinyImageFormat_HasStencil(pDesc->format))
//			&& (descriptors & SG_DESCRIPTOR_TYPE_TEXTURE))
//		{
//			srvDesc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
//			SG_CHECK_VKRESULT(vkCreateImageView(pRenderer->pVkDevice, &srvDesc, &gVkAllocationCallbacks, &pTexture->pVkSRVStencilDescriptor));
//		}
//
//		// UAV
//		if (descriptors & SG_DESCRIPTOR_TYPE_RW_TEXTURE)
//		{
//			VkImageViewCreateInfo uavDesc = srvDesc;
//			// #NOTE : We don't support imageCube, imageCubeArray for consistency with other APIs
//			// All cubemaps will be used as image2DArray for Image Load / Store ops
//			if (uavDesc.viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY || uavDesc.viewType == VK_IMAGE_VIEW_TYPE_CUBE)
//				uavDesc.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
//			uavDesc.subresourceRange.levelCount = 1;
//			for (uint32_t i = 0; i < pDesc->mipLevels; ++i)
//			{
//				uavDesc.subresourceRange.baseMipLevel = i;
//				SG_CHECK_VKRESULT(vkCreateImageView(pRenderer->pVkDevice, &uavDesc, &gVkAllocationCallbacks, &pTexture->pVkUAVDescriptors[i]));
//			}
//		}
//
//		pTexture->nodeIndex = pDesc->nodeIndex;
//		pTexture->width = pDesc->width;
//		pTexture->height = pDesc->height;
//		pTexture->depth = pDesc->depth;
//		pTexture->mipLevels = pDesc->mipLevels;
//		pTexture->uav = pDesc->descriptors & SG_DESCRIPTOR_TYPE_RW_TEXTURE;
//		pTexture->arraySizeMinusOne = pDesc->arraySize - 1;
//		pTexture->format = pDesc->format;
//
////#if defined(ENABLE_GRAPHICS_DEBUG)
////		if (pDesc->name)
////		{
////			set_texture_name(pRenderer, pTexture, pDesc->name);
////		}
////#endif
//
//		*ppTexture = pTexture;
//	}
//
//	void remove_texture(Renderer* pRenderer, Texture* pTexture)
//	{
//		ASSERT(pRenderer);
//		ASSERT(pTexture);
//		ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
//		ASSERT(VK_NULL_HANDLE != pTexture->pVkImage);
//
//		if (pTexture->ownsImage)
//		{
//			const TinyImageFormat fmt = (TinyImageFormat)pTexture->format;
//			const bool isSinglePlane = TinyImageFormat_IsSinglePlane(fmt);
//			if (isSinglePlane)
//			{
//				vmaDestroyImage(pRenderer->pVmaAllocator, pTexture->pVkImage, pTexture->pVkAllocation);
//			}
//			else
//			{
//				vkDestroyImage(pRenderer->pVkDevice, pTexture->pVkImage, &gVkAllocationCallbacks);
//				vkFreeMemory(pRenderer->pVkDevice, pTexture->pVkDeviceMemory, &gVkAllocationCallbacks);
//			}
//		}
//
//		if (VK_NULL_HANDLE != pTexture->pVkSRVDescriptor)
//			vkDestroyImageView(pRenderer->pVkDevice, pTexture->pVkSRVDescriptor, &gVkAllocationCallbacks);
//		if (VK_NULL_HANDLE != pTexture->pVkSRVStencilDescriptor)
//			vkDestroyImageView(pRenderer->pVkDevice, pTexture->pVkSRVStencilDescriptor, &gVkAllocationCallbacks);
//
//		if (pTexture->pVkUAVDescriptors)
//		{
//			for (uint32_t i = 0; i < pTexture->mipLevels; ++i)
//			{
//				vkDestroyImageView(pRenderer->pVkDevice, pTexture->pVkUAVDescriptors[i], &gVkAllocationCallbacks);
//			}
//		}
//
//		//if (pTexture->pSvt)
//		//{
//		//	remove_virtual_texture(pRenderer, pTexture->pSvt);
//		//}
//
//		SG_SAFE_FREE(pTexture);
//	}
//
#pragma endregion (Texture)

#pragma region (Default Resourece)
//
//	// create default resources to be used a null descriptors in case user does not specify some descriptors
//	static VkPipelineRasterizationStateCreateInfo gDefaultRasterizerDesc = {};
//	static VkPipelineDepthStencilStateCreateInfo gDefaultDepthDesc = {};
//	static VkPipelineColorBlendStateCreateInfo gDefaultBlendDesc = {};
//	static VkPipelineColorBlendAttachmentState gDefaultBlendAttachments[SG_MAX_RENDER_TARGET_ATTACHMENTS] = {};
//
//	typedef struct NullDescriptors
//	{
//		Texture* pDefaultTextureSRV[SG_MAX_LINKED_GPUS][SG_TEXTURE_DIM_COUNT];
//		Texture* pDefaultTextureUAV[SG_MAX_LINKED_GPUS][SG_TEXTURE_DIM_COUNT];
//		Buffer* pDefaultBufferSRV[SG_MAX_LINKED_GPUS];
//		Buffer* pDefaultBufferUAV[SG_MAX_LINKED_GPUS];
//		Sampler* pDefaultSampler;
//		Mutex    mSubmitMutex;
//	} NullDescriptors;
//
//	static void add_default_resources(Renderer* pRenderer)
//	{
//		pRenderer->pNullDescriptors = (NullDescriptors*)sg_calloc(1, sizeof(NullDescriptors));
//		pRenderer->pNullDescriptors->mSubmitMutex.Init();
//
//		for (uint32_t i = 0; i < pRenderer->linkedNodeCount; ++i) // the count of GPU device
//		{
//			// 1D texture
//			TextureCreateDesc textureDesc = {};
//			textureDesc.nodeIndex = i;
//			textureDesc.arraySize = 1;
//			textureDesc.depth = 1;
//			textureDesc.format = TinyImageFormat_R8G8B8A8_UNORM;
//			textureDesc.height = 1;
//			textureDesc.mipLevels = 1;
//			textureDesc.sampleCount = SG_SAMPLE_COUNT_1;
//			textureDesc.startState = SG_RESOURCE_STATE_COMMON;
//			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE;
//			textureDesc.width = 1;
//			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_1D]);
//			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_RW_TEXTURE;
//			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][SG_TEXTURE_DIM_1D]);
//
//			// 1D texture array
//			textureDesc.arraySize = 2;
//			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE;
//			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_1D_ARRAY]);
//			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_RW_TEXTURE;
//			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][SG_TEXTURE_DIM_1D_ARRAY]);
//
//			// 2D texture
//			textureDesc.width = 2;
//			textureDesc.height = 2;
//			textureDesc.mArraySize = 1;
//			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE;
//			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_2D]);
//			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_RW_TEXTURE;
//			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][SG_TEXTURE_DIM_2D]);
//
//			// 2D MS texture
//			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE;
//			textureDesc.sampleCount = SG_SAMPLE_COUNT_2;
//			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_2DMS]);
//			textureDesc.sampleCount = SG_SAMPLE_COUNT_1;
//
//			// 2D texture array
//			textureDesc.mArraySize = 2;
//			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_2D_ARRAY]);
//			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_RW_TEXTURE;
//			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][SG_TEXTURE_DIM_2D_ARRAY]);
//
//			// 2D MS texture array
//			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE;
//			textureDesc.sampleCount = SG_SAMPLE_COUNT_2;
//			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_2DMS_ARRAY]);
//			textureDesc.sampleCount = SG_SAMPLE_COUNT_1;
//
//			// 3D texture
//			textureDesc.depth = 2;
//			textureDesc.mArraySize = 1;
//			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_3D]);
//			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_RW_TEXTURE;
//			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][SG_TEXTURE_DIM_3D]);
//
//			// Cube texture
//			textureDesc.depth = 1;
//			textureDesc.mArraySize = 6;
//			textureDesc.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE_CUBE;
//			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_CUBE]);
//			textureDesc.mArraySize = 6 * 2;
//			add_texture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][SG_TEXTURE_DIM_CUBE_ARRAY]);
//
//			BufferCreateDesc bufferDesc = {};
//			bufferDesc.mNodeIndex = i;
//			bufferDesc.descriptors = SG_DESCRIPTOR_TYPE_BUFFER | SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
//			bufferDesc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_GPU_ONLY;
//			bufferDesc.startState = SG_RESOURCE_STATE_COMMON;
//			bufferDesc.size = sizeof(uint32_t);
//			bufferDesc.firstElement = 0;
//			bufferDesc.elementCount = 1;
//			bufferDesc.structStride = sizeof(uint32_t);
//			bufferDesc.mFormat = TinyImageFormat_R32_UINT;
//			add_buffer(pRenderer, &bufferDesc, &pRenderer->pNullDescriptors->pDefaultBufferSRV[i]);
//			bufferDesc.descriptors = SG_DESCRIPTOR_TYPE_RW_BUFFER;
//			add_buffer(pRenderer, &bufferDesc, &pRenderer->pNullDescriptors->pDefaultBufferUAV[i]);
//		}
//
//		SamplerDesc samplerDesc = {};
//		samplerDesc.addressU = SG_ADDRESS_MODE_CLAMP_TO_BORDER;
//		samplerDesc.addressV = SG_ADDRESS_MODE_CLAMP_TO_BORDER;
//		samplerDesc.addressW = SG_ADDRESS_MODE_CLAMP_TO_BORDER;
//		add_sampler(pRenderer, &samplerDesc, &pRenderer->pNullDescriptors->pDefaultSampler);
//
//		BlendStateDesc blendStateDesc = {};
//		// No blending
//		blendStateDesc.dstAlphaFactors[0] = SG_BLEND_CONST_ZERO;
//		blendStateDesc.dstFactors[0] = SG_BLEND_CONST_ZERO;
//		blendStateDesc.srcAlphaFactors[0] = SG_BLEND_CONST_ONE;
//		blendStateDesc.srcFactors[0] = SG_BLEND_CONST_ZERO;
//
//		blendStateDesc.masks[0] = ALL;
//		blendStateDesc.renderTargetMask = SG_BLEND_STATE_TARGET_ALL; // apply to all the render targets
//		blendStateDesc.independentBlend = false;
//		gDefaultBlendDesc = util_to_blend_desc(&blendStateDesc, gDefaultBlendAttachments);
//
//		DepthStateDesc depthStateDesc = {};
//		depthStateDesc.depthFunc = SG_COMPARE_MODE_LEQUAL;
//		depthStateDesc.depthTest = false;
//		depthStateDesc.depthWrite = false;
//		depthStateDesc.stencilBackFunc = SG_COMPARE_MODE_ALWAYS;
//		depthStateDesc.stencilFrontFunc = SG_COMPARE_MODE_ALWAYS;
//		depthStateDesc.stencilReadMask = 0xFF;
//		depthStateDesc.stencilWriteMask = 0xFF;
//		gDefaultDepthDesc = util_to_depth_desc(&depthStateDesc);
//
//		RasterizerStateDesc rasterizerStateDesc = {};
//		rasterizerStateDesc.cullMode = SG_CULL_MODE_BACK;
//		gDefaultRasterizerDesc = util_to_rasterizer_desc(&rasterizerStateDesc);
//
//		// Create command buffer to transition resources to the correct state
//		Queue* graphicsQueue = {};
//		CmdPool* cmdPool = {};
//		Cmd* cmd = {};
//
//		QueueDesc queueDesc = {};
//		queueDesc.mType = SG_QUEUE_TYPE_GRAPHICS;
//		add_queue(pRenderer, &queueDesc, &graphicsQueue);
//
//		CmdPoolDesc cmdPoolDesc = {};
//		cmdPoolDesc.pQueue = graphicsQueue;
//		cmdPoolDesc.transient = true; // for temporary use
//		add_command_pool(pRenderer, &cmdPoolDesc, &cmdPool);
//
//		CmdDesc cmdDesc = {};
//		cmdDesc.pPool = cmdPool;
//		add_cmd(pRenderer, &cmdDesc, &cmd);
//
//		// Transition resources
//		begin_cmd(cmd);
//
//		BufferBarrier bufferBarriers[SG_MAX_LINKED_GPUS * 2] = {};
//		uint32_t bufferBarrierCount = 0;
//		TextureBarrier textureBarriers[SG_MAX_LINKED_GPUS * SG_TEXTURE_DIM_COUNT * 2] = {};
//		uint32_t textureBarrierCount = 0;
//
//		for (uint32_t i = 0; i < pRenderer->linkedNodeCount; ++i)
//		{
//			for (uint32_t dim = 0; dim < SG_TEXTURE_DIM_COUNT; ++dim)
//			{
//				if (pRenderer->pNullDescriptors->pDefaultTextureSRV[i][dim])
//					textureBarriers[textureBarrierCount++] = { pRenderer->pNullDescriptors->pDefaultTextureSRV[i][dim], SG_RESOURCE_STATE_UNDEFINED, SG_RESOURCE_STATE_SHADER_RESOURCE };
//
//				if (pRenderer->pNullDescriptors->pDefaultTextureUAV[i][dim])
//					textureBarriers[textureBarrierCount++] = { pRenderer->pNullDescriptors->pDefaultTextureUAV[i][dim], SG_RESOURCE_STATE_UNDEFINED, SG_RESOURCE_STATE_UNORDERED_ACCESS };
//			}
//
//			bufferBarriers[bufferBarrierCount++] = { pRenderer->pNullDescriptors->pDefaultBufferSRV[i], SG_RESOURCE_STATE_UNDEFINED, SG_RESOURCE_STATE_SHADER_RESOURCE };
//			bufferBarriers[bufferBarrierCount++] = { pRenderer->pNullDescriptors->pDefaultBufferUAV[i], SG_RESOURCE_STATE_UNDEFINED, SG_RESOURCE_STATE_UNORDERED_ACCESS };
//		}
//
//		cmd_resource_barrier(cmd, bufferBarrierCount, bufferBarriers, textureBarrierCount, textureBarriers, 0, NULL);
//		end_cmd(cmd);
//
//		QueueSubmitDesc submitDesc = {};
//		submitDesc.cmdCount = 1;
//		submitDesc.ppCmds = &cmd;
//		queue_submit(graphicsQueue, &submitDesc);
//		wait_queue_idle(graphicsQueue);
//
//		// delete temporary resources
//		remove_cmd(pRenderer, cmd);
//		remove_command_pool(pRenderer, cmdPool);
//		remove_queue(pRenderer, graphicsQueue);
//	}
//
//	static void remove_default_resources(Renderer* pRenderer)
//	{
//		for (uint32_t i = 0; i < pRenderer->linkedNodeCount; ++i)
//		{
//			for (uint32_t dim = 0; dim < SG_TEXTURE_DIM_COUNT; ++dim)
//			{
//				if (pRenderer->pNullDescriptors->pDefaultTextureSRV[i][dim])
//					remove_texture(pRenderer, pRenderer->pNullDescriptors->pDefaultTextureSRV[i][dim]);
//
//				if (pRenderer->pNullDescriptors->pDefaultTextureUAV[i][dim])
//					remove_texture(pRenderer, pRenderer->pNullDescriptors->pDefaultTextureUAV[i][dim]);
//			}
//
//			remove_buffer(pRenderer, pRenderer->pNullDescriptors->pDefaultBufferSRV[i]);
//			remove_buffer(pRenderer, pRenderer->pNullDescriptors->pDefaultBufferUAV[i]);
//		}
//
//		remove_sampler(pRenderer, pRenderer->pNullDescriptors->pDefaultSampler);
//
//		pRenderer->pNullDescriptors->mSubmitMutex.Destroy();
//		SG_SAFE_FREE(pRenderer->pNullDescriptors);
//	}
//
#pragma endregion (Default Resourece)

#pragma region (Renderer)

//	void init_renderer(const char* appName, const RendererCreateDesc* pDesc, Renderer** ppRenderer)
//	{
//		ASSERT(appName);
//		ASSERT(pDesc);
//		ASSERT(ppRenderer);
//
//		auto* pRenderer = (Renderer*)sg_calloc_memalign(1, alignof(Renderer), sizeof(Renderer));
//		ASSERT(pRenderer);
//
//		pRenderer->gpuMode = pDesc->gpuMode;
//		pRenderer->shaderTarget = pDesc->shaderTarget;
//		pRenderer->enableGpuBasedValidation = pDesc->enableGPUBasedValidation;
//		pRenderer->api = SG_RENDERER_API_VULKAN;
//
//		pRenderer->name = (char*)sg_calloc(strlen(appName) + 1, sizeof(char));
//		strcpy(pRenderer->name, appName);
//
//		// Initialize the vulkan internal bits
//		{
//			//AGSReturnCode agsRet = agsInit();
//			//if (AGSReturnCode::AGS_SUCCESS == agsRet)
//			//{
//			//	agsPrintDriverInfo();
//			//}
//
//			//// Display NVIDIA driver version using nvapi
//			//NvAPI_Status nvStatus = nvapiInit();
//			//if (NvAPI_Status::NVAPI_OK == nvStatus)
//			//{
//			//	nvapiPrintDriverInfo();
//			//}
//
////#if defined(VK_USE_DISPATCH_TABLES)
////			VkResult vkRes = volkInitializeWithDispatchTables(pRenderer);
////			if (vkRes != VK_SUCCESS)
////			{
////				SG_LOG_ERROR("Failed to initialize vulkan");
////				return;
////			}
////#else
//			const char** instanceLayers = (const char**)alloca((2 + pDesc->instanceLayerCount) * sizeof(char*));
//			uint32_t instanceLayerCount = 0;
//
//#if defined(SG_ENABLE_GRAPHICS_DEBUG)
//			// this turns on all validation layers
//			instanceLayers[instanceLayerCount++] = "VK_LAYER_KHRONOS_validation";
//#endif
//
//			// this turns on render doc layer for gpu capture
//#ifdef USE_RENDER_DOC
//			instanceLayers[instanceLayerCount++] = "VK_LAYER_RENDERDOC_Capture";
//#endif
//
//			// Add user specified instance layers for instance creation
//			for (uint32_t i = 0; i < (uint32_t)pDesc->instanceLayerCount; ++i)
//				instanceLayers[instanceLayerCount++] = pDesc->ppInstanceLayers[i];
//
//			//#if !defined(NX64)
//			//		VkResult vkRes = volkInitialize();
//			//		if (vkRes != VK_SUCCESS)
//			//		{
//			//			LOGF(LogLevel::eERROR, "Failed to initialize Vulkan");
//			//			return;
//			//		}
//			//#endif
//
//			//SG_LOG_ERROR("Failed to initialize vulkan!");
//
//			create_instance(appName, pDesc, instanceLayerCount, instanceLayers, pRenderer);
////#endif
//			// this include the creations of physical device, device and device queue
//			if (!add_device(pDesc, pRenderer))
//			{
//				*ppRenderer = nullptr;
//				return;
//			}
//
//			// anything below LOW preset is not supported and we will exit
//			if (pRenderer->pActiveGpuSettings->gpuVendorPreset.presetLevel < SG_GPU_PRESET_LOW)
//			{
//				// have the condition in the assert as well so its cleared when the assert message box appears
//				ASSERT(pRenderer->pActiveGpuSettings->gpuVendorPreset.presetLevel >= SG_GPU_PRESET_LOW);
//
//				SG_SAFE_FREE(pRenderer->name);
//
//				// remove device and any memory we allocated in just above as this is the first function called
//				// when initializing the forge
//#if !defined(VK_USE_DISPATCH_TABLES)
//				remove_device(pRenderer);
//				remove_instance(pRenderer);
//				SG_SAFE_FREE(pRenderer);
//				SG_LOG_INFO("Selected GPU has an Office Preset in gpu.cfg.");
//				SG_LOG_INFO("Office preset is not supported by The Forge.");
//#endif
//
//				// return NULL pRenderer so that client can gracefully handle exit
//				// this is better than exiting from here in case client has allocated memory or has callbacks
//				*ppRenderer = nullptr;
//				return;
//			}
//
//			/// Vma Memory allocator
//			//VmaAllocatorCreateInfo createInfo = { 0 };
//			//createInfo.device = pRenderer->pVkDevice;
//			//createInfo.physicalDevice = pRenderer->pVkActiveGPU;
//			//createInfo.instance = pRenderer->pVkInstance;
//
//			//// Render Doc Capture currently does not support use of this extension
//			//if (gDedicatedAllocationExtension && !gRenderDocLayerEnabled)
//			//{
//			//	createInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
//			//}
//
//			//VmaVulkanFunctions vulkanFunctions = {};
//			//vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
//			//vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
//			//vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
//			//vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
//			//vulkanFunctions.vkCreateImage = vkCreateImage;
//			//vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
//			//vulkanFunctions.vkDestroyImage = vkDestroyImage;
//			//vulkanFunctions.vkFreeMemory = vkFreeMemory;
//			//vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
//			//vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
//			//vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
//			//vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
//			//vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
//			//vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
//			//vulkanFunctions.vkMapMemory = vkMapMemory;
//			//vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
//			//vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
//			//vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
//			//vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
//
//			//createInfo.pVulkanFunctions = &vulkanFunctions;
//			//createInfo.pAllocationCallbacks = &gVkAllocationCallbacks;
//			//vmaCreateAllocator(&createInfo, &pRenderer->pVmaAllocator);
//		}
//
//		VkDescriptorPoolSize descriptorPoolSizes[SG_DESCRIPTOR_TYPE_RANGE_SIZE] =
//		{
//			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1024 },
//			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
//			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 8192 },
//			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024 },
//			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1024 },
//			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1024 },
//			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8192 },
//			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
//			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024 },
//			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1 },
//			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1 },
//		};
//#ifdef VK_NV_RAY_TRACING_SPEC_VERSION
//		if (gNVRayTracingExtension)
//		{
//			descriptorPoolSizes[SG_DESCRIPTOR_TYPE_RANGE_SIZE - 1] = { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1024 };
//		}
//#endif
//
//		//add_descriptor_pool(pRenderer, 8192, (VkDescriptorPoolCreateFlags)0, descriptorPoolSizes, gDescriptorTypeRangeSize, &pRenderer->pDescriptorPool);
//
//		//pRenderPassMutex = (Mutex*)sg_calloc(1, sizeof(Mutex));
//		//pRenderPassMutex->Init();
//		//gRenderPassMap = sg_placement_new<eastl::hash_map<ThreadID, RenderPassMap> >(sg_malloc(sizeof(*gRenderPassMap)));
//		//gFrameBufferMap = sg_placement_new<eastl::hash_map<ThreadID, FrameBufferMap> >(sg_malloc(sizeof(*gFrameBufferMap)));
//
//		VkPhysicalDeviceFeatures2 gpuFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };
//		vkGetPhysicalDeviceFeatures2(pRenderer->pVkActiveGPU, &gpuFeatures);
//
//		// Set shader macro based on runtime information
//		static char descriptorIndexingMacroBuffer[2] = {};
//		static char textureArrayDynamicIndexingMacroBuffer[2] = {};
//		sprintf(descriptorIndexingMacroBuffer, "%u", (uint32_t)(gDescriptorIndexingExtension));
//		sprintf(textureArrayDynamicIndexingMacroBuffer, "%u", (uint32_t)(gpuFeatures.features.shaderSampledImageArrayDynamicIndexing));
//		static ShaderMacro rendererShaderDefines[] =
//		{
//			{ "VK_EXT_DESCRIPTOR_INDEXING_ENABLED", descriptorIndexingMacroBuffer },
//			{ "VK_FEATURE_TEXTURE_ARRAY_DYNAMIC_INDEXING_ENABLED", textureArrayDynamicIndexingMacroBuffer },
//			// Descriptor set indices
//			{ "UPDATE_FREQ_NONE",      "set = 0" },
//			{ "UPDATE_FREQ_PER_FRAME", "set = 1" },
//			{ "UPDATE_FREQ_PER_BATCH", "set = 2" },
//			{ "UPDATE_FREQ_PER_DRAW",  "set = 3" },
//		};
//		pRenderer->builtinShaderDefinesCount = sizeof(rendererShaderDefines) / sizeof(rendererShaderDefines[0]);
//		pRenderer->pBuiltinShaderDefines = rendererShaderDefines;
//
//		util_find_queue_family_index(pRenderer, 0, SG_QUEUE_TYPE_GRAPHICS, nullptr, &pRenderer->graphicsQueueFamilyIndex, nullptr);
//		util_find_queue_family_index(pRenderer, 0, SG_QUEUE_TYPE_COMPUTE,  nullptr, &pRenderer->computeQueueFamilyIndex,  nullptr);
//		util_find_queue_family_index(pRenderer, 0, SG_QUEUE_TYPE_TRANSFER, nullptr, &pRenderer->transferQueueFamilyIndex, nullptr);
//
//		SG_LOG_INFO("Graphic queue family index: %d", pRenderer->graphicsQueueFamilyIndex);
//		SG_LOG_INFO("Transfer queue family index: %d", pRenderer->transferQueueFamilyIndex);
//		SG_LOG_INFO("Compute queue family index: %d", pRenderer->computeQueueFamilyIndex);
//
//		//add_default_resources(pRenderer);
//
//		// renderer is good!
//		*ppRenderer = pRenderer;
//	}
//
//	void remove_renderer(Renderer* pRenderer)
//	{
//		ASSERT(pRenderer);
//
//		//remove_default_resources(pRenderer);
//		//remove_descriptor_pool(pRenderer, pRenderer->pDescriptorPool);
//
//		// Remove the render passes
//		//for (eastl::hash_map<ThreadID, RenderPassMap>::value_type& t : *gRenderPassMap)
//		//	for (RenderPassMapNode& it : t.second)
//		//		remove_render_pass(pRenderer, it.second);
//
//		//for (eastl::hash_map<ThreadID, FrameBufferMap>::value_type& t : *gFrameBufferMap)
//		//	for (FrameBufferMapNode& it : t.second)
//		//		remove_framebuffer(pRenderer, it.second);
//
//		// Destroy the Vulkan bits
//		//vmaDestroyAllocator(pRenderer->pVmaAllocator);
//
//#if defined(VK_USE_DISPATCH_TABLES)
//#else
//		remove_device(pRenderer);
//		remove_instance(pRenderer);
//#endif
//
//		//nvapiExit();
//		//agsExit();
//
//		//pRenderPassMutex->Destroy();
//		//gRenderPassMap->clear(true);
//		//gFrameBufferMap->clear(true);
//
//		//SG_SAFE_FREE(pRenderPassMutex);
//		//SG_SAFE_FREE(gRenderPassMap);
//		//SG_SAFE_FREE(gFrameBufferMap);
//
//		for (uint32_t i = 0; i < pRenderer->linkedNodeCount; ++i)
//		{
//			SG_SAFE_FREE(pRenderer->pAvailableQueueCount[i]);
//			SG_SAFE_FREE(pRenderer->pUsedQueueCount[i]);
//		}
//
//		// free all the renderer components!
//		SG_SAFE_FREE(pRenderer->pAvailableQueueCount);
//		SG_SAFE_FREE(pRenderer->pUsedQueueCount);
//		SG_SAFE_FREE(pRenderer->pCapBits);
//		SG_SAFE_FREE(pRenderer->name);
//		SG_SAFE_FREE(pRenderer);
//	}

	void init_renderer_test(const char* appName, const RendererCreateDesc* pDesc, Renderer** ppRenderer)
	{
		ASSERT(appName);
		ASSERT(pDesc);
		ASSERT(ppRenderer);

		auto* pRenderer = (Renderer*)sg_calloc_memalign(1, alignof(Renderer), sizeof(Renderer));
		ASSERT(pRenderer);

		pRenderer->gpuMode = pDesc->gpuMode;
		pRenderer->shaderTarget = pDesc->shaderTarget;
		pRenderer->enableGpuBasedValidation = pDesc->enableGPUBasedValidation;
		pRenderer->api = SG_RENDERER_API_VULKAN;

		pRenderer->name = (char*)sg_calloc(strlen(appName) + 1, sizeof(char));
		strcpy(pRenderer->name, appName);

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
#ifdef USE_RENDER_DOC
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

			create_instance(appName, pDesc, instanceLayerCount, instanceLayers, pRenderer);
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

//		VkDescriptorPoolSize descriptorPoolSizes[SG_DESCRIPTOR_TYPE_RANGE_SIZE] =
//		{
//			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1024 },
//			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
//			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 8192 },
//			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024 },
//			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1024 },
//			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1024 },
//			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8192 },
//			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
//			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024 },
//			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1 },
//			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1 },
//		};
//#ifdef VK_NV_RAY_TRACING_SPEC_VERSION
//		if (gNVRayTracingExtension)
//		{
//			descriptorPoolSizes[SG_DESCRIPTOR_TYPE_RANGE_SIZE - 1] = { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1024 };
//		}
//#endif

		//add_descriptor_pool(pRenderer, 8192, (VkDescriptorPoolCreateFlags)0, descriptorPoolSizes, gDescriptorTypeRangeSize, &pRenderer->pDescriptorPool);

		//pRenderPassMutex = (Mutex*)sg_calloc(1, sizeof(Mutex));
		//pRenderPassMutex->Init();
		//gRenderPassMap = sg_placement_new<eastl::hash_map<ThreadID, RenderPassMap> >(sg_malloc(sizeof(*gRenderPassMap)));
		//gFrameBufferMap = sg_placement_new<eastl::hash_map<ThreadID, FrameBufferMap> >(sg_malloc(sizeof(*gFrameBufferMap)));

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
			{ "UPDATE_FREQ_NONE",      "set = 0" },
			{ "UPDATE_FREQ_PER_FRAME", "set = 1" },
			{ "UPDATE_FREQ_PER_BATCH", "set = 2" },
			{ "UPDATE_FREQ_PER_DRAW",  "set = 3" },
		};
		pRenderer->builtinShaderDefinesCount = sizeof(rendererShaderDefines) / sizeof(rendererShaderDefines[0]);
		pRenderer->pBuiltinShaderDefines = rendererShaderDefines;

		util_find_queue_family_index(pRenderer, 0, SG_QUEUE_TYPE_GRAPHICS, nullptr, &pRenderer->graphicsQueueFamilyIndex, nullptr);
		util_find_queue_family_index(pRenderer, 0, SG_QUEUE_TYPE_COMPUTE, nullptr, &pRenderer->computeQueueFamilyIndex, nullptr);
		util_find_queue_family_index(pRenderer, 0, SG_QUEUE_TYPE_TRANSFER, nullptr, &pRenderer->transferQueueFamilyIndex, nullptr);

		SG_LOG_INFO("Graphic queue family index: %d", pRenderer->graphicsQueueFamilyIndex);
		SG_LOG_INFO("Transfer queue family index: %d", pRenderer->transferQueueFamilyIndex);
		SG_LOG_INFO("Compute queue family index: %d", pRenderer->computeQueueFamilyIndex);

		//add_default_resources(pRenderer);

		// renderer is good!
		*ppRenderer = pRenderer;
	}

	void remove_renderer_test(Renderer* pRenderer)
	{
		ASSERT(pRenderer);

		//remove_default_resources(pRenderer);
		//remove_descriptor_pool(pRenderer, pRenderer->pDescriptorPool);

		// Remove the render passes
		//for (eastl::hash_map<ThreadID, RenderPassMap>::value_type& t : *gRenderPassMap)
		//	for (RenderPassMapNode& it : t.second)
		//		remove_render_pass(pRenderer, it.second);

		//for (eastl::hash_map<ThreadID, FrameBufferMap>::value_type& t : *gFrameBufferMap)
		//	for (FrameBufferMapNode& it : t.second)
		//		remove_framebuffer(pRenderer, it.second);

		// Destroy the Vulkan bits
		vmaDestroyAllocator(pRenderer->vmaAllocator);

#if defined(VK_USE_DISPATCH_TABLES)
#else
		remove_device(pRenderer);
		remove_instance(pRenderer);
#endif

		//nvapiExit();
		//agsExit();

		//pRenderPassMutex->Destroy();
		//gRenderPassMap->clear(true);
		//gFrameBufferMap->clear(true);

		//SG_SAFE_FREE(pRenderPassMutex);
		//SG_SAFE_FREE(gRenderPassMap);
		//SG_SAFE_FREE(gFrameBufferMap);

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

}