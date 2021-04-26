#include <include/tinyimageformat_base.h>
#include <include/tinyimageformat_query.h>
#include <include/tinyimageformat_bits.h>
#include <include/tinyimageformat_apis.h>

#define TINYKTX_IMPLEMENTATION
#include <tinyktx.h>
//#include "../ThirdParty/OpenSource/basis_universal/transcoder/basisu_transcoder.h"

#define CGLTF_IMPLEMENTATION
#include <include/cgltf.h>

#include "Interface/ILog.h"
#include "Interface/IThread.h"

#include <include/EASTL/algorithm.h>
#include <include/EASTL/unordered_map.h>

//#if defined(__ANDROID__) && defined(SG_GRAPHIC_API_VULKAN)
//#include <shaderc/shaderc.h>
//#endif

//#if defined(SG_GRAPHIC_API_GLES)
//#include "OpenGLES/GLESContextCreator.h"
//#endif

#include "Math/MathTypes.h"

#include "IRenderer.h"
#include "IResourceLoader.h"

#include "Core/Atomic.h"
#include "TextureSystem/TextureContainer.h"

#include "Interface/IMemory.h"

//#ifdef NX64
//#include "../ThirdParty/OpenSource/murmurhash3/MurmurHash3_32.h"
//#endif

namespace SG
{

	struct SubresourceDataDesc
	{
		uint64_t                           srcOffset;
		uint32_t                           mipLevel;
		uint32_t                           arrayLayer;
#if defined(SG_GRAPHIC_API_D3D11) || defined(SG_GRAPHIC_API_METAL) || defined(SG_GRAPHIC_API_VULKAN)
		uint32_t                           rowPitch;
		uint32_t                           slicePitch;
#endif
	};

}

#define SG_MIP_REDUCE(s, mip) (eastl::max(1u, (uint32_t)((s) >> (mip))))

enum
{
	MAPPED_RANGE_FLAG_UNMAP_BUFFER = (1 << 0),
	MAPPED_RANGE_FLAG_TEMP_BUFFER = (1 << 1),
};

static inline uint32_t round_up(uint32_t value, uint32_t multiple) { return ((value + multiple - 1) / multiple) * multiple; }
static inline uint32_t round_down(uint32_t value, uint32_t multiple) { return value - value % multiple; }

namespace SG
{

	extern void add_buffer(Renderer* pRenderer, const BufferCreateDesc* pDesc, Buffer** ppBuffer);
	extern void remove_buffer(Renderer* pRenderer, Buffer* pBuffer);

	extern void add_texture(Renderer* pRenderer, const TextureCreateDesc* pDesc, Texture** ppTexture);
	extern void remove_texture(Renderer* pRenderer, Texture* pTexture);

	//extern void add_virtual_texture(Cmd* pCmd, const TextureCreateDesc* pDesc, Texture** ppTexture, void* pImageData);

	extern void map_buffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
	extern void unmap_buffer(Renderer* pRenderer, Buffer* pBuffer);

	// copy new buffer to override old buffer
	extern void cmd_update_buffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size);
	// copy read-in buffer to image
	extern void cmd_update_subresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const SubresourceDataDesc* pSubresourceDesc);
}

// Xbox, Orbis, Prospero, iOS have unified memory
// so we dont need a command buffer to upload linear data
// A simple memcpy suffices since the GPU memory is marked as CPU write combine
#if defined(XBOX) || defined(ORBIS) || defined(PROSPERO) || defined(TARGET_IOS)
#define UMA 1
#else
#define UMA 0
#endif

#define MAX_FRAMES 3U

//struct VertexTemp
//{
//	SG::Vec3 position;
//	SG::Vec3 normal;
//	SG::Vec2 texCoord;
//
//	bool operator==(const VertexTemp& rhs)
//	{
//		return this->position == rhs.position &&
//			this->normal == rhs.normal &&
//			this->texCoord == rhs.texCoord;
//	}
//};
//
//namespace eastl
//{
//	template <>
//	struct hash<SG::Vec2>
//	{
//		size_t operator()(SG::Vec2 const& vec2) const
//		{
//			return ((hash<float>()(vec2[0])) ^ (hash<float>()(vec2[1]) << 1));
//		}
//	};
//
//	template <>
//	struct hash<SG::Vec3>
//	{
//		size_t operator()(SG::Vec3 const& vec3) const
//		{
//			return ((hash<float>()(vec3[0]) ^
//				(hash<float>()(vec3[1]) << 1)) >> 1) ^ (hash<float>()(vec3[2]) << 1);
//		}
//	};
//
//	template <> 
//	struct hash<VertexTemp>
//	{
//		size_t operator()(VertexTemp const& vertex) const
//		{
//			return ((hash<SG::Vec3>()(vertex.position) ^
//				(hash<SG::Vec3>()(vertex.normal) << 1)) >> 1) ^ (hash<SG::Vec2>()(vertex.texCoord) << 1);
//		}
//	};
//}

namespace SG
{

	ResourceLoaderDesc gDefaultResourceLoaderDesc = { 8ull << 20, 2 };

	// Surface Utils
	static inline ResourceState util_determine_resource_start_state(bool uav)
	{
		if (uav)
			return SG_RESOURCE_STATE_UNORDERED_ACCESS;
		else	   
			return SG_RESOURCE_STATE_SHADER_RESOURCE;
	}

	static inline ResourceState util_determine_resource_start_state(const BufferCreateDesc* pDesc)
	{
		// Host visible (Upload Heap)
		if (pDesc->memoryUsage == SG_RESOURCE_MEMORY_USAGE_CPU_ONLY || pDesc->memoryUsage == SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
		{
			return SG_RESOURCE_STATE_GENERIC_READ;
		}
		// Device Local (Default Heap)
		else if (pDesc->memoryUsage == SG_RESOURCE_MEMORY_USAGE_GPU_ONLY)
		{
			DescriptorType usage = (DescriptorType)pDesc->descriptors;
			// Try to limit number of states used overall to avoid sync complexities
			if (usage & SG_DESCRIPTOR_TYPE_RW_BUFFER)
				return SG_RESOURCE_STATE_UNORDERED_ACCESS;
			if ((usage & SG_DESCRIPTOR_TYPE_VERTEX_BUFFER) || (usage & SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER))
				return SG_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
			if (usage & SG_DESCRIPTOR_TYPE_INDEX_BUFFER)
				return SG_RESOURCE_STATE_INDEX_BUFFER;
			if ((usage & SG_DESCRIPTOR_TYPE_BUFFER))
				return SG_RESOURCE_STATE_SHADER_RESOURCE;

			return SG_RESOURCE_STATE_COMMON;
		}
		// Host Cached (Readback Heap)
		else
		{
			return SG_RESOURCE_STATE_COPY_DEST;
		}
	}

	//static inline constexpr ShaderSemantic util_cgltf_attrib_type_to_semantic(cgltf_attribute_type type, uint32_t index)
	//{
	//	switch (type)
	//	{
	//	case cgltf_attribute_type_position: return SEMANTIC_POSITION;
	//	case cgltf_attribute_type_normal: return SEMANTIC_NORMAL;
	//	case cgltf_attribute_type_tangent: return SEMANTIC_TANGENT;
	//	case cgltf_attribute_type_color: return SEMANTIC_COLOR;
	//	case cgltf_attribute_type_joints: return SEMANTIC_JOINTS;
	//	case cgltf_attribute_type_weights: return SEMANTIC_WEIGHTS;
	//	case cgltf_attribute_type_texcoord:
	//		return (ShaderSemantic)(SEMANTIC_TEXCOORD0 + index);
	//	default:
	//		return SEMANTIC_TEXCOORD0;
	//	}
	//}
	//
	//static inline constexpr TinyImageFormat util_cgltf_type_to_image_format(cgltf_type type, cgltf_component_type compType)
	//{
	//	switch (type)
	//	{
	//	case cgltf_type_scalar:
	//		if (cgltf_component_type_r_8 == compType)
	//			return TinyImageFormat_R8_SINT;
	//		else if (cgltf_component_type_r_16 == compType)
	//			return TinyImageFormat_R16_SINT;
	//		else if (cgltf_component_type_r_16u == compType)
	//			return TinyImageFormat_R16_UINT;
	//		else if (cgltf_component_type_r_32f == compType)
	//			return TinyImageFormat_R32_SFLOAT;
	//		else if (cgltf_component_type_r_32u == compType)
	//			return TinyImageFormat_R32_UINT;
	//	case cgltf_type_vec2:
	//		if (cgltf_component_type_r_8 == compType)
	//			return TinyImageFormat_R8G8_SINT;
	//		else if (cgltf_component_type_r_16 == compType)
	//			return TinyImageFormat_R16G16_SINT;
	//		else if (cgltf_component_type_r_16u == compType)
	//			return TinyImageFormat_R16G16_UINT;
	//		else if (cgltf_component_type_r_32f == compType)
	//			return TinyImageFormat_R32G32_SFLOAT;
	//		else if (cgltf_component_type_r_32u == compType)
	//			return TinyImageFormat_R32G32_UINT;
	//	case cgltf_type_vec3:
	//		if (cgltf_component_type_r_8 == compType)
	//			return TinyImageFormat_R8G8B8_SINT;
	//		else if (cgltf_component_type_r_16 == compType)
	//			return TinyImageFormat_R16G16B16_SINT;
	//		else if (cgltf_component_type_r_16u == compType)
	//			return TinyImageFormat_R16G16B16_UINT;
	//		else if (cgltf_component_type_r_32f == compType)
	//			return TinyImageFormat_R32G32B32_SFLOAT;
	//		else if (cgltf_component_type_r_32u == compType)
	//			return TinyImageFormat_R32G32B32_UINT;
	//	case cgltf_type_vec4:
	//		if (cgltf_component_type_r_8 == compType)
	//			return TinyImageFormat_R8G8B8A8_SINT;
	//		else if (cgltf_component_type_r_16 == compType)
	//			return TinyImageFormat_R16G16B16A16_SINT;
	//		else if (cgltf_component_type_r_16u == compType)
	//			return TinyImageFormat_R16G16B16A16_UINT;
	//		else if (cgltf_component_type_r_32f == compType)
	//			return TinyImageFormat_R32G32B32A32_SFLOAT;
	//		else if (cgltf_component_type_r_32u == compType)
	//			return TinyImageFormat_R32G32B32A32_UINT;
	//		// #NOTE: Not applicable to vertex formats
	//	case cgltf_type_mat2:
	//	case cgltf_type_mat3:
	//	case cgltf_type_mat4:
	//	default:
	//		return TinyImageFormat_UNDEFINED;
	//	}
	//}

	#define F16_EXPONENT_BITS 0x1F
	#define F16_EXPONENT_SHIFT 10
	#define F16_EXPONENT_BIAS 15
	#define F16_MANTISSA_BITS 0x3ff
	#define F16_MANTISSA_SHIFT (23 - F16_EXPONENT_SHIFT)
	#define F16_MAX_EXPONENT (F16_EXPONENT_BITS << F16_EXPONENT_SHIFT)

	static inline uint16_t util_float_to_half(float val)
	{
		uint32_t f32 = (*(uint32_t*)&val);
		uint16_t f16 = 0;
		/* Decode IEEE 754 little-endian 32-bit floating-point value */
		int sign = (f32 >> 16) & 0x8000;
		/* Map exponent to the range [-127,128] */
		int exponent = ((f32 >> 23) & 0xff) - 127;
		int mantissa = f32 & 0x007fffff;
		if (exponent == 128)
		{ /* Infinity or NaN */
			f16 = (uint16_t)(sign | F16_MAX_EXPONENT);
			if (mantissa)
				f16 |= (mantissa & F16_MANTISSA_BITS);
		}
		else if (exponent > 15)
		{ /* Overflow - flush to Infinity */
			f16 = (unsigned short)(sign | F16_MAX_EXPONENT);
		}
		else if (exponent > -15)
		{ /* Representable value */
			exponent += F16_EXPONENT_BIAS;
			mantissa >>= F16_MANTISSA_SHIFT;
			f16 = (unsigned short)(sign | exponent << F16_EXPONENT_SHIFT | mantissa);
		}
		else
		{
			f16 = (unsigned short)sign;
		}
		return f16;
	}

	static inline void util_pack_float2_to_half2(uint32_t count, uint32_t stride, uint32_t offset, const uint8_t* src, uint8_t* dst)
	{
		struct f2 { float x; float y; };
		f2* f = (f2*)src;
		for (uint32_t e = 0; e < count; ++e)
		{
			*(uint32_t*)(dst + e * sizeof(uint32_t) + offset) = (
				(util_float_to_half(f[e].x) & 0x0000FFFF) | ((util_float_to_half(f[e].y) << 16) & 0xFFFF0000));
		}
	}

	static inline uint32_t util_float2_to_unorm2x16(const float* v)
	{
		uint32_t x = (uint32_t)round(eastl::clamp(v[0], 0.0f, 1.0f) * 65535.0f);
		uint32_t y = (uint32_t)round(eastl::clamp(v[1], 0.0f, 1.0f) * 65535.0f);
		return ((uint32_t)0x0000FFFF & x) | ((y << 16) & (uint32_t)0xFFFF0000);
	}

	#define OCT_WRAP(v, w) ((1.0f - abs((w))) * ((v) >= 0.0f ? 1.0f : -1.0f))

	static inline void util_pack_float3_direction_to_half2(uint32_t count, uint32_t stride, uint32_t offset, const uint8_t* src, uint8_t* dst)
	{
		struct f3 { float x; float y; float z; };
		for (uint32_t e = 0; e < count; ++e)
		{
			f3 f = *(f3*)(src + e * stride);
			float absLength = (abs(f.x) + abs(f.y) + abs(f.z));
			f3 enc = {};
			if (absLength)
			{
				enc.x = f.x / absLength;
				enc.y = f.y / absLength;
				enc.z = f.z / absLength;
				if (enc.z < 0)
				{
					float oldX = enc.x;
					enc.x = OCT_WRAP(enc.x, enc.y);
					enc.y = OCT_WRAP(enc.y, oldX);
				}
				enc.x = enc.x * 0.5f + 0.5f;
				enc.y = enc.y * 0.5f + 0.5f;
				*(uint32_t*)(dst + e * sizeof(uint32_t) + offset) = util_float2_to_unorm2x16(&enc.x);
			}
			else
			{
				*(uint32_t*)(dst + e * sizeof(uint32_t) + offset) = 0;
			}
		}
	}

	// Internal Structures
	typedef void(*PreMipStepFunc)(FileStream* pStream, uint32_t mip);

	typedef struct TextureUpdateDescInternal
	{
		Texture* pTexture;
		FileStream stream;
		MappedMemoryRange range;
		uint32_t          baseMipLevel;
		uint32_t          mipLevels;
		uint32_t          baseArrayLayer;
		uint32_t          layerCount;
		PreMipStepFunc    preMipFunc;
		bool              mipsAfterSlice;
	} TextureUpdateDescInternal;

	typedef struct CopyResourceSet
	{
	#if !defined(SG_GRAPHIC_API_D3D11)
		Fence* pFence;
	#endif
		Cmd* pCmd;
		CmdPool* pCmdPool;
		Buffer* pBuffer;
		uint64_t allocatedSpace;

		/// Buffers created in case we ran out of space in the original staging buffer
		/// Will be cleaned up after the fence for this set is complete
		eastl::vector<Buffer*> tempBuffers;
	} CopyResourceSet;

	// Synchronization? (i think we need a asynchronization loading system)
	// CopyEngine is just a transqueue with some extra data
	typedef struct CopyEngine
	{
		Queue* pQueue;
		CopyResourceSet* resourceSets;
		uint64_t         bufferSize;
		uint32_t         bufferCount;
		bool             isRecording;
	} CopyEngine;

	typedef enum UpdateRequestType
	{
		SG_UPDATE_REQUEST_UPDATE_BUFFER,
		SG_UPDATE_REQUEST_UPDATE_TEXTURE,
		SG_UPDATE_REQUEST_BUFFER_BARRIER,
		SG_UPDATE_REQUEST_TEXTURE_BARRIER,
		SG_UPDATE_REQUEST_LOAD_TEXTURE,
		SG_UPDATE_REQUEST_LOAD_GEOMETRY,
		SG_UPDATE_REQUEST_INVALID,
	} UpdateRequestType;

	typedef enum UploadFunctionResult
	{
		SG_UPLOAD_FUNCTION_RESULT_COMPLETED,
		SG_UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL,
		SG_UPLOAD_FUNCTION_RESULT_INVALID_REQUEST
	} UploadFunctionResult;

	// abstraction of any kind of update event happened in the resource
	struct UpdateRequest
	{
		UpdateRequest(const BufferUpdateDesc& buffer) : type(SG_UPDATE_REQUEST_UPDATE_BUFFER), bufferUpdateDesc(buffer) {}
		UpdateRequest(const TextureLoadDesc& texture) : type(SG_UPDATE_REQUEST_LOAD_TEXTURE), texLoadDesc(texture) {}
		UpdateRequest(const TextureUpdateDescInternal& texture) : type(SG_UPDATE_REQUEST_UPDATE_TEXTURE), texUpdateDesc(texture) {}
		UpdateRequest(const GeometryLoadDesc& geom) : type(SG_UPDATE_REQUEST_LOAD_GEOMETRY), geomLoadDesc(geom) {}
		UpdateRequest(const BufferBarrier& barrier) : type(SG_UPDATE_REQUEST_BUFFER_BARRIER), bufferBarrier(barrier) {}
		UpdateRequest(const TextureBarrier& barrier) : type(SG_UPDATE_REQUEST_TEXTURE_BARRIER), textureBarrier(barrier) {}

		UpdateRequestType type = SG_UPDATE_REQUEST_INVALID;
		uint64_t waitIndex = 0;
		Buffer* pUploadBuffer = nullptr;
		union
		{
			BufferUpdateDesc          bufferUpdateDesc;
			TextureUpdateDescInternal texUpdateDesc;
			TextureLoadDesc           texLoadDesc;
			GeometryLoadDesc          geomLoadDesc;
			BufferBarrier             bufferBarrier;
			TextureBarrier            textureBarrier;
		};
	};

	struct ResourceLoader
	{
		Renderer* pRenderer;

		ResourceLoaderDesc desc;

		volatile int                 run;
		ThreadDesc				     threadDesc;
		ThreadHandle                 mThread;

		Mutex                        queueMutex;
		ConditionVariable            queueCv;
		Mutex                        tokenMutex;
		ConditionVariable            tokenCv;
		eastl::vector<UpdateRequest> requestQueue[SG_MAX_LINKED_GPUS];

		sg_atomic64_t                tokenCompleted;
		sg_atomic64_t                tokenCounter;

		SyncToken                    currentTokenState[MAX_FRAMES];

		CopyEngine                   pCopyEngines[SG_MAX_LINKED_GPUS];
		uint32_t                     nextSet;
		uint32_t                     submittedSets;

	#if defined(NX64)
		ThreadTypeNX                 threadType;
		void* threadStackPtr;
	#endif
	};

	static ResourceLoader* pResourceLoader = nullptr;

	static uint32_t util_get_texture_row_alignment(Renderer* pRenderer)
	{
		return eastl::max(1u, pRenderer->pActiveGpuSettings->uploadBufferTextureRowAlignment);
	}

	static uint32_t util_get_texture_subresource_alignment(Renderer* pRenderer, TinyImageFormat fmt = TinyImageFormat_UNDEFINED)
	{
		uint32_t blockSize = eastl::max(1u, TinyImageFormat_BitSizeOfBlock(fmt) >> 3);
		uint32_t alignment = round_up(pRenderer->pActiveGpuSettings->uploadBufferTextureAlignment, blockSize);
		return round_up(alignment, util_get_texture_row_alignment(pRenderer));
	}

	//  Internal Functions
	/// Return a new staging buffer
	static MappedMemoryRange allocate_upload_memory(Renderer* pRenderer, uint64_t memoryRequirement, uint32_t alignment)
	{
	#if defined(SG_GRAPHIC_API_D3D11) || defined(SG_GRAPHIC_API_GLES)
		// there is no such thing as staging buffer in D3D11
		// to keep code paths unified in update functions, we allocate space for a dummy buffer and the system memory for pCpuMappedAddress
		Buffer* buffer = (Buffer*)sg_memalign(alignof(Buffer), sizeof(Buffer) + (size_t)memoryRequirement);
		*buffer = {};
		buffer->pCpuMappedAddress = buffer + 1;
		buffer->size = memoryRequirement;
	#else
		SG_LOG_INFO("Allocating temporary staging buffer.Required allocation size of %llu is larger than the staging buffer capacity of ", memoryRequirement);
		Buffer* buffer = {};
		BufferCreateDesc bufferDesc = {};
		bufferDesc.size = memoryRequirement;
		bufferDesc.alignment = alignment;
		bufferDesc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_CPU_ONLY;
		bufferDesc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		add_buffer(pRenderer, &bufferDesc, &buffer);
	#endif
		return { (uint8_t*)buffer->pCpuMappedAddress, buffer, 0, memoryRequirement };
	}

	// create the transfer queue and staging buffer
	static void setup_copy_engine(Renderer* pRenderer, CopyEngine* pCopyEngine, uint32_t nodeIndex, uint64_t size, uint32_t bufferCount)
	{
		QueueCreateDesc desc = { SG_QUEUE_TYPE_TRANSFER, SG_QUEUE_FLAG_NONE, SG_QUEUE_PRIORITY_NORMAL, nodeIndex };
		add_queue(pRenderer, &desc, &pCopyEngine->pQueue);

		const uint64_t maxBlockSize = 32;
		size = eastl::max(size, maxBlockSize);

		// resources that we want to copy
		pCopyEngine->resourceSets = (CopyResourceSet*)sg_malloc(sizeof(CopyResourceSet) * bufferCount);
		for (uint32_t i = 0; i < bufferCount; ++i)
		{
			sg_placement_new<CopyResourceSet>(pCopyEngine->resourceSets + i);

			CopyResourceSet& resourceSet = pCopyEngine->resourceSets[i];
	#if !defined(SG_GRAPHIC_API_D3D11)
			add_fence(pRenderer, &resourceSet.pFence);
	#endif
			CmdPoolCreateDesc cmdPoolDesc = {};
			cmdPoolDesc.pQueue = pCopyEngine->pQueue;
			cmdPoolDesc.transient = true;
			add_command_pool(pRenderer, &cmdPoolDesc, &resourceSet.pCmdPool);

			CmdCreateDesc cmdDesc = {};
			cmdDesc.pPool = resourceSet.pCmdPool;
			add_cmd(pRenderer, &cmdDesc, &resourceSet.pCmd);

			resourceSet.pBuffer = allocate_upload_memory(pRenderer, size, util_get_texture_subresource_alignment(pRenderer)).pBuffer;
		}

		pCopyEngine->bufferSize = size;
		pCopyEngine->bufferCount = bufferCount;
		pCopyEngine->isRecording = false;
	}

	static void cleanup_copy_engine(Renderer* pRenderer, CopyEngine* pCopyEngine)
	{
		for (uint32_t i = 0; i < pCopyEngine->bufferCount; ++i)
		{
			CopyResourceSet& resourceSet = pCopyEngine->resourceSets[i];
			remove_buffer(pRenderer, resourceSet.pBuffer);

			remove_cmd(pRenderer, resourceSet.pCmd);
			remove_command_pool(pRenderer, resourceSet.pCmdPool);
	#if !defined(SG_GRAPHIC_API_D3D11)
			remove_fence(pRenderer, resourceSet.pFence);
	#endif
			if (!resourceSet.tempBuffers.empty())
				SG_LOG_INFO("Was not cleaned up %d", i);
			for (Buffer*& buffer : resourceSet.tempBuffers)
			{
				remove_buffer(pRenderer, buffer);
			}
			pCopyEngine->resourceSets[i].tempBuffers.set_capacity(0);
		}

		sg_free(pCopyEngine->resourceSets);

		remove_queue(pRenderer, pCopyEngine->pQueue);
	}

	// wait for the transfer queue's fence
	static bool wait_copy_engine_set(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, bool wait)
	{
		ASSERT(!pCopyEngine->isRecording);
		CopyResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
		bool             completed = true;
	#if !defined(SG_GRAPHIC_API_D3D11)
		FenceStatus status;
		get_fence_status(pRenderer, resourceSet.pFence, &status);
		completed = status != SG_FENCE_STATUS_INCOMPLETE;
		if (wait && !completed)
		{
			wait_for_fences(pRenderer, 1, &resourceSet.pFence);
		}
	#else
		UNREF_PARAM(pRenderer);
		UNREF_PARAM(pCopyEngine);
		UNREF_PARAM(activeSet);
	#endif
		return completed;
	}

	static void reset_copy_engine_set(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet)
	{
		ASSERT(!pCopyEngine->isRecording);
		pCopyEngine->resourceSets[activeSet].allocatedSpace = 0;
		pCopyEngine->isRecording = false;

		for (Buffer*& buffer : pCopyEngine->resourceSets[activeSet].tempBuffers)
		{
			remove_buffer(pRenderer, buffer);
		}
		pCopyEngine->resourceSets[activeSet].tempBuffers.clear();
	}

	// if we are not recording the cmds, we start to record cmds and begin_cmd()
	static Cmd* acquire_cmd(CopyEngine* pCopyEngine, size_t activeSet)
	{
		CopyResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
		if (!pCopyEngine->isRecording)
		{
			reset_command_pool(pResourceLoader->pRenderer, resourceSet.pCmdPool);
			begin_cmd(resourceSet.pCmd);
			pCopyEngine->isRecording = true;
		}
		return resourceSet.pCmd;
	}

	static void flush_cmd(CopyEngine* pCopyEngine, size_t activeSet)
	{
		if (pCopyEngine->isRecording)
		{
			CopyResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
			end_cmd(resourceSet.pCmd);
			QueueSubmitDesc submitDesc = {};
			submitDesc.cmdCount = 1;
			submitDesc.ppCmds = &resourceSet.pCmd;
	#if !defined(SG_GRAPHIC_API_D3D11)
			submitDesc.pSignalFence = resourceSet.pFence;
	#endif
			{
				queue_submit(pCopyEngine->pQueue, &submitDesc);
			}
			pCopyEngine->isRecording = false;
		}
	}

	/// Return memory from pre-allocated staging buffer or create a temporary buffer if the streamer ran out of memory
	static MappedMemoryRange allocate_staging_memory(uint64_t memoryRequirement, uint32_t alignment)
	{
		// use the copy engine for GPU 0.
		CopyEngine* pCopyEngine = &pResourceLoader->pCopyEngines[0];

		uint64_t offset = pCopyEngine->resourceSets[pResourceLoader->nextSet].allocatedSpace;
		if (alignment != 0)
		{
			offset = round_up(offset, alignment);
		}

		CopyResourceSet* pResourceSet = &pCopyEngine->resourceSets[pResourceLoader->nextSet];
		uint64_t size = (uint64_t)pResourceSet->pBuffer->size;
		bool memoryAvailable = (offset < size) && (memoryRequirement <= size - offset);
		if (memoryAvailable && pResourceSet->pBuffer->pCpuMappedAddress)
		{
			Buffer* buffer = pResourceSet->pBuffer;
			ASSERT(buffer->pCpuMappedAddress);
			uint8_t* pDstData = (uint8_t*)buffer->pCpuMappedAddress + offset;
			pCopyEngine->resourceSets[pResourceLoader->nextSet].allocatedSpace = offset + memoryRequirement;
			return { pDstData, buffer, offset, memoryRequirement };
		}
		else
		{
			if (pCopyEngine->bufferSize < memoryRequirement)
			{
				MappedMemoryRange range = allocate_upload_memory(pResourceLoader->pRenderer, memoryRequirement, alignment);
				SG_LOG_INFO("Allocating temporary staging buffer. Required allocation size of %llu is larger than the staging buffer capacity of %llu", memoryRequirement, size);
				pResourceSet->tempBuffers.emplace_back(range.pBuffer);
				return range;
			}
		}

		MappedMemoryRange range = allocate_upload_memory(pResourceLoader->pRenderer, memoryRequirement, alignment);
		SG_LOG_INFO("Allocating temporary staging buffer. Required allocation size of %llu is larger than the staging buffer capacity of %llu", memoryRequirement, size);
		pResourceSet->tempBuffers.emplace_back(range.pBuffer);
		return range;
	}

	static void free_all_upload_memory()
	{
		for (size_t i = 0; i < SG_MAX_LINKED_GPUS; ++i)
		{
			for (UpdateRequest& request : pResourceLoader->requestQueue[i])
			{
				if (request.pUploadBuffer)
				{
					remove_buffer(pResourceLoader->pRenderer, request.pUploadBuffer);
				}
			}
		}
	}

	static UploadFunctionResult update_texture(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, const TextureUpdateDescInternal& texUpdateDesc)
	{
		// when this call comes from updateResource, staging buffer data is already filled
		// all that is left to do is record and execute the Copy commands
		bool dataAlreadyFilled = texUpdateDesc.range.pBuffer ? true : false;
		Texture* texture = texUpdateDesc.pTexture;
		const TinyImageFormat fmt = (TinyImageFormat)texture->format;
		FileStream stream = texUpdateDesc.stream;
		Cmd* cmd = acquire_cmd(pCopyEngine, activeSet);

		ASSERT(pCopyEngine->pQueue->nodeIndex == texUpdateDesc.pTexture->nodeIndex);

		const uint32_t sliceAlignment = util_get_texture_subresource_alignment(pRenderer, fmt);
		const uint32_t rowAlignment = util_get_texture_row_alignment(pRenderer);
		const uint64_t requiredSize = util_get_surface_size(fmt, texture->width, texture->height, texture->depth,
			rowAlignment,
			sliceAlignment,
			texUpdateDesc.baseMipLevel, texUpdateDesc.mipLevels,
			texUpdateDesc.baseArrayLayer, texUpdateDesc.layerCount);

	#if defined(SG_GRAPHIC_API_VULKAN)
		TextureBarrier barrier = { texture, SG_RESOURCE_STATE_UNDEFINED, SG_RESOURCE_STATE_COPY_DEST };
		cmd_resource_barrier(cmd, 0, nullptr, 1, &barrier, 0, nullptr);
	#endif

		MappedMemoryRange upload = dataAlreadyFilled ? texUpdateDesc.range : allocate_staging_memory(requiredSize, sliceAlignment);
		uint64_t offset = 0;

		// #TODO: Investigate - fsRead crashes if we pass the upload buffer mapped address. Allocating temporary buffer as a workaround. Does NX support loading from disk to GPU shared memory?
		//#ifdef NX64
		//	void* nxTempBuffer = NULL;
		//	if (!dataAlreadyFilled)
		//	{
		//		size_t remainingBytes = fsGetStreamFileSize(&stream) - fsGetStreamSeekPosition(&stream);
		//		nxTempBuffer = tf_malloc(remainingBytes);
		//		ssize_t bytesRead = fsReadFromStream(&stream, nxTempBuffer, remainingBytes);
		//		if (bytesRead != remainingBytes)
		//		{
		//			fsCloseStream(&stream);
		//			tf_free(nxTempBuffer);
		//			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		//		}

		//		fsCloseStream(&stream);
		//		fsOpenStreamFromMemory(nxTempBuffer, remainingBytes, FM_READ_BINARY, true, &stream);
		//	}
		//#endif

		if (!upload.pData)
		{
			return SG_UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL;
		}

		uint32_t firstStart = texUpdateDesc.mipsAfterSlice ? texUpdateDesc.baseMipLevel : texUpdateDesc.baseArrayLayer;
		uint32_t firstEnd = texUpdateDesc.mipsAfterSlice ? (texUpdateDesc.baseMipLevel + texUpdateDesc.mipLevels) : (texUpdateDesc.baseArrayLayer + texUpdateDesc.layerCount);
		uint32_t secondStart = texUpdateDesc.mipsAfterSlice ? texUpdateDesc.baseArrayLayer : texUpdateDesc.baseMipLevel;
		uint32_t secondEnd = texUpdateDesc.mipsAfterSlice ? (texUpdateDesc.baseArrayLayer + texUpdateDesc.layerCount) : (texUpdateDesc.baseMipLevel + texUpdateDesc.mipLevels);

		for (uint32_t p = 0; p < 1; ++p)
		{
			for (uint32_t j = firstStart; j < firstEnd; ++j)
			{
				if (texUpdateDesc.mipsAfterSlice && texUpdateDesc.preMipFunc)
				{
					texUpdateDesc.preMipFunc(&stream, j);
				}

				for (uint32_t i = secondStart; i < secondEnd; ++i)
				{
					if (!texUpdateDesc.mipsAfterSlice && texUpdateDesc.preMipFunc)
					{
						texUpdateDesc.preMipFunc(&stream, i);
					}

					uint32_t mip = texUpdateDesc.mipsAfterSlice ? j : i;
					uint32_t layer = texUpdateDesc.mipsAfterSlice ? i : j;

					uint32_t width = SG_MIP_REDUCE(texture->width, mip);
					uint32_t height = SG_MIP_REDUCE(texture->height, mip);
					uint32_t depth = SG_MIP_REDUCE(texture->depth, mip);

					uint32_t numBytes = 0;
					uint32_t rowBytes = 0;
					uint32_t numRows = 0;

					bool ret = util_get_surface_info(width, height, fmt, &numBytes, &rowBytes, &numRows);
					if (!ret)
					{
						return SG_UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
					}

					uint32_t subRowPitch = round_up(rowBytes, rowAlignment);
					uint32_t subSlicePitch = round_up(subRowPitch * numRows, sliceAlignment);
					uint32_t subNumRows = numRows;
					uint32_t subDepth = depth;
					uint32_t subRowSize = rowBytes;
					uint8_t* data = upload.pData + offset;

					if (!dataAlreadyFilled)
					{
						for (uint32_t z = 0; z < subDepth; ++z)
						{
							uint8_t* dstData = data + subSlicePitch * z;
							for (uint32_t r = 0; r < subNumRows; ++r)
							{
								ssize_t bytesRead = sgfs_read_from_stream(&stream, dstData + r * subRowPitch, subRowSize);
								if (bytesRead != subRowSize)
								{
									return SG_UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
								}
							}
						}
					}

					SubresourceDataDesc subresourceDesc = {};
					subresourceDesc.arrayLayer = layer;
					subresourceDesc.mipLevel = mip;
					subresourceDesc.srcOffset = upload.offset + offset;
	#if defined(SG_GRAPHIC_API_D3D11) || defined(SG_GRAPHIC_API_METAL) || defined(SG_GRAPHIC_API_VULKAN)
					subresourceDesc.rowPitch = subRowPitch;
					subresourceDesc.slicePitch = subSlicePitch;
	#endif
					// copy the buffer to the texture
					cmd_update_subresource(cmd, texture, upload.pBuffer, &subresourceDesc);
					offset += subDepth * subSlicePitch;
				}
			}
		}

	#if defined(SG_GRAPHIC_API_VULKAN)
		barrier = { texture, SG_RESOURCE_STATE_COPY_DEST, SG_RESOURCE_STATE_SHADER_RESOURCE };
		cmd_resource_barrier(cmd, 0, nullptr, 1, &barrier, 0, nullptr);
	#endif

		if (stream.pIO)
		{
			sgfs_close_stream(&stream);
		}

		return SG_UPLOAD_FUNCTION_RESULT_COMPLETED;
	}

	static UploadFunctionResult load_texture(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, const UpdateRequest& pTextureUpdate)
	{
		const TextureLoadDesc* pTextureDesc = &pTextureUpdate.texLoadDesc;

		if (pTextureDesc->fileName)
		{
			FileStream stream = {};
			char fileName[SG_MAX_FILEPATH] = {};
			bool success = false;

			TextureUpdateDescInternal updateDesc = {};
			TextureContainerType container = pTextureDesc->container;
			static const char* extensions[] = { nullptr, "dds", "ktx", "gnf", "basis", "svt" };

			// find the texture format's extension that we use
			if (SG_TEXTURE_CONTAINER_DEFAULT == container)
			{
	#if defined(TARGET_IOS) || defined(__ANDROID__) || defined(NX64)
				container = SG_TEXTURE_CONTAINER_KTX;
	#elif defined(SG_PLATFORM_WINDOWS) || defined(XBOX) || defined(__APPLE__) || defined(__linux__)
				container = SG_TEXTURE_CONTAINER_DDS;
	#elif defined(ORBIS) || defined(PROSPERO)
				container = SG_TEXTURE_CONTAINER_GNF;
	#endif
			}

			TextureCreateDesc textureDesc = {};
			textureDesc.name = pTextureDesc->fileName;

			// validate that we have found the file format now
			ASSERT(container != SG_TEXTURE_CONTAINER_DEFAULT);
			if (SG_TEXTURE_CONTAINER_DEFAULT == container)
			{
				return SG_UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
			}
			// append the extension to the name
			sgfs_append_path_extension(pTextureDesc->fileName, extensions[container], fileName);

			switch (container)
			{
			case SG_TEXTURE_CONTAINER_DDS:
			{
	#if defined(XBOX)
				success = sgfs_open_stream_from_path(SG_RD_TEXTURES, fileName, SG_FM_READ_BINARY, &stream);
				uint32_t res = 1;
				if (success)
				{
					extern uint32_t load_dds_texture(Renderer * pRenderer, FileStream * stream, const char* name, TextureCreationFlags flags, Texture * *ppTexture);
					res = load_dds_texture(pRenderer, &stream, fileName, pTextureDesc->creationFlag, pTextureDesc->ppTexture);
					sgfs_close_stream(&stream);
				}

				return res ? SG_UPLOAD_FUNCTION_RESULT_INVALID_REQUEST : SG_UPLOAD_FUNCTION_RESULT_COMPLETED;
	#else
				success = sgfs_open_stream_from_path(SG_RD_TEXTURES, fileName, SG_FM_READ_BINARY, &stream);
				if (success)
				{
					success = load_dds_texture(&stream, &textureDesc);
				}
	#endif
				break;
			}
			case SG_TEXTURE_CONTAINER_KTX:
			{
				success = sgfs_open_stream_from_path(SG_RD_TEXTURES, fileName, SG_FM_READ_BINARY, &stream);
				if (success)
				{
					success = load_ktx_texture(&stream, &textureDesc);
					updateDesc.mipsAfterSlice = true;
					// KTX stores mip size before the mip data
					// This function gets called to skip the mip size so we read the mip data
					updateDesc.preMipFunc = [](FileStream* pStream, uint32_t)
					{
						uint32_t mipSize = 0;
						sgfs_read_from_stream(pStream, &mipSize, sizeof(mipSize));
					};
				}
				break;
			}
			//case SG_TEXTURE_CONTAINER_BASIS:
			//{
			//	void* data = nullptr;
			//	uint32_t dataSize = 0;
			//	success = sgfs_open_stream_from_path(SG_RD_TEXTURES, fileName, SG_FM_READ_BINARY, &stream);
			//	if (success)
			//	{
			//		//success = loadBASISTextureDesc(&stream, &textureDesc, &data, &dataSize);
			//		if (success)
			//		{
			//			sgfs_close_stream(&stream);
			//			sgfs_open_stream_from_memory(data, dataSize, SG_FM_READ_BINARY, true, &stream);
			//		}
			//	}
			//	break;
			//}
	//		case SG_TEXTURE_CONTAINER_GNF:
	//		{
	//#if defined(ORBIS) || defined(PROSPERO)
	//			success = sgfs_open_stream_from_path(SG_RD_TEXTURES, fileName, SG_FM_READ_BINARY, &stream);
	//			uint32_t res = 1;
	//			if (success)
	//			{
	//				extern uint32_t loadGnfTexture(Renderer * pRenderer, FileStream * stream, const char* name, TextureCreationFlags flags, Texture * *ppTexture);
	//				res = loadGnfTexture(pRenderer, &stream, fileName, pTextureDesc->mCreationFlag, pTextureDesc->ppTexture);
	//				sgfs_close_stream(&stream);
	//			}

	//			return res ? SG_UPLOAD_FUNCTION_RESULT_INVALID_REQUEST : SG_UPLOAD_FUNCTION_RESULT_COMPLETED;
	//#endif
	//		}
			default:
				break;
			}

			if (success)
			{
				textureDesc.startState = SG_RESOURCE_STATE_COMMON;
				textureDesc.flags |= pTextureDesc->creationFlag;
				textureDesc.nodeIndex = pTextureDesc->nodeIndex;
	#if defined (SG_GRAPHIC_API_VULKAN)
				if (nullptr != pTextureDesc->desc)
					textureDesc.pVkSamplerYcbcrConversionInfo = pTextureDesc->desc->pVkSamplerYcbcrConversionInfo;
	#endif
				add_texture(pRenderer, &textureDesc, pTextureDesc->ppTexture);

				updateDesc.stream = stream;
				updateDesc.pTexture = *pTextureDesc->ppTexture;
				updateDesc.baseMipLevel = 0;
				updateDesc.mipLevels = textureDesc.mipLevels;
				updateDesc.baseArrayLayer = 0;
				updateDesc.layerCount = textureDesc.arraySize;

				return update_texture(pRenderer, pCopyEngine, activeSet, updateDesc);
			}

			// Sparse Textures
	#if defined(SG_GRAPHIC_API_D3D12) || defined(SG_GRAPHIC_API_VULKAN)
			if (SG_TEXTURE_CONTAINER_SVT == container)
			{
				if (sgfs_open_stream_from_path(SG_RD_TEXTURES, fileName, SG_FM_READ_BINARY, &stream))
				{
					success = load_svt_texture(&stream, &textureDesc);
					if (success)
					{
						ssize_t dataSize = sgfs_get_stream_file_size(&stream) - sgfs_get_offset_stream_position(&stream);
						void* data = sg_malloc(dataSize);
						sgfs_read_from_stream(&stream, data, dataSize);

						textureDesc.startState = SG_RESOURCE_STATE_COPY_DEST;
						textureDesc.flags |= pTextureDesc->creationFlag;
						textureDesc.nodeIndex = pTextureDesc->nodeIndex;
						//add_virtual_texture(acquire_cmd(pCopyEngine, activeSet), &textureDesc, pTextureDesc->ppTexture, data);
						// Create visibility buffer
						eastl::vector<VirtualTexturePage>* pPageTable = (eastl::vector<VirtualTexturePage>*)(*pTextureDesc->ppTexture)->pSvt->pPages;

						if (pPageTable == nullptr)
							return SG_UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;

						BufferLoadDesc visDesc = {};
						visDesc.desc.descriptors = SG_DESCRIPTOR_TYPE_RW_BUFFER; // UAV
						visDesc.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_GPU_ONLY;
						visDesc.desc.structStride = sizeof(unsigned int);
						visDesc.desc.elementCount = (uint64_t)pPageTable->size();
						visDesc.desc.size = visDesc.desc.structStride * visDesc.desc.elementCount;
						visDesc.desc.startState = SG_RESOURCE_STATE_COMMON;
						visDesc.desc.name = "Vis Buffer for Sparse Texture";
						visDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->visibility;
						add_resource(&visDesc, nullptr);

						BufferLoadDesc prevVisDesc = {};
						prevVisDesc.desc.descriptors = SG_DESCRIPTOR_TYPE_RW_BUFFER;
						prevVisDesc.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_GPU_ONLY;
						prevVisDesc.desc.structStride = sizeof(unsigned int);
						prevVisDesc.desc.elementCount = (uint64_t)pPageTable->size();
						prevVisDesc.desc.size = prevVisDesc.desc.structStride * prevVisDesc.desc.elementCount;
						prevVisDesc.desc.startState = SG_RESOURCE_STATE_COMMON;
						prevVisDesc.desc.name = "Prev Vis Buffer for Sparse Texture";
						prevVisDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->prevVisibility;
						add_resource(&prevVisDesc, nullptr);

						BufferLoadDesc alivePageDesc = {};
						alivePageDesc.desc.descriptors = SG_DESCRIPTOR_TYPE_RW_BUFFER;
						alivePageDesc.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	#if defined(SG_GRAPHIC_API_D3D12)			  
						alivePageDesc.desc.flags = SG_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	#elif defined(SG_GRAPHIC_API_VULKAN)	
						alivePageDesc.desc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	#else							  	  
						alivePageDesc.desc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | SG_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	#endif							  
						alivePageDesc.desc.structStride = sizeof(unsigned int);
						alivePageDesc.desc.elementCount = (uint64_t)pPageTable->size();
						alivePageDesc.desc.size = alivePageDesc.desc.structStride * alivePageDesc.desc.elementCount;
						alivePageDesc.desc.name = "Alive pages buffer for Sparse Texture";
						alivePageDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->alivePage;
						add_resource(&alivePageDesc, nullptr);

						BufferLoadDesc removePageDesc = {};
						removePageDesc.desc.descriptors = SG_DESCRIPTOR_TYPE_RW_BUFFER;
						removePageDesc.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	#if defined(SG_GRAPHIC_API_D3D12)
						removePageDesc.desc.flags = SG_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	#elif defined(SG_GRAPHIC_API_VULKAN)
						removePageDesc.desc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	#else
						removePageDesc.desc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | SG_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	#endif
						removePageDesc.desc.structStride = sizeof(unsigned int);
						removePageDesc.desc.elementCount = (uint64_t)pPageTable->size();
						removePageDesc.desc.size = removePageDesc.desc.structStride * removePageDesc.desc.elementCount;
						removePageDesc.desc.name = "Remove pages buffer for Sparse Texture";
						removePageDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->removePage;
						add_resource(&removePageDesc, nullptr);

						BufferLoadDesc pageCountsDesc = {};
						pageCountsDesc.desc.descriptors = SG_DESCRIPTOR_TYPE_RW_BUFFER;
						pageCountsDesc.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	#if defined(SG_GRAPHIC_API_D3D12)
						pageCountsDesc.desc.flags = SG_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	#elif defined(SG_GRAPHIC_API_VULKAN)
						pageCountsDesc.desc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	#else
						pageCountsDesc.desc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | SG_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	#endif
						pageCountsDesc.desc.structStride = sizeof(unsigned int);
						pageCountsDesc.desc.elementCount = 4;
						pageCountsDesc.desc.size = pageCountsDesc.desc.structStride * pageCountsDesc.desc.elementCount;
						pageCountsDesc.desc.name = "Page count buffer for Sparse Texture";
						pageCountsDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->pageCounts;
						add_resource(&pageCountsDesc, nullptr);

						sgfs_close_stream(&stream);

						return SG_UPLOAD_FUNCTION_RESULT_COMPLETED;
					}
				}

				return SG_UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
			}
	#endif
		}

		return SG_UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
	}

	static UploadFunctionResult update_buffer(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, const BufferUpdateDesc& bufUpdateDesc)
	{
		ASSERT(pCopyEngine->pQueue->nodeIndex == bufUpdateDesc.pBuffer->nodeIndex);
		Buffer* pBuffer = bufUpdateDesc.pBuffer;
		ASSERT(pBuffer->memoryUsage == SG_RESOURCE_MEMORY_USAGE_GPU_ONLY || pBuffer->memoryUsage == SG_RESOURCE_MEMORY_USAGE_GPU_TO_CPU);

		Cmd* pCmd = acquire_cmd(pCopyEngine, activeSet);

		MappedMemoryRange range = bufUpdateDesc.mInternal.mappedRange;
		// copy the data from the src buffer to dst buffer
		cmd_update_buffer(pCmd, pBuffer, bufUpdateDesc.dstOffset, range.pBuffer, range.offset, range.size);

		return SG_UPLOAD_FUNCTION_RESULT_COMPLETED;
	}

	static inline constexpr ShaderSemantic util_cgltf_attrib_type_to_shader_semantic(cgltf_attribute_type type, uint32_t index)
	{
		switch (type)
		{
			case cgltf_attribute_type_position: return SG_SEMANTIC_POSITION;
			case cgltf_attribute_type_normal:   return SG_SEMANTIC_NORMAL;
			case cgltf_attribute_type_tangent:  return SG_SEMANTIC_TANGENT;
			case cgltf_attribute_type_color:    return SG_SEMANTIC_COLOR;
			case cgltf_attribute_type_joints:   return SG_SEMANTIC_JOINTS;
			case cgltf_attribute_type_weights:  return SG_SEMANTIC_WEIGHTS;
			case cgltf_attribute_type_texcoord:
				return (ShaderSemantic)(SG_SEMANTIC_TEXCOORD0 + index);
			default:
				return SG_SEMANTIC_TEXCOORD0;
		}
	}

	static inline constexpr TinyImageFormat util_cgltf_type_to_image_format(cgltf_type type, cgltf_component_type compType)
	{
		switch (type)
		{
		case cgltf_type_scalar:
			if (cgltf_component_type_r_8 == compType)
				return TinyImageFormat_R8_SINT;
			else if (cgltf_component_type_r_16 == compType)
				return TinyImageFormat_R16_SINT;
			else if (cgltf_component_type_r_16u == compType)
				return TinyImageFormat_R16_UINT;
			else if (cgltf_component_type_r_32f == compType)
				return TinyImageFormat_R32_SFLOAT;
			else if (cgltf_component_type_r_32u == compType)
				return TinyImageFormat_R32_UINT;
		case cgltf_type_vec2:
			if (cgltf_component_type_r_8 == compType)
				return TinyImageFormat_R8G8_SINT;
			else if (cgltf_component_type_r_16 == compType)
				return TinyImageFormat_R16G16_SINT;
			else if (cgltf_component_type_r_16u == compType)
				return TinyImageFormat_R16G16_UINT;
			else if (cgltf_component_type_r_32f == compType)
				return TinyImageFormat_R32G32_SFLOAT;
			else if (cgltf_component_type_r_32u == compType)
				return TinyImageFormat_R32G32_UINT;
		case cgltf_type_vec3:
			if (cgltf_component_type_r_8 == compType)
				return TinyImageFormat_R8G8B8_SINT;
			else if (cgltf_component_type_r_16 == compType)
				return TinyImageFormat_R16G16B16_SINT;
			else if (cgltf_component_type_r_16u == compType)
				return TinyImageFormat_R16G16B16_UINT;
			else if (cgltf_component_type_r_32f == compType)
				return TinyImageFormat_R32G32B32_SFLOAT;
			else if (cgltf_component_type_r_32u == compType)
				return TinyImageFormat_R32G32B32_UINT;
		case cgltf_type_vec4:
			if (cgltf_component_type_r_8 == compType)
				return TinyImageFormat_R8G8B8A8_SINT;
			else if (cgltf_component_type_r_16 == compType)
				return TinyImageFormat_R16G16B16A16_SINT;
			else if (cgltf_component_type_r_16u == compType)
				return TinyImageFormat_R16G16B16A16_UINT;
			else if (cgltf_component_type_r_32f == compType)
				return TinyImageFormat_R32G32B32A32_SFLOAT;
			else if (cgltf_component_type_r_32u == compType)
				return TinyImageFormat_R32G32B32A32_UINT;
			// #NOTE: Not applicable to vertex formats
		case cgltf_type_mat2:
		case cgltf_type_mat3:
		case cgltf_type_mat4:
		default:
			return TinyImageFormat_UNDEFINED;
		}
	}

	static UploadFunctionResult load_geometry(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, UpdateRequest& pGeometryLoad)
	{
		GeometryLoadDesc* pDesc = &pGeometryLoad.geomLoadDesc;

		char iext[SG_MAX_FILEPATH] = { 0 };
		sgfs_get_path_extension(pDesc->fileName, iext);

		// Geometry in gltf container
		if (iext[0] != 0 && (stricmp(iext, "gltf") == 0 || stricmp(iext, "glb") == 0))
		{
			FileStream file = {};
			if (!sgfs_open_stream_from_path(SG_RD_MESHES, pDesc->fileName, SG_FM_READ_BINARY, &file))
			{
				SG_LOG_ERROR("Failed to open gltf file %s", pDesc->fileName);
				ASSERT(false);
				return SG_UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
			}

			ssize_t fileSize = sgfs_get_stream_file_size(&file);
			void* fileData = sg_malloc(fileSize);
			cgltf_result result = cgltf_result_invalid_gltf;

			sgfs_read_from_stream(&file, fileData, fileSize);

			cgltf_options options = {};
			cgltf_data* data = nullptr;
			// use seagull memory allocation
			options.memory.alloc = [](void* user, cgltf_size size) { return sg_malloc(size); };
			options.memory.free = [](void* user, void* ptr) { sg_free(ptr); };
			result = cgltf_parse(&options, fileData, fileSize, &data);

			sgfs_close_stream(&file);

			if (cgltf_result_success != result)
			{
				SG_LOG_ERROR("Failed to parse gltf file %s with error %u", pDesc->fileName, (uint32_t)result);
				ASSERT(false);
				sg_free(fileData);
				return SG_UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
			}

	#if defined(SG_DEBUG)
			result = cgltf_validate(data);
			if (cgltf_result_success != result)
			{
				SG_LOG_WARNING("GLTF validation finished with error %u for file %s", (uint32_t)result, pDesc->fileName);
			}
	#endif

			// Load buffers located in separate files (.bin) using our file system
			for (uint32_t i = 0; i < data->buffers_count; ++i)
			{
				const char* uri = data->buffers[i].uri;

				if (!uri || data->buffers[i].data)
				{
					continue;
				}

				if (strncmp(uri, "data:", 5) != 0 && !strstr(uri, "://"))
				{
					char parent[SG_MAX_FILEPATH] = { 0 };
					sgfs_get_parent_path(pDesc->fileName, parent);
					char path[SG_MAX_FILEPATH] = { 0 };
					sgfs_append_path_component(parent, uri, path);
					FileStream fs = {};
					if (sgfs_open_stream_from_path(SG_RD_MESHES, path, SG_FM_READ_BINARY, &fs))
					{
						ASSERT(sgfs_get_stream_file_size(&fs) >= (ssize_t)data->buffers[i].size);
						data->buffers[i].data = sg_malloc(data->buffers[i].size);
						sgfs_read_from_stream(&fs, data->buffers[i].data, data->buffers[i].size);
					}
					sgfs_close_stream(&fs);
				}
			}

			result = cgltf_load_buffers(&options, data, pDesc->fileName);
			if (cgltf_result_success != result)
			{
				SG_LOG_ERROR("Failed to load buffers from gltf file %s with error %u", pDesc->fileName, (uint32_t)result);
				ASSERT(false);
				sg_free(fileData);
				return SG_UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
			}

			typedef void (*PackingFunction)(uint32_t count, uint32_t stride, uint32_t offset, const uint8_t* src, uint8_t* dst);

			uint32_t vertexStrides[SG_SEMANTIC_TEXCOORD9 + 1] = {};
			uint32_t vertexAttribCount[SG_SEMANTIC_TEXCOORD9 + 1] = {};
			uint32_t vertexOffsets[SG_SEMANTIC_TEXCOORD9 + 1] = {};
			uint32_t vertexBindings[SG_SEMANTIC_TEXCOORD9 + 1] = {};
			cgltf_attribute* vertexAttribs[SG_SEMANTIC_TEXCOORD9 + 1] = {};
			PackingFunction vertexPacking[SG_SEMANTIC_TEXCOORD9 + 1] = {};
			for (uint32_t i = 0; i < SG_SEMANTIC_TEXCOORD9 + 1; ++i)
				vertexOffsets[i] = UINT_MAX;

			uint32_t indexCount = 0;
			uint32_t vertexCount = 0;
			uint32_t drawCount = 0;
			uint32_t jointCount = 0;
			uint32_t vertexBufferCount = 0;

			// Find number of traditional draw calls required to draw this piece of geometry
			// Find total index count, total vertex count
			for (uint32_t i = 0; i < data->meshes_count; ++i)
			{
				for (uint32_t p = 0; p < data->meshes[i].primitives_count; ++p)
				{
					const cgltf_primitive* prim = &data->meshes[i].primitives[p];
					indexCount += (uint32_t)prim->indices->count;
					vertexCount += (uint32_t)prim->attributes->data->count;
					++drawCount;

					for (uint32_t i = 0; i < prim->attributes_count; ++i)
						vertexAttribs[util_cgltf_attrib_type_to_shader_semantic(prim->attributes[i].type, prim->attributes[i].index)] = &prim->attributes[i];
				}
			}

			// Determine vertex stride for each binding
			for (uint32_t i = 0; i < pDesc->pVertexLayout->attribCount; ++i)
			{
				const VertexAttrib* attr = &pDesc->pVertexLayout->attribs[i];
				const cgltf_attribute* cgltfAttr = vertexAttribs[attr->semantic];
				ASSERT(cgltfAttr);

				const uint32_t dstFormatSize = TinyImageFormat_BitSizeOfBlock(attr->format) >> 3;
				const uint32_t srcFormatSize = (uint32_t)cgltfAttr->data->stride;

				vertexStrides[attr->binding] += dstFormatSize ? dstFormatSize : srcFormatSize;
				vertexOffsets[attr->semantic] = attr->offset;
				vertexBindings[attr->semantic] = attr->binding;
				++vertexAttribCount[attr->binding];

				// Compare vertex attrib format to the gltf attrib type
				// Select a packing function if dst format is packed version
				// Texcoords - Pack float2 to half2
				// Directions - Pack float3 to float2 to unorm2x16 (Normal, Tangent)
				// Position - No packing yet
				const TinyImageFormat srcFormat = util_cgltf_type_to_image_format(cgltfAttr->data->type, cgltfAttr->data->component_type);
				const TinyImageFormat dstFormat = attr->format == TinyImageFormat_UNDEFINED ? srcFormat : attr->format;

				if (dstFormat != srcFormat)
				{
					// Select appropriate packing function which will be used when filling the vertex buffer
					switch (cgltfAttr->type)
					{
					case cgltf_attribute_type_texcoord:
					{
						if (sizeof(uint32_t) == dstFormatSize && sizeof(float[2]) == srcFormatSize)
							vertexPacking[attr->semantic] = util_pack_float2_to_half2;
						// #TODO: Add more variations if needed
						break;
					}
					case cgltf_attribute_type_normal:
					case cgltf_attribute_type_tangent:
					{
						if (sizeof(uint32_t) == dstFormatSize && (sizeof(float[3]) == srcFormatSize || sizeof(float[4]) == srcFormatSize))
							vertexPacking[attr->semantic] = util_pack_float3_direction_to_half2;
						// #TODO: Add more variations if needed
						break;
					}
					default:
						break;
					}
				}
			}

			// determine number of vertex buffers needed based on number of unique bindings found
			// for each unique binding the vertex stride will be non zero
			for (uint32_t i = 0; i < SG_MAX_VERTEX_BINDINGS; ++i)
				if (vertexStrides[i])
					++vertexBufferCount;

			for (uint32_t i = 0; i < data->skins_count; ++i)
				jointCount += (uint32_t)data->skins[i].joints_count;

			// Determine index stride
			// This depends on vertex count rather than the stride specified in gltf
			// since gltf assumes we have index buffer per primitive which is non optimal
			const uint32_t indexStride = vertexCount > UINT16_MAX ? sizeof(uint32_t) : sizeof(uint16_t);

			uint32_t totalSize = 0;
			totalSize += round_up(sizeof(Geometry), 16);
			totalSize += round_up(drawCount * sizeof(IndirectDrawIndexArguments), 16);
			totalSize += round_up(jointCount * sizeof(Matrix4), 16);
			totalSize += round_up(jointCount * sizeof(uint32_t), 16);

			Geometry* geom = (Geometry*)sg_calloc(1, totalSize);
			ASSERT(geom);

			geom->pDrawArgs = (IndirectDrawIndexArguments*)(geom + 1);
			geom->pInverseBindPoses = (Matrix4*)((uint8_t*)geom->pDrawArgs + round_up(drawCount * sizeof(*geom->pDrawArgs), 16));
			geom->pJointRemaps = (uint32_t*)((uint8_t*)geom->pInverseBindPoses + round_up(jointCount * sizeof(*geom->pInverseBindPoses), 16));

			uint32_t shadowSize = 0;
			if (pDesc->flags & SG_GEOMETRY_LOAD_FLAG_SHADOWED)
			{
				shadowSize += (uint32_t)vertexAttribs[SG_SEMANTIC_POSITION]->data->stride * vertexCount;
				shadowSize += indexCount * indexStride;

				geom->pShadow = (Geometry::ShadowData*)sg_calloc(1, sizeof(Geometry::ShadowData) + shadowSize);
				geom->pShadow->pIndices = geom->pShadow + 1;
				geom->pShadow->pAttributes[SG_SEMANTIC_POSITION] = (uint8_t*)geom->pShadow->pIndices + (indexCount * indexStride);
				// #TODO: Add more if needed
			}

			geom->vertexBufferCount = vertexBufferCount;
			geom->drawArgCount = drawCount;
			geom->indexCount = indexCount;
			geom->vertexCount = vertexCount;
			geom->indexType = (sizeof(uint16_t) == indexStride) ? SG_INDEX_TYPE_UINT16 : SG_INDEX_TYPE_UINT32;
			geom->jointCount = jointCount;

			// Allocate buffer memory
			const bool structuredBuffers = (pDesc->flags & SG_GEOMETRY_LOAD_FLAG_STRUCTURED_BUFFERS);

			// Index buffer
			BufferCreateDesc indexBufferDesc = {};
			indexBufferDesc.descriptors = SG_DESCRIPTOR_TYPE_INDEX_BUFFER |
				(structuredBuffers ?
					(SG_DESCRIPTOR_TYPE_BUFFER | SG_DESCRIPTOR_TYPE_RW_BUFFER) :
					(SG_DESCRIPTOR_TYPE_BUFFER_RAW | SG_DESCRIPTOR_TYPE_RW_BUFFER_RAW));
			indexBufferDesc.size = indexStride * indexCount;
			indexBufferDesc.elementCount = indexBufferDesc.size / (structuredBuffers ? indexStride : sizeof(uint32_t));
			indexBufferDesc.structStride = indexStride;
			indexBufferDesc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_GPU_ONLY;
			add_buffer(pRenderer, &indexBufferDesc, &geom->pIndexBuffer);

			BufferUpdateDesc indexUpdateDesc = {};
			BufferUpdateDesc vertexUpdateDesc[SG_MAX_VERTEX_BINDINGS] = {};

			indexUpdateDesc.size = indexCount * indexStride;
			indexUpdateDesc.pBuffer = geom->pIndexBuffer;
	#if UMA
			indexUpdateDesc.mInternal.mappedRange = { (uint8_t*)geom->pIndexBuffer->pCpuMappedAddress };
	#else
			indexUpdateDesc.mInternal.mappedRange = allocate_staging_memory(indexUpdateDesc.size, SG_RESOURCE_BUFFER_ALIGNMENT);
	#endif
			indexUpdateDesc.pMappedData = indexUpdateDesc.mInternal.mappedRange.pData;

			uint32_t bufferCounter = 0;
			for (uint32_t i = 0; i < SG_MAX_VERTEX_BINDINGS; ++i)
			{
				if (!vertexStrides[i])
					continue;

				BufferCreateDesc vertexBufferDesc = {};
				vertexBufferDesc.descriptors = SG_DESCRIPTOR_TYPE_VERTEX_BUFFER |
					(structuredBuffers ?
						(SG_DESCRIPTOR_TYPE_BUFFER | SG_DESCRIPTOR_TYPE_RW_BUFFER) :
						(SG_DESCRIPTOR_TYPE_BUFFER_RAW | SG_DESCRIPTOR_TYPE_RW_BUFFER_RAW));
				vertexBufferDesc.size = vertexStrides[i] * vertexCount;
				vertexBufferDesc.elementCount = vertexBufferDesc.size / (structuredBuffers ? vertexStrides[i] : sizeof(uint32_t));
				vertexBufferDesc.structStride = vertexStrides[i];
				vertexBufferDesc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_GPU_ONLY;
				add_buffer(pRenderer, &vertexBufferDesc, &geom->pVertexBuffers[bufferCounter]);

				geom->vertexStrides[bufferCounter] = vertexStrides[i];

				vertexUpdateDesc[i].pBuffer = geom->pVertexBuffers[bufferCounter];
				vertexUpdateDesc[i].size = vertexBufferDesc.size;
	#if UMA
				vertexUpdateDesc[i].mInternal.mappedRange = { (uint8_t*)geom->pVertexBuffers[bufferCounter]->pCpuMappedAddress, 0 };
#else
				vertexUpdateDesc[i].mInternal.mappedRange = allocate_staging_memory(vertexUpdateDesc[i].size, SG_RESOURCE_BUFFER_ALIGNMENT);
	#endif
				vertexUpdateDesc[i].pMappedData = vertexUpdateDesc[i].mInternal.mappedRange.pData;
				++bufferCounter;
			}

			indexCount = 0;
			vertexCount = 0;
			drawCount = 0;

			for (uint32_t i = 0; i < data->meshes_count; ++i)
			{
				for (uint32_t p = 0; p < data->meshes[i].primitives_count; ++p)
				{
					const cgltf_primitive* prim = &data->meshes[i].primitives[p];
					// Fill index buffer for this primitive
					if (sizeof(uint16_t) == indexStride)
					{
						uint16_t* dst = (uint16_t*)indexUpdateDesc.pMappedData;
						for (uint32_t idx = 0; idx < prim->indices->count; ++idx)
							dst[indexCount + idx] = vertexCount + (uint16_t)cgltf_accessor_read_index(prim->indices, idx);
					}
					else
					{
						uint32_t* dst = (uint32_t*)indexUpdateDesc.pMappedData;
						for (uint32_t idx = 0; idx < prim->indices->count; ++idx)
							dst[indexCount + idx] = vertexCount + (uint32_t)cgltf_accessor_read_index(prim->indices, idx);
					}

					// Fill vertex buffers for this primitive
					for (uint32_t a = 0; a < prim->attributes_count; ++a)
					{
						cgltf_attribute* attr = &prim->attributes[a];
						uint32_t index = util_cgltf_attrib_type_to_shader_semantic(attr->type, attr->index);

						if (vertexOffsets[index] != UINT_MAX)
						{
							const uint32_t binding = vertexBindings[index];
							const uint32_t offset = vertexOffsets[index];
							const uint32_t stride = vertexStrides[binding];
							const uint8_t* src = (uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;

							// If this vertex attribute is not interleaved with any other attribute use fast path instead of copying one by one
							// In this case a simple memcpy will be enough to transfer the data to the buffer
							if (1 == vertexAttribCount[binding])
							{
								uint8_t* dst = (uint8_t*)vertexUpdateDesc[binding].pMappedData + vertexCount * stride;
								if (vertexPacking[index])
									vertexPacking[index]((uint32_t)attr->data->count, (uint32_t)attr->data->stride, 0, src, dst);
								else
									memcpy(dst, src, attr->data->count * attr->data->stride);
							}
							else
							{
								uint8_t* dst = (uint8_t*)vertexUpdateDesc[binding].pMappedData + vertexCount * stride;
								// Loop through all vertices copying into the correct place in the vertex buffer
								// Example:
								// [ POSITION | NORMAL | TEXCOORD ] => [ 0 | 12 | 24 ], [ 32 | 44 | 52 ], ... (vertex stride of 32 => 12 + 12 + 8)
								if (vertexPacking[index])
									vertexPacking[index]((uint32_t)attr->data->count, (uint32_t)attr->data->stride, offset, src, dst);
								else
									for (uint32_t e = 0; e < attr->data->count; ++e)
										memcpy(dst + e * stride + offset, src + e * attr->data->stride, attr->data->stride);
							}
						}
					}

					// Fill draw arguments for this primitive
					geom->pDrawArgs[drawCount].indexCount = (uint32_t)prim->indices->count;
					geom->pDrawArgs[drawCount].instanceCount = 1;
					geom->pDrawArgs[drawCount].startIndex = indexCount;
					geom->pDrawArgs[drawCount].startInstance = 0;
					// Since we already offset indices when creating the index buffer, vertex offset will be zero
					// With this approach, we can draw everything in one draw call or use the traditional draw per subset without the
					// need for changing shader code
					geom->pDrawArgs[drawCount].vertexOffset = 0;

					indexCount += (uint32_t)prim->indices->count;
					vertexCount += (uint32_t)prim->attributes->data->count;
					++drawCount;
				}
			}

			UploadFunctionResult uploadResult = SG_UPLOAD_FUNCTION_RESULT_COMPLETED;
	#if !UMA
			uploadResult = update_buffer(pRenderer, pCopyEngine, activeSet, indexUpdateDesc);

			for (uint32_t i = 0; i < SG_MAX_VERTEX_BINDINGS; ++i)
			{
				if (vertexUpdateDesc[i].pMappedData)
				{
					uploadResult = update_buffer(pRenderer, pCopyEngine, activeSet, vertexUpdateDesc[i]);
				}
			}
	#endif

			// Load the remap joint indices generated in the offline process
			uint32_t remapCount = 0;
			for (uint32_t i = 0; i < data->skins_count; ++i)
			{
				const cgltf_skin* skin = &data->skins[i];
				uint32_t extrasSize = (uint32_t)(skin->extras.end_offset - skin->extras.start_offset);
				if (extrasSize)
				{
					const char* jointRemaps = (const char*)data->json + skin->extras.start_offset;
					jsmn_parser parser = {};
					jsmntok_t* tokens = (jsmntok_t*)sg_malloc((skin->joints_count + 1) * sizeof(jsmntok_t));
					jsmn_parse(&parser, (const char*)jointRemaps, extrasSize, tokens, skin->joints_count + 1);
					ASSERT(tokens[0].size == skin->joints_count + 1);
					cgltf_accessor_unpack_floats(skin->inverse_bind_matrices, (cgltf_float*)geom->pInverseBindPoses, skin->joints_count * sizeof(float[16]) / sizeof(float));
					for (uint32_t r = 0; r < skin->joints_count; ++r)
						geom->pJointRemaps[remapCount + r] = atoi(jointRemaps + tokens[1 + r].start);
					sg_free(tokens);
				}

				remapCount += (uint32_t)skin->joints_count;
			}

			// Load the tressfx specific data generated in the offline process
			if (stricmp(data->asset.generator, "tressfx") == 0)
			{
				// { "mVertexCountPerStrand" : "16", "mGuideCountPerStrand" : "3456" }
				uint32_t extrasSize = (uint32_t)(data->asset.extras.end_offset - data->asset.extras.start_offset);
				const char* json = data->json + data->asset.extras.start_offset;
				jsmn_parser parser = {};
				jsmntok_t tokens[5] = {};
				jsmn_parse(&parser, (const char*)json, extrasSize, tokens, 5);
				geom->hair.vertexCountPerStrand = atoi(json + tokens[2].start);
				geom->hair.guideCountPerStrand = atoi(json + tokens[4].start);
			}

			if (pDesc->flags & SG_GEOMETRY_LOAD_FLAG_SHADOWED)
			{
				indexCount = 0;
				vertexCount = 0;

				for (uint32_t i = 0; i < data->meshes_count; ++i)
				{
					for (uint32_t p = 0; p < data->meshes[i].primitives_count; ++p)
					{
						const cgltf_primitive* prim = &data->meshes[i].primitives[p];

						// Fill index buffer for this primitive
						if (sizeof(uint16_t) == indexStride)
						{
							uint16_t* dst = (uint16_t*)geom->pShadow->pIndices;
							for (uint32_t idx = 0; idx < prim->indices->count; ++idx)
								dst[indexCount + idx] = vertexCount + (uint16_t)cgltf_accessor_read_index(prim->indices, idx);
						}
						else
						{
							uint32_t* dst = (uint32_t*)geom->pShadow->pIndices;
							for (uint32_t idx = 0; idx < prim->indices->count; ++idx)
								dst[indexCount + idx] = vertexCount + (uint32_t)cgltf_accessor_read_index(prim->indices, idx);
						}

						for (uint32_t a = 0; a < prim->attributes_count; ++a)
						{
							cgltf_attribute* attr = &prim->attributes[a];
							if (cgltf_attribute_type_position == attr->type)
							{
								const uint8_t* src = (uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
								uint8_t* dst = (uint8_t*)geom->pShadow->pAttributes[SG_SEMANTIC_POSITION] + vertexCount * attr->data->stride;
								memcpy(dst, src, attr->data->count * attr->data->stride);
							}
						}

						indexCount += (uint32_t)prim->indices->count;
						vertexCount += (uint32_t)prim->attributes->data->count;
					}
				}
			}

			data->file_data = fileData;
			cgltf_free(data);

			sg_free(pDesc->pVertexLayout);

			*pDesc->ppGeometry = geom;

			return uploadResult;
		}

		return SG_UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
	}

	// internal Resource Loader Implementation
	// to check if there are some loading operation is functioning
	static bool are_tasks_available(ResourceLoader* pLoader)
	{
		for (size_t i = 0; i < SG_MAX_LINKED_GPUS; ++i)
		{
			if (!pLoader->requestQueue[i].empty())
			{
				return true;
			}
		}
		return false;
	}

	// for each thread to load the data
	static void streamer_thread_func(void* pThreadData)
	{
		Thread::set_curr_thread_name("ResourceLoading");
		SG_LOG_DEBUG("Thread ID: %ul is loading resourece", Thread::get_curr_thread_id());

		ResourceLoader* pLoader = (ResourceLoader*)pThreadData;
		ASSERT(pLoader);

	#if defined(SG_GRAPHIC_API_GLES)
		GLContext localContext;
		if (!pLoader->desc.mSingleThreaded)
			initGLContext(pLoader->pRenderer->pConfig, &localContext, pLoader->pRenderer->pContext);
	#endif

		uint32_t linkedGPUCount = pLoader->pRenderer->linkedNodeCount;

		SyncToken maxToken = {};

		while (pLoader->run)
		{
			{
				MutexLock lck(pLoader->queueMutex);
				//pLoader->queueMutex.Acquire();

				// check for pending tokens
				// safe to use tokenCounter as we are inside critical section

				// we are in the beginning of a new round
				bool allTokensSignaled = (pLoader->tokenCompleted == sg_atomic64_load_relaxed(&pLoader->tokenCounter));

				while (!are_tasks_available(pLoader) && allTokensSignaled && pLoader->run)
				{
					// no waiting if not running dedicated resource loader thread.
					if (pLoader->desc.singleThreaded)
					{
						return;
					}
					// sleep until someone adds an update request to the queue
					pLoader->queueCv.Wait(pLoader->queueMutex);
				}
				//pLoader->queueMutex.Release();
			}

			pLoader->nextSet = (pLoader->nextSet + 1) % pLoader->desc.bufferCount;
			for (uint32_t nodeIndex = 0; nodeIndex < linkedGPUCount; ++nodeIndex)
			{
				// wait for the transfer queue to do the next job
				wait_copy_engine_set(pLoader->pRenderer, &pLoader->pCopyEngines[nodeIndex], pLoader->nextSet, true);
				reset_copy_engine_set(pLoader->pRenderer, &pLoader->pCopyEngines[nodeIndex], pLoader->nextSet);
			}

			// signal pending tokens from previous frames
			pLoader->tokenMutex.Acquire();
			sg_atomic64_store_release(&pLoader->tokenCompleted, pLoader->currentTokenState[pLoader->nextSet]);
			pLoader->tokenMutex.Release();
			pLoader->tokenCv.WakeAll();

			for (uint32_t nodeIndex = 0; nodeIndex < linkedGPUCount; ++nodeIndex)
			{
				uint64_t completionMask = 0;

				pLoader->queueMutex.Acquire();

				eastl::vector<UpdateRequest>& requestQueue = pLoader->requestQueue[nodeIndex];
				CopyEngine& copyEngine = pLoader->pCopyEngines[nodeIndex];

				if (requestQueue.empty()) // no job to do
				{
					pLoader->queueMutex.Release();
					continue;
				}

				eastl::vector<UpdateRequest> activeQueue;
				eastl::swap(requestQueue, activeQueue);
				pLoader->queueMutex.Release();

				size_t requestCount = activeQueue.size();
				for (size_t j = 0; j < requestCount; ++j)
				{
					UpdateRequest updateState = activeQueue[j];

					UploadFunctionResult result = SG_UPLOAD_FUNCTION_RESULT_COMPLETED;
					switch (updateState.type)
					{
					case SG_UPDATE_REQUEST_UPDATE_BUFFER:
						result = update_buffer(pLoader->pRenderer, &copyEngine, pLoader->nextSet, updateState.bufferUpdateDesc);
						break;
					case SG_UPDATE_REQUEST_UPDATE_TEXTURE:
						result = update_texture(pLoader->pRenderer, &copyEngine, pLoader->nextSet, updateState.texUpdateDesc);
						break;
					case SG_UPDATE_REQUEST_BUFFER_BARRIER:
						cmd_resource_barrier(acquire_cmd(&copyEngine, pLoader->nextSet), 1, &updateState.bufferBarrier, 0, nullptr, 0, nullptr);
						result = SG_UPLOAD_FUNCTION_RESULT_COMPLETED;
						break;
					case SG_UPDATE_REQUEST_TEXTURE_BARRIER:
						cmd_resource_barrier(acquire_cmd(&copyEngine, pLoader->nextSet), 0, nullptr, 1, &updateState.textureBarrier, 0, nullptr);
						result = SG_UPLOAD_FUNCTION_RESULT_COMPLETED;
						break;
					case SG_UPDATE_REQUEST_LOAD_TEXTURE:
						result = load_texture(pLoader->pRenderer, &copyEngine, pLoader->nextSet, updateState);
						break;
					case SG_UPDATE_REQUEST_LOAD_GEOMETRY:
						result = load_geometry(pLoader->pRenderer, &copyEngine, pLoader->nextSet, updateState);
						break;
					case SG_UPDATE_REQUEST_INVALID:
						break;
					}

					if (updateState.pUploadBuffer)
					{
						CopyResourceSet& resourceSet = copyEngine.resourceSets[pLoader->nextSet];
						resourceSet.tempBuffers.push_back(updateState.pUploadBuffer);
					}

					bool completed = result == SG_UPLOAD_FUNCTION_RESULT_COMPLETED || result == SG_UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;

					completionMask |= completed << nodeIndex;

					if (updateState.waitIndex && completed)
					{
						ASSERT(maxToken < updateState.waitIndex);
						maxToken = updateState.waitIndex;
					}

					ASSERT(result != SG_UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL);
				}

				if (completionMask != 0)
				{
					for (uint32_t nodeIndex = 0; nodeIndex < linkedGPUCount; ++nodeIndex)
					{
						// commands recording complete
						flush_cmd(&pLoader->pCopyEngines[nodeIndex], pLoader->nextSet);
					}
				}
			}

			SyncToken nextToken = eastl::max(maxToken, get_last_token_completed());
			pLoader->currentTokenState[pLoader->nextSet] = nextToken;
			if (pResourceLoader->desc.singleThreaded)
			{
				return;
			}
		}

		for (uint32_t nodeIndex = 0; nodeIndex < linkedGPUCount; ++nodeIndex)
		{
			flush_cmd(&pLoader->pCopyEngines[nodeIndex], pLoader->nextSet);
	#if !defined(SG_GRAPHIC_API_D3D11) && !defined(SG_GRAPHIC_API_GLES)
			wait_queue_idle(pLoader->pCopyEngines[nodeIndex].pQueue);
	#endif
			cleanup_copy_engine(pLoader->pRenderer, &pLoader->pCopyEngines[nodeIndex]);
		}

		free_all_upload_memory();

	#if defined(GLES)
		if (!pResourceLoader->desc.mSingleThreaded)
			removeGLContext(&localContext);
	#endif
	}

	// init function
	static void add_resource_loader(Renderer* pRenderer, ResourceLoaderDesc* pDesc, ResourceLoader** ppLoader)
	{
		ResourceLoader* pLoader = sg_new(ResourceLoader);
		pLoader->pRenderer = pRenderer;

		pLoader->run = true;
		pLoader->desc = pDesc ? *pDesc : gDefaultResourceLoaderDesc;

		pLoader->queueMutex.Init();
		pLoader->tokenMutex.Init();
		pLoader->queueCv.Init();
		pLoader->tokenCv.Init();

		pLoader->tokenCounter = 0;
		pLoader->tokenCompleted = 0;

		uint32_t linkedGPUCount = pLoader->pRenderer->linkedNodeCount;
		for (uint32_t i = 0; i < linkedGPUCount; ++i)
		{
			setup_copy_engine(pLoader->pRenderer, &pLoader->pCopyEngines[i], i, pLoader->desc.bufferSize, pLoader->desc.bufferCount);
		}

		pLoader->threadDesc.pFunc = streamer_thread_func;
		pLoader->threadDesc.pData = pLoader;

	#if defined(NX64)
		pLoader->mThreadDesc.pThreadStack = aligned_alloc(THREAD_STACK_ALIGNMENT_NX, ALIGNED_THREAD_STACK_SIZE_NX);
		pLoader->mThreadDesc.hThread = &pLoader->mThreadType;
		pLoader->mThreadDesc.pThreadName = "ResourceLoaderTask";
		pLoader->mThreadDesc.preferredCore = 1;
	#endif

	#if defined(SG_GRAPHIC_API_D3D11)
		pLoader->desc.singleThreaded = true;
	#endif

		// create dedicated resource loader thread.
		if (!pLoader->desc.singleThreaded)
		{
			pLoader->mThread = create_thread(&pLoader->threadDesc);
		}

		*ppLoader = pLoader;
	}

	// exit function
	static void remove_resource_loader(ResourceLoader* pLoader)
	{
		pLoader->run = false;

		if (pLoader->desc.singleThreaded)
		{
			streamer_thread_func(pLoader);
		}
		else
		{
			pLoader->queueCv.WakeOne();
			destroy_thread(pLoader->mThread);
		}

		pLoader->queueCv.Destroy();
		pLoader->tokenCv.Destroy();
		pLoader->queueMutex.Destroy();
		pLoader->tokenMutex.Destroy();

		sg_delete(pLoader);
		pLoader = nullptr;
	}

	static void queue_buffer_update(ResourceLoader* pLoader, BufferUpdateDesc* pBufferUpdate, SyncToken* token)
	{
		uint32_t nodeIndex = pBufferUpdate->pBuffer->nodeIndex;
		pLoader->queueMutex.Acquire();

		// add one
		SyncToken t = sg_atomic64_add_relaxed(&pLoader->tokenCounter, 1) + 1;

		pLoader->requestQueue[nodeIndex].emplace_back(UpdateRequest(*pBufferUpdate));
		pLoader->requestQueue[nodeIndex].back().waitIndex = t;
		pLoader->requestQueue[nodeIndex].back().pUploadBuffer =
			(pBufferUpdate->mInternal.mappedRange.flags & MAPPED_RANGE_FLAG_TEMP_BUFFER) ? pBufferUpdate->mInternal.mappedRange.pBuffer
			: nullptr;

		pLoader->queueMutex.Release();
		// update finish, one thread can go and get the job
		pLoader->queueCv.WakeOne();
		if (token) *token = eastl::max(t, *token);
	}

	static void queue_texture_load(ResourceLoader* pLoader, TextureLoadDesc* pTextureUpdate, SyncToken* token)
	{
		uint32_t nodeIndex = pTextureUpdate->nodeIndex;
		pLoader->queueMutex.Acquire();

		SyncToken t = sg_atomic64_add_relaxed(&pLoader->tokenCounter, 1) + 1;

		pLoader->requestQueue[nodeIndex].emplace_back(UpdateRequest(*pTextureUpdate));
		pLoader->requestQueue[nodeIndex].back().waitIndex = t;
		pLoader->queueMutex.Release();
		pLoader->queueCv.WakeOne();
		if (token) *token = eastl::max(t, *token);
	}

	static void queue_geometry_load(ResourceLoader* pLoader, GeometryLoadDesc* pGeometryLoad, SyncToken* token)
	{
		uint32_t nodeIndex = pGeometryLoad->nodeIndex;
		pLoader->queueMutex.Acquire();

		SyncToken t = sg_atomic64_add_relaxed(&pLoader->tokenCounter, 1) + 1;

		pLoader->requestQueue[nodeIndex].emplace_back(UpdateRequest(*pGeometryLoad));
		pLoader->requestQueue[nodeIndex].back().waitIndex = t;
		pLoader->queueMutex.Release();
		pLoader->queueCv.WakeOne();
		if (token) *token = eastl::max(t, *token);
	}

	static void queue_texture_update(ResourceLoader* pLoader, TextureUpdateDescInternal* pTextureUpdate, SyncToken* token)
	{
		ASSERT(pTextureUpdate->range.pBuffer);

		uint32_t nodeIndex = pTextureUpdate->pTexture->nodeIndex;
		pLoader->queueMutex.Acquire();

		SyncToken t = sg_atomic64_add_relaxed(&pLoader->tokenCounter, 1) + 1;

		pLoader->requestQueue[nodeIndex].emplace_back(UpdateRequest(*pTextureUpdate));
		pLoader->requestQueue[nodeIndex].back().waitIndex = t;
		pLoader->requestQueue[nodeIndex].back().pUploadBuffer =
			(pTextureUpdate->range.flags & MAPPED_RANGE_FLAG_TEMP_BUFFER) ? pTextureUpdate->range.pBuffer : nullptr;
		pLoader->queueMutex.Release();
		pLoader->queueCv.WakeOne();
		if (token) *token = eastl::max(t, *token);
	}

	static void queue_buffer_barrier(ResourceLoader* pLoader, Buffer* pBuffer, ResourceState state, SyncToken* token)
	{
		uint32_t nodeIndex = pBuffer->nodeIndex;
		pLoader->queueMutex.Acquire();

		SyncToken t = sg_atomic64_add_relaxed(&pLoader->tokenCounter, 1) + 1;

		pLoader->requestQueue[nodeIndex].emplace_back(UpdateRequest{ BufferBarrier{ pBuffer, SG_RESOURCE_STATE_UNDEFINED, state } });
		pLoader->requestQueue[nodeIndex].back().waitIndex = t;
		pLoader->queueMutex.Release();
		pLoader->queueCv.WakeOne();
		if (token) *token = eastl::max(t, *token);
	}

	static void queue_texture_barrier(ResourceLoader* pLoader, Texture* pTexture, ResourceState state, SyncToken* token)
	{
		uint32_t nodeIndex = pTexture->nodeIndex;
		pLoader->queueMutex.Acquire();

		SyncToken t = sg_atomic64_add_relaxed(&pLoader->tokenCounter, 1) + 1;

		pLoader->requestQueue[nodeIndex].emplace_back(UpdateRequest{ TextureBarrier{ pTexture, SG_RESOURCE_STATE_UNDEFINED, state } });
		pLoader->requestQueue[nodeIndex].back().waitIndex = t;
		pLoader->queueMutex.Release();
		pLoader->queueCv.WakeOne();
		if (token) *token = eastl::max(t, *token);
	}

	static void wait_for_token(ResourceLoader* pLoader, const SyncToken* token)
	{
		if (pLoader->desc.singleThreaded)
		{
			return;
		}
		pLoader->tokenMutex.Acquire();
		while (!is_token_completed(token))
		{
			pLoader->tokenCv.Wait(pLoader->tokenMutex);
		}
		pLoader->tokenMutex.Release();
	}

#pragma region (Interface Implemetation)

	void init_resource_loader_interface(Renderer* pRenderer, ResourceLoaderDesc* pDesc)
	{
		add_resource_loader(pRenderer, pDesc, &pResourceLoader);
	}

	void exit_resource_loader_interface(Renderer* pRenderer)
	{
		remove_resource_loader(pResourceLoader);
	}

	void add_resource(BufferLoadDesc* pBufferDesc, SyncToken* token)
	{
		uint64_t stagingBufferSize = pResourceLoader->pCopyEngines[0].bufferSize;
		bool update = pBufferDesc->pData || pBufferDesc->forceReset;

		ASSERT(stagingBufferSize > 0);
		if (SG_RESOURCE_MEMORY_USAGE_GPU_ONLY == pBufferDesc->desc.memoryUsage && !pBufferDesc->desc.startState && !update)
		{
			pBufferDesc->desc.startState = util_determine_resource_start_state(&pBufferDesc->desc);
			SG_LOG_WARNING("Buffer start state not provided. Determined the start state as (%u) based on the provided BufferCreateDesc",
				(uint32_t)pBufferDesc->desc.startState);
		}

		if (pBufferDesc->desc.memoryUsage == SG_RESOURCE_MEMORY_USAGE_GPU_ONLY && update)
		{
			pBufferDesc->desc.startState = SG_RESOURCE_STATE_COMMON;
		}
		add_buffer(pResourceLoader->pRenderer, &pBufferDesc->desc, pBufferDesc->ppBuffer);

		if (update)
		{
			if (pBufferDesc->desc.size > stagingBufferSize &&
				pBufferDesc->desc.memoryUsage == SG_RESOURCE_MEMORY_USAGE_GPU_ONLY)
			{
				// the data is too large for a single staging buffer copy, so perform it in stages.
				// save the data parameter so we can restore it later.
				const void* data = pBufferDesc->pData;

				BufferUpdateDesc updateDesc = {};
				updateDesc.pBuffer = *pBufferDesc->ppBuffer;
				for (uint64_t offset = 0; offset < pBufferDesc->desc.size; offset += stagingBufferSize)
				{
					size_t chunkSize = (size_t)eastl::min(stagingBufferSize, pBufferDesc->desc.size - offset);
					updateDesc.size = chunkSize;
					updateDesc.dstOffset = offset;
					begin_update_resource(&updateDesc);
					if (pBufferDesc->forceReset)
					{
						memset(updateDesc.pMappedData, 0, chunkSize);
					}
					else
					{
						memcpy(updateDesc.pMappedData, (char*)data + offset, chunkSize);
					}
					end_update_resource(&updateDesc, token);
				}
			}
			else
			{
				BufferUpdateDesc updateDesc = {};
				updateDesc.pBuffer = *pBufferDesc->ppBuffer;
				begin_update_resource(&updateDesc);
				if (pBufferDesc->forceReset)
				{
					memset(updateDesc.pMappedData, 0, (size_t)pBufferDesc->desc.size);
				}
				else
				{
					memcpy(updateDesc.pMappedData, pBufferDesc->pData, (size_t)pBufferDesc->desc.size);
				}
				end_update_resource(&updateDesc, token);
			}
		}
		else
		{
			// transition GPU buffer to desired state for Vulkan since all Vulkan resources are created in undefined state
			if (pResourceLoader->pRenderer->api == SG_RENDERER_API_VULKAN &&
				pBufferDesc->desc.memoryUsage == SG_RESOURCE_MEMORY_USAGE_GPU_ONLY &&
				// check whether this is required (user specified a state other than undefined / common)
				(pBufferDesc->desc.startState != SG_RESOURCE_STATE_UNDEFINED && pBufferDesc->desc.startState != SG_RESOURCE_STATE_COMMON))
				queue_buffer_barrier(pResourceLoader, *pBufferDesc->ppBuffer, pBufferDesc->desc.startState, token);
		}
	}

	void add_resource(TextureLoadDesc* pTextureDesc, SyncToken* token)
	{
		ASSERT(pTextureDesc->ppTexture);

		if (!pTextureDesc->fileName && pTextureDesc->desc)
		{
			ASSERT(pTextureDesc->desc->startState);

			// If texture is supposed to be filled later (UAV / Update later / ...) proceed with the startState provided by the user in the texture description
			add_texture(pResourceLoader->pRenderer, pTextureDesc->desc, pTextureDesc->ppTexture);

			// Transition texture to desired state for Vulkan since all Vulkan resources are created in undefined state
			if (pResourceLoader->pRenderer->api == SG_RENDERER_API_VULKAN)
			{
				ResourceState startState = pTextureDesc->desc->startState;
				// Check whether this is required (user specified a state other than undefined / common)
				if (startState == SG_RESOURCE_STATE_UNDEFINED || startState == SG_RESOURCE_STATE_COMMON)
				{
					startState = util_determine_resource_start_state(pTextureDesc->desc->descriptors & SG_DESCRIPTOR_TYPE_RW_TEXTURE);
				}
				queue_texture_barrier(pResourceLoader, *pTextureDesc->ppTexture, startState, token);
			}
		}
		else
		{
			TextureLoadDesc updateDesc = *pTextureDesc;
			queue_texture_load(pResourceLoader, &updateDesc, token);
			if (pResourceLoader->desc.singleThreaded)
			{
				streamer_thread_func(pResourceLoader);
			}
		}
	}

	void add_resource(GeometryLoadDesc* pDesc, SyncToken* token)
	{
		ASSERT(pDesc->ppGeometry);

		GeometryLoadDesc updateDesc = *pDesc;
		updateDesc.fileName = pDesc->fileName;
		updateDesc.pVertexLayout = (VertexLayout*)sg_calloc(1, sizeof(VertexLayout));
		memcpy(updateDesc.pVertexLayout, pDesc->pVertexLayout, sizeof(VertexLayout));
		queue_geometry_load(pResourceLoader, &updateDesc, token);
		if (pResourceLoader->desc.singleThreaded)
		{
			streamer_thread_func(pResourceLoader);
		}
	}

	void remove_resource(Buffer* pBuffer)
	{
		remove_buffer(pResourceLoader->pRenderer, pBuffer);
	}

	void remove_resource(Texture* pTexture)
	{
		remove_texture(pResourceLoader->pRenderer, pTexture);
	}

	void remove_resource(Geometry* pGeom)
	{
		remove_resource(pGeom->pIndexBuffer);

		for (uint32_t i = 0; i < pGeom->vertexBufferCount; ++i)
			remove_resource(pGeom->pVertexBuffers[i]);

		sg_free(pGeom);
	}

	void begin_update_resource(BufferUpdateDesc* pBufferUpdate)
	{
		Buffer* pBuffer = pBufferUpdate->pBuffer;
		ASSERT(pBuffer);

		uint64_t size = pBufferUpdate->size > 0 ? pBufferUpdate->size : (pBufferUpdate->pBuffer->size - pBufferUpdate->dstOffset);
		ASSERT(pBufferUpdate->dstOffset + size <= pBuffer->size);

		ResourceMemoryUsage memoryUsage = (ResourceMemoryUsage)pBufferUpdate->pBuffer->memoryUsage;
		if (UMA || memoryUsage != SG_RESOURCE_MEMORY_USAGE_GPU_ONLY)
		{
			bool map = !pBuffer->pCpuMappedAddress;
			if (map)
			{
				map_buffer(pResourceLoader->pRenderer, pBuffer, nullptr);
			}

			pBufferUpdate->mInternal.mappedRange = { (uint8_t*)pBuffer->pCpuMappedAddress + pBufferUpdate->dstOffset, pBuffer };
			pBufferUpdate->pMappedData = pBufferUpdate->mInternal.mappedRange.pData;
			pBufferUpdate->mInternal.mappedRange.flags = map ? MAPPED_RANGE_FLAG_UNMAP_BUFFER : 0;
		}
		else
		{
			// we need to use a staging buffer.
			MappedMemoryRange range = allocate_upload_memory(pResourceLoader->pRenderer, size, SG_RESOURCE_BUFFER_ALIGNMENT);
			pBufferUpdate->pMappedData = range.pData;

			pBufferUpdate->mInternal.mappedRange = range;
			pBufferUpdate->mInternal.mappedRange.flags = MAPPED_RANGE_FLAG_TEMP_BUFFER;
		}
	}

	void end_update_resource(BufferUpdateDesc* pBufferUpdate, SyncToken* token)
	{
		if (pBufferUpdate->mInternal.mappedRange.flags & MAPPED_RANGE_FLAG_UNMAP_BUFFER)
		{
			unmap_buffer(pResourceLoader->pRenderer, pBufferUpdate->pBuffer);
		}

		ResourceMemoryUsage memoryUsage = (ResourceMemoryUsage)pBufferUpdate->pBuffer->memoryUsage;
		if (!UMA && memoryUsage == SG_RESOURCE_MEMORY_USAGE_GPU_ONLY)
		{
			queue_buffer_update(pResourceLoader, pBufferUpdate, token);
		}

		// Restore the state to before the beginUpdateResource call.
		pBufferUpdate->pMappedData = nullptr;
		pBufferUpdate->mInternal = {};
		if (pResourceLoader->desc.singleThreaded)
		{
			streamer_thread_func(pResourceLoader);
		}
	}

	void begin_update_resource(TextureUpdateDesc* pTextureUpdate)
	{
		const Texture* texture = pTextureUpdate->pTexture;
		const TinyImageFormat fmt = (TinyImageFormat)texture->format;
		const uint32_t alignment = util_get_texture_subresource_alignment(pResourceLoader->pRenderer, fmt);

		bool success = util_get_surface_info(
			SG_MIP_REDUCE(texture->width, pTextureUpdate->mipLevel),
			SG_MIP_REDUCE(texture->height, pTextureUpdate->mipLevel),
			fmt,
			&pTextureUpdate->srcSliceStride,
			&pTextureUpdate->srcRowStride,
			&pTextureUpdate->rowCount);
		ASSERT(success);
		UNREF_PARAM(success);

		pTextureUpdate->dstRowStride = round_up(pTextureUpdate->srcRowStride, util_get_texture_row_alignment(pResourceLoader->pRenderer));
		pTextureUpdate->dstSliceStride = round_up(pTextureUpdate->dstRowStride * pTextureUpdate->rowCount, alignment);

		const ssize_t requiredSize = round_up(
			SG_MIP_REDUCE(texture->depth, pTextureUpdate->mipLevel) * pTextureUpdate->dstRowStride * pTextureUpdate->rowCount,
			alignment);

		// We need to use a staging buffer.
		pTextureUpdate->mInternal.mappedRange = allocate_upload_memory(pResourceLoader->pRenderer, requiredSize, alignment);
		pTextureUpdate->mInternal.mappedRange.flags = MAPPED_RANGE_FLAG_TEMP_BUFFER;
		pTextureUpdate->pMappedData = pTextureUpdate->mInternal.mappedRange.pData;
	}

	void end_update_resource(TextureUpdateDesc* pTextureUpdate, SyncToken* token)
	{
		TextureUpdateDescInternal desc = {};
		desc.pTexture = pTextureUpdate->pTexture;
		desc.range = pTextureUpdate->mInternal.mappedRange;
		desc.baseMipLevel = pTextureUpdate->mipLevel;
		desc.mipLevels = 1;
		desc.baseArrayLayer = pTextureUpdate->arrayLayer;
		desc.layerCount = 1;
		queue_texture_update(pResourceLoader, &desc, token);

		// Restore the state to before the beginUpdateResource call.
		pTextureUpdate->pMappedData = NULL;
		pTextureUpdate->mInternal = {};
		if (pResourceLoader->desc.singleThreaded)
		{
			streamer_thread_func(pResourceLoader);
		}
	}

	SyncToken get_last_token_completed()
	{
		return sg_atomic64_load_acquire(&pResourceLoader->tokenCompleted);
	}

	bool is_token_completed(const SyncToken* token)
	{
		return *token <= sg_atomic64_load_acquire(&pResourceLoader->tokenCompleted);
	}

	void wait_for_token(const SyncToken* token)
	{
		wait_for_token(pResourceLoader, token);
	}

	bool all_resource_loads_completed()
	{
		SyncToken token = sg_atomic64_load_relaxed(&pResourceLoader->tokenCounter);
		return token <= sg_atomic64_load_acquire(&pResourceLoader->tokenCompleted);
	}

	void wait_for_all_resource_loads()
	{
		SyncToken token = sg_atomic64_load_relaxed(&pResourceLoader->tokenCounter);
		wait_for_token(pResourceLoader, &token);
	}

#pragma endregion (Interface Implemetation)

	// Shader loading
	#if defined(__ANDROID__) && defined(SG_GRAPHIC_API_VULKAN)
	// Translate Vulkan Shader Type to shaderc shader type
	shaderc_shader_kind getShadercShaderType(ShaderStage type)
	{
		switch (type)
		{
		case ShaderStage::SHADER_STAGE_VERT: return shaderc_glsl_vertex_shader;
		case ShaderStage::SHADER_STAGE_FRAG: return shaderc_glsl_fragment_shader;
		case ShaderStage::SHADER_STAGE_TESC: return shaderc_glsl_tess_control_shader;
		case ShaderStage::SHADER_STAGE_TESE: return shaderc_glsl_tess_evaluation_shader;
		case ShaderStage::SHADER_STAGE_GEOM: return shaderc_glsl_geometry_shader;
		case ShaderStage::SHADER_STAGE_COMP: return shaderc_glsl_compute_shader;
		default: break;
		}

		ASSERT(false);
		return static_cast<shaderc_shader_kind>(-1);
	}
	#endif

	#if defined(SG_GRAPHIC_API_VULKAN)
		#if defined(__ANDROID__)
		// Android:
		// Use shaderc to compile glsl to spirV
		void vk_compileShader(
			Renderer* pRenderer, ShaderStage stage, uint32_t codeSize, const char* code, const char* outFile, uint32_t macroCount,
			ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint)
		{
			// compile into spir-V shader
			shaderc_compiler_t           compiler = shaderc_compiler_initialize();
			shaderc_compile_options_t	 options = shaderc_compile_options_initialize();
			for (uint32_t i = 0; i < macroCount; ++i)
			{
				shaderc_compile_options_add_macro_definition(options, pMacros[i].definition, strlen(pMacros[i].definition),
					pMacros[i].value, strlen(pMacros[i].value));
			}

			eastl::string android_definition = "TARGET_ANDROID";
			shaderc_compile_options_add_macro_definition(options, android_definition.c_str(), android_definition.size(), "1", 1);

			shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);

			shaderc_compilation_result_t spvShader =
				shaderc_compile_into_spv(compiler, code, codeSize, getShadercShaderType(stage), "shaderc_error", pEntryPoint ? pEntryPoint : "main", options);
			shaderc_compilation_status spvStatus = shaderc_result_get_compilation_status(spvShader);
			if (spvStatus != shaderc_compilation_status_success)
			{
				const char* errorMessage = shaderc_result_get_error_message(spvShader);
				LOGF(LogLevel::eERROR, "Shader compiling failed! with status %s", errorMessage);
				abort();
			}

			// Resize the byteCode block based on the compiled shader size
			pOut->mByteCodeSize = shaderc_result_get_length(spvShader);
			pOut->pByteCode = tf_malloc(pOut->mByteCodeSize);
			memcpy(pOut->pByteCode, shaderc_result_get_bytes(spvShader), pOut->mByteCodeSize);

			// Release resources
			shaderc_result_release(spvShader);
			shaderc_compiler_release(compiler);
		}
		#else
		// PC:
		// Vulkan has no builtin functions to compile source to spirv
		// So we call the glslangValidator tool located inside VulkanSDK on user machine to compile the glsl code to spirv
		// This code is not added to Vulkan.cpp since it calls no Vulkan specific functions
		void vk_compile_shader(
			Renderer* pRenderer, ShaderTarget target, ShaderStage stage, const char* fileName, const char* outFile, uint32_t macroCount,
			ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint)
		{
			eastl::string commandLine;
			char filePath[SG_MAX_FILEPATH] = { 0 };
			sgfs_append_path_component(sgfs_get_resource_directory(SG_RD_SHADER_SOURCES), fileName, filePath);
			char outFilePath[SG_MAX_FILEPATH] = { 0 };
			sgfs_append_path_component(sgfs_get_resource_directory(SG_RD_SHADER_BINARIES), outFile, outFilePath);

			// if there is a config file located in the shader source directory use it to specify the limits
			FileStream confStream = {};
			if (sgfs_open_stream_from_path(SG_RD_SHADER_SOURCES, "config.conf", SG_FM_READ, &confStream))
			{
				sgfs_close_stream(&confStream);
				char configFilePath[SG_MAX_FILEPATH] = { 0 };
				sgfs_append_path_component(sgfs_get_resource_directory(SG_RD_SHADER_SOURCES), "config.conf", configFilePath);

				// dd command to compile from Vulkan GLSL to Spirv
				commandLine.append_sprintf(
					"\"%s\" -V \"%s\" -o \"%s\"",
					configFilePath,
					filePath,
					outFilePath);
			}
			else
			{
				commandLine.append_sprintf("-V \"%s\" -o \"%s\"",
					filePath,
					outFilePath);
			}

			if (target >= SG_SHADER_TARGET_6_0)
				commandLine += " --target-env vulkan1.1 ";
			//commandLine += " \"-D" + eastl::string("VULKAN") + "=" + "1" + "\"";

			if (pEntryPoint != nullptr)
				commandLine.append_sprintf(" -e %s", pEntryPoint);

			// add platform macro
		#ifdef SG_PLATFORM_WINDOWS
			commandLine += " \"-D WINDOWS\"";
		#elif defined(__ANDROID__)
			commandLine += " \"-D ANDROID\"";
		#elif defined(__linux__)
			commandLine += " \"-D LINUX\"";
		#endif

			// add user defined macros to the command line
			for (uint32_t i = 0; i < macroCount; ++i)
			{
				commandLine += " \"-D" + eastl::string(pMacros[i].definition) + "=" + pMacros[i].value + "\"";
			}

			eastl::string glslangValidator = ::getenv("VULKAN_SDK");
			if (glslangValidator.size())
				glslangValidator += "/Bin/glslangValidator";
			else
				glslangValidator = "/usr/Bin/glslangValidator";
			
			const char* args[1] = { commandLine.c_str() };

			char logFileName[SG_MAX_FILEPATH] = { 0 };
			sgfs_get_path_file_name(outFile, logFileName);
			strcat(logFileName, "_compile.log");

			char logFilePath[SG_MAX_FILEPATH] = { 0 };
			sgfs_append_path_component(sgfs_get_resource_directory(SG_RD_SHADER_BINARIES), logFileName, logFilePath);

			if (system_run(glslangValidator.c_str(), args, 1, logFilePath) == 0)
			{
				FileStream fh = {};
				bool success = sgfs_open_stream_from_path(SG_RD_SHADER_BINARIES, outFile, SG_FM_READ_BINARY, &fh);
				//Check if the File Handle exists
				ASSERT(success);
				pOut->byteCodeSize = (uint32_t)sgfs_get_stream_file_size(&fh);
				pOut->pByteCode = sg_malloc(pOut->byteCodeSize);
				sgfs_read_from_stream(&fh, pOut->pByteCode, pOut->byteCodeSize);
				sgfs_close_stream(&fh);
			}
			else
			{
				FileStream fh = {};
				// If for some reason the error file could not be created just log error msg
				if (!sgfs_open_stream_from_path(SG_RD_SHADER_BINARIES, logFileName, SG_FM_READ_BINARY, &fh))
				{
					SG_LOG_ERROR("Failed to compile shader %s", filePath);
				}
				else
				{
					size_t size = sgfs_get_stream_file_size(&fh);
					if (size)
					{
						char* errorLog = (char*)sg_malloc(size + 1);
						errorLog[size] = 0;
						sgfs_read_from_stream(&fh, errorLog, size);
						SG_LOG_ERROR("Failed to compile shader %s with error\n%s", filePath, errorLog);
					}
					sgfs_close_stream(&fh);
				}
			}
		}
		#endif
		#elif defined(SG_GRAPHIC_API_METAL)
		// On Metal, on the other hand, we can compile from code into a MTLLibrary, but cannot save this
		// object's bytecode to disk. We instead use the xcbuild bash tool to compile the shaders.
		void mtl_compileShader(
			Renderer* pRenderer, const char* fileName, const char* outFile, uint32_t macroCount, ShaderMacro* pMacros,
			BinaryShaderStageDesc* pOut, const char* /*pEntryPoint*/)
		{
			char filePath[FS_MAX_PATH] = {};
			fsAppendPathComponent(fsGetResourceDirectory(RD_SHADER_SOURCES), fileName, filePath);
			char outFilePath[FS_MAX_PATH] = {};
			fsAppendPathComponent(fsGetResourceDirectory(RD_SHADER_BINARIES), outFile, outFilePath);
			char intermediateFilePath[FS_MAX_PATH] = {};
			fsAppendPathExtension(outFilePath, "air", intermediateFilePath);

			const char* xcrun = "/usr/bin/xcrun";
			eastl::vector<eastl::string> args;
			eastl::string tmpArg = "";

			// Compile the source into a temporary .air file.
			args.push_back("-sdk");
			args.push_back("macosx");
			args.push_back("metal");
			args.push_back("-c");
			args.push_back(filePath);
			args.push_back("-o");
			args.push_back(intermediateFilePath);

			//enable the 2 below for shader debugging on xcode
			//args.push_back("-MO");
			//args.push_back("-gline-tables-only");
			args.push_back("-D");
			args.push_back("MTL_SHADER=1");    // Add MTL_SHADER macro to differentiate structs in headers shared by app/shader code.

			// Add user defined macros to the command line
			for (uint32_t i = 0; i < macroCount; ++i)
			{
				args.push_back("-D");
				args.push_back(eastl::string(pMacros[i].definition) + "=" + pMacros[i].value);
			}

			eastl::vector<const char*> cArgs;
			for (eastl::string& arg : args) {
				cArgs.push_back(arg.c_str());
			}

			if (system_run(xcrun, &cArgs[0], cArgs.size(), NULL) == 0)
			{
				// Create a .metallib file from the .air file.
				args.clear();
				tmpArg = "";
				args.push_back("-sdk");
				args.push_back("macosx");
				args.push_back("metallib");
				args.push_back(intermediateFilePath);
				args.push_back("-o");
				tmpArg = eastl::string().sprintf(
					""
					"%s"
					"",
					outFilePath);
				args.push_back(tmpArg);

				cArgs.clear();
				for (eastl::string& arg : args) {
					cArgs.push_back(arg.c_str());
				}

				if (systemRun(xcrun, &cArgs[0], cArgs.size(), NULL) == 0)
				{
					// Remove the temp .air file.
					const char* nativePath = intermediateFilePath;
					systemRun("rm", &nativePath, 1, NULL);

					// Store the compiled bytecode.
					FileStream fHandle = {};
					if (fsOpenStreamFromPath(RD_SHADER_BINARIES, outFile, FM_READ_BINARY, &fHandle))
					{
						pOut->mByteCodeSize = (uint32_t)fsGetStreamFileSize(&fHandle);
						pOut->pByteCode = tf_malloc(pOut->mByteCodeSize);
						fsReadFromStream(&fHandle, pOut->pByteCode, pOut->mByteCodeSize);
						fsCloseStream(&fHandle);
					}
				}
				else
				{
					LOGF(eERROR, "Failed to assemble shader's %s .metallib file", outFile);
				}
			}
			else
			{
				LOGF(eERROR, "Failed to compile shader %s", filePath);
			}
		}
		#endif
		#if (defined(SG_GRAPHIC_API_D3D12) || defined(SG_GRAPHIC_API_D3D11))
		extern void compileShader(
			Renderer* pRenderer, ShaderTarget target, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code,
			bool enablePrimitiveId, uint32_t macroCount, ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint);
		#endif
		#if defined(ORBIS)
		extern bool orbis_compileShader(
			Renderer* pRenderer,
			ShaderStage stage, ShaderStage allStages,
			const char* srcFileName,
			const char* outFileName,
			uint32_t macroCount, ShaderMacro* pMacros,
			BinaryShaderStageDesc* pOut,
			const char* pEntryPoint);
		#endif
		#if defined(PROSPERO)
		extern bool prospero_compileShader(
			Renderer* pRenderer,
			ShaderStage stage, ShaderStage allStages,
			const char* srcFileName,
			const char* outFileName,
			uint32_t macroCount, ShaderMacro* pMacros,
			BinaryShaderStageDesc* pOut,
			const char* pEntryPoint);
		#endif
		#if defined(SG_GRAPHIC_API_GLES)
		extern void gl_compileShader(Renderer* pRenderer, ShaderTarget target, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code,
			bool enablePrimitiveId, uint32_t macroCount, ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint);
	#endif

	static eastl::string sgfs_read_from_stream_stl_line(FileStream* stream)
	{
		eastl::string result;

		while (!sgfs_is_stream_at_end(stream))
		{
			char nextChar = 0;
			sgfs_read_from_stream(stream, &nextChar, sizeof(nextChar));
			if (nextChar == 0 || nextChar == '\n')
			{
				break;
			}
			if (nextChar == '\r')
			{
				char newLine = 0;
				sgfs_read_from_stream(stream, &newLine, sizeof(newLine));
				if (newLine == '\n')
				{
					break;
				}
				else
				{
					// We're not looking at a "\r\n" sequence, so add the '\r' to the buffer.
					sgfs_seek_stream(stream, SG_SBO_CURRENT_POSITION, -1);
				}
			}
			result.push_back(nextChar);
		}
		return result;
	}

	// function to generate the timestamp of this shader source file considering all include file timestamp
	#if !defined(NX64)
	static bool process_source_file(const char* pAppName, FileStream* original, const char* filePath, FileStream* file, time_t& outTimeStamp, eastl::string& outCode)
	{
		// If the source if a non-packaged file, store the timestamp
		if (file)
		{
			time_t fileTimeStamp = sgfs_get_last_modified_time(SG_RD_SHADER_SOURCES, filePath);

			if (fileTimeStamp > outTimeStamp)
				outTimeStamp = fileTimeStamp;
		}
		else
		{
			return true; // The source file is missing, but we may still be able to use the shader binary.
		}

		const eastl::string pIncludeDirective = "#include";
		while (!sgfs_is_stream_at_end(file))
		{
			eastl::string line = sgfs_read_from_stream_stl_line(file);

			size_t filePos = line.find(pIncludeDirective, 0);
			const size_t  commentPosCpp = line.find("//", 0);
			const size_t  commentPosC = line.find("/*", 0);

			// if we have an "#include \"" in our current line
			const bool bLineHasIncludeDirective = filePos != eastl::string::npos;
			const bool bLineIsCommentedOut = (commentPosCpp != eastl::string::npos && commentPosCpp < filePos) ||
				(commentPosC != eastl::string::npos && commentPosC < filePos);

			if (bLineHasIncludeDirective && !bLineIsCommentedOut)
			{
				// get the include file name
				size_t        currentPos = filePos + pIncludeDirective.length();
				eastl::string fileName;
				while (line.at(currentPos++) == ' ')
					;    // skip empty spaces
				if (currentPos >= line.size())
					continue;
				if (line.at(currentPos - 1) != '\"')
					continue;
				else
				{
					// read char by char until we have the include file name
					while (line.at(currentPos) != '\"')
					{
						fileName.push_back(line.at(currentPos));
						++currentPos;
					}
				}

				// get the include file path
				//TODO: remove Comments
				if (fileName.at(0) == '<')    // disregard bracketsauthop
					continue;

				// open the include file
				FileStream fHandle = {};
				char includePath[SG_MAX_FILEPATH] = {};
				{
					char parentPath[SG_MAX_FILEPATH] = {};
					sgfs_get_parent_path(filePath, parentPath);
					sgfs_append_path_component(parentPath, fileName.c_str(), includePath);
				}
				if (!sgfs_open_stream_from_path(SG_RD_SHADER_SOURCES, includePath, SG_FM_READ_BINARY, &fHandle))
				{
					SG_LOG_ERROR("Cannot open #include file: %s", includePath);
					continue;
				}

				// add the include file into the current code recursively
				if (!process_source_file(pAppName, original, includePath, &fHandle, outTimeStamp, outCode))
				{
					sgfs_close_stream(&fHandle);
					return false;
				}

				sgfs_close_stream(&fHandle);
			}

	#if defined(TARGET_IOS) || defined(ANDROID)
			// iOS doesn't have support for resolving user header includes in shader code
			// when compiling with shader source using Metal runtime.
			// https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/FunctionsandLibraries.html
			//
			// Here we write out the contents of the header include into the original source
			// where its included from -- we're expanding the headers as the pre-processor
			// would do.
			//
			//const bool bAreWeProcessingAnIncludedHeader = file != original;
			if (!bLineHasIncludeDirective)
			{
				outCode += line + "\n";
			}
	#else
			// simply write out the current line if we are not in a header file
			const bool bAreWeProcessingTheShaderSource = file == original;
			if (bAreWeProcessingTheShaderSource)
			{
				outCode += line + "\n";
			}
	#endif
		}
		return true;
	}
	#endif

	// loads the bytecode from file if the binary shader file is newer than the source
	bool check_for_byte_code(Renderer* pRenderer, const char* binaryShaderPath, time_t sourceTimeStamp, BinaryShaderStageDesc* pOut)
	{
		// if source code is loaded from a package, its timestamp will be zero. Else check that binary is not older
		// than source, to make sure we are using the latest bytecode
		time_t dstTimeStamp = sgfs_get_last_modified_time(SG_RD_SHADER_BINARIES, binaryShaderPath);
		if (!sourceTimeStamp || (dstTimeStamp < sourceTimeStamp))
			return false;

		FileStream fh = {};
		if (!sgfs_open_stream_from_path(SG_RD_SHADER_BINARIES, binaryShaderPath, SG_FM_READ_BINARY, &fh))
		{
			SG_LOG_ERROR("'%s' is not a valid shader bytecode file", binaryShaderPath);
			return false;
		}

		if (!sgfs_get_stream_file_size(&fh))
		{
			sgfs_close_stream(&fh);
			return false;
		}

	#if defined(PROSPERO)
		extern void prospero_loadByteCode(Renderer*, FileStream*, BinaryShaderStageDesc*);
		prospero_loadByteCode(pRenderer, &fh, pOut);
	#else
		ssize_t size = sgfs_get_stream_file_size(&fh);
		pOut->byteCodeSize = (uint32_t)size;
		//pOut->pByteCode = sg_memalign(256, size);
		pOut->pByteCode = sg_malloc(size);
		ASSERT(pOut->pByteCode);

		sgfs_read_from_stream(&fh, (void*)pOut->pByteCode, size);
	#endif
		sgfs_close_stream(&fh);

		return true;
	}

	// saves bytecode to a file
	bool save_byte_code(const char* binaryShaderPath, char* byteCode, uint32_t byteCodeSize)
	{
		if (!byteCodeSize)
			return false;

		FileStream fh = {};
		if (!sgfs_open_stream_from_path(SG_RD_SHADER_BINARIES, binaryShaderPath, SG_FM_WRITE_BINARY, &fh))
			return false;

		sgfs_write_to_stream(&fh, byteCode, byteCodeSize);
		sgfs_close_stream(&fh);
		return true;
	}

	bool load_shader_stage_byte_code(
		Renderer* pRenderer, ShaderTarget target, ShaderStage stage, ShaderStage allStages, const ShaderStageLoadDesc& loadDesc, uint32_t macroCount,
		ShaderMacro* pMacros, BinaryShaderStageDesc* pOut)
	{
		UNREF_PARAM(loadDesc.flags);

		eastl::string code;
	#if !defined(NX64)
		time_t timeStamp = 0;
	#endif

	#if !defined(SG_GRAPHIC_API_METAL) && !defined(NX64)
		FileStream sourceFileStream = {};
		bool sourceExists = sgfs_open_stream_from_path(SG_RD_SHADER_SOURCES, loadDesc.fileName, SG_FM_READ_BINARY, &sourceFileStream);
		ASSERT(sourceExists);

		if (!process_source_file(pRenderer->name, &sourceFileStream, loadDesc.fileName, &sourceFileStream, timeStamp, code))
		{
			sgfs_close_stream(&sourceFileStream);
			return false;
		}
	#elif defined(NX64)
		eastl::string shaderDefines;
		for (uint32_t i = 0; i < macroCount; ++i)
		{
			shaderDefines += (eastl::string(pMacros[i].definition) + pMacros[i].value);
		}

		uint32_t hash = 0;
		MurmurHash3_x86_32(shaderDefines.c_str(), shaderDefines.size(), 0, &hash);

		char hashStringBuffer[14];
		sprintf(&hashStringBuffer[0], "%zu.%s", (size_t)hash, "spv");

		char nxShaderPath[FS_MAX_PATH] = {};
		fsAppendPathExtension(loadDesc.pFileName, hashStringBuffer, nxShaderPath);
		FileStream sourceFileStream = {};
		bool sourceExists = fsOpenStreamFromPath(RD_SHADER_SOURCES, nxShaderPath, FM_READ_BINARY, &sourceFileStream);
		ASSERT(sourceExists);

		if (sourceExists)
		{
			pOut->mByteCodeSize = (uint32_t)fsGetStreamFileSize(&sourceFileStream);
			pOut->pByteCode = tf_malloc(pOut->mByteCodeSize);
			fsReadFromStream(&sourceFileStream, pOut->pByteCode, pOut->mByteCodeSize);

			LOGF(LogLevel::eINFO, "Shader loaded: '%s' with macro string '%s'", nxShaderPath, shaderDefines.c_str());
			fsCloseStream(&sourceFileStream);
			return true;
		}
		else
		{
			LOGF(LogLevel::eERROR, "Failed to load shader: '%s' with macro string '%s'", nxShaderPath, shaderDefines.c_str());
			return false;
		}

	#else
		char metalShaderPath[FS_MAX_PATH] = {};
		fsAppendPathExtension(loadDesc.pFileName, "metal", metalShaderPath);
		FileStream sourceFileStream = {};
		bool sourceExists = fsOpenStreamFromPath(RD_SHADER_SOURCES, metalShaderPath, FM_READ_BINARY, &sourceFileStream);
		ASSERT(sourceExists);
		if (!process_source_file(pRenderer->pName, &sourceFileStream, metalShaderPath, &sourceFileStream, timeStamp, code))
		{
			fsCloseStream(&sourceFileStream);
			return false;
		}
	#endif

	#ifndef NX64
		eastl::string shaderDefines;
		// apply user specified macros
		for (uint32_t i = 0; i < macroCount; ++i)
		{
			shaderDefines += (eastl::string(pMacros[i].definition) + pMacros[i].value);
		}
	#ifdef _DEBUG
		shaderDefines += "_DEBUG";
	#else
		shaderDefines += "NDEBUG";
	#endif

		eastl::string rendererApi;
		switch (pRenderer->api)
		{
			case SG_RENDERER_API_D3D12:
			//case SG_RENDERER_API_XBOX_D3D12: rendererApi = "D3D12"; break;
			//case SG_RENDERER_API_D3D11: rendererApi = "D3D11"; break;
			case SG_RENDERER_API_VULKAN: rendererApi = "Vulkan"; break;
			//case SG_RENDERER_API_METAL: rendererApi = "Metal"; break;
			case SG_RENDERER_API_GLES: rendererApi = "GLES"; break;
			default: break;
		}
		char extension[SG_MAX_FILEPATH] = { 0 };
		sgfs_get_path_extension(loadDesc.fileName, extension);

		char fileName[SG_MAX_FILEPATH] = { 0 };
		sgfs_get_path_file_name(loadDesc.fileName, fileName);

		eastl::string appName(pRenderer->name);
	#ifdef __linux__
		appName.make_lower();
		appName = appName != pRenderer->pName ? appName : appName + "_";
	#endif
		eastl::string binaryShaderComponent = fileName +
			eastl::string().sprintf("_%zu", eastl::string_hash<eastl::string>()(shaderDefines)) + extension +
			eastl::string().sprintf("%u", target) +
	#ifdef SG_GRAPHIC_API_D3D11
			eastl::string().sprintf("%u", pRenderer->mFeatureLevel) +
	#endif
			".bin";

		SG_LOG_DEBUG("binary shader component: %s", binaryShaderComponent.c_str());

		// shader source is newer than binary
		if (!check_for_byte_code(pRenderer, binaryShaderComponent.c_str(), timeStamp, pOut))
		{
			if (!sourceExists)
			{
				SG_LOG_ERROR("No source shader or precompiled binary present for file %s", fileName);
				return false;
			}
	#if defined(ORBIS)
			orbis_compileShader(pRenderer,
				stage, allStages,
				loadDesc.pFileName,
				binaryShaderComponent.c_str(),
				macroCount, pMacros,
				pOut,
				loadDesc.pEntryPointName);
	#elif defined(PROSPERO)
			prospero_compileShader(pRenderer,
				stage, allStages,
				loadDesc.pFileName,
				binaryShaderComponent.c_str(),
				macroCount, pMacros,
				pOut,
				loadDesc.pEntryPointName);
	#else
			if (pRenderer->api == SG_RENDERER_API_VULKAN || pRenderer->api == SG_RENDERER_API_GLES)
			{
#if defined(SG_GRAPHIC_API_VULKAN)
		#if defined(__ANDROID__)
					vk_compileShader(pRenderer, stage, (uint32_t)code.size(), code.c_str(), binaryShaderComponent.c_str(), macroCount, pMacros, pOut, loadDesc.entryPointName);
					if (!save_byte_code(binaryShaderComponent.c_str(), (char*)(pOut->pByteCode), pOut->byteCodeSize))
					{
						LOGF(LogLevel::eWARNING, "Failed to save byte code for file %s", loadDesc.pFileName);
					}
		#else
				vk_compile_shader(pRenderer, target, stage, loadDesc.fileName, binaryShaderComponent.c_str(), macroCount, pMacros, pOut, loadDesc.entryPointName);
		#endif
#elif defined(SG_GRAPHIC_API_METAL)
				mtl_compileShader(pRenderer, metalShaderPath, binaryShaderComponent.c_str(), macroCount, pMacros, pOut, loadDesc.entryPointName);
#elif defined(SG_GRAPHIC_API_GLES)
				gl_compileShader(pRenderer, target, stage, loadDesc.pFileName, (uint32_t)code.size(), code.c_str(), binaryShaderComponent.c_str(), macroCount, pMacros, pOut, loadDesc.entryPointName);
#endif
			}
			else
			{
	#if defined(SG_RENDERER_API_D3D12) || defined(SG_RENDERER_API_D3D11)
				compileShader(
					pRenderer, target, stage, loadDesc.pFileName, (uint32_t)code.size(), code.c_str(),
					loadDesc.flags & SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID,
					macroCount, pMacros,
					pOut, loadDesc.pEntryPointName);

				if (!save_byte_code(binaryShaderComponent.c_str(), (char*)(pOut->pByteCode), pOut->mByteCodeSize))
				{
					LOGF(LogLevel::eWARNING, "Failed to save byte code for file %s", loadDesc.pFileName);
				}
	#endif
			}

			if (!pOut->pByteCode)
			{
				SG_LOG_ERROR("Error while generating bytecode for shader %s", loadDesc.fileName);
				sgfs_close_stream(&sourceFileStream);
				ASSERT(false);
				return false;
			}
	#endif
		}
	#else
	#endif

		sgfs_close_stream(&sourceFileStream);
		return true;
	}

	#ifdef TARGET_IOS
	bool find_shader_stage(const char* fileName, ShaderDesc* pDesc, ShaderStageDesc** pOutStage, ShaderStage* pStage)
	{
		char extension[FS_MAX_PATH] = { 0 };
		fsGetPathExtension(fileName, extension);
		if (stricmp(extension, "vert") == 0)
		{
			*pOutStage = &pDesc->mVert;
			*pStage = SHADER_STAGE_VERT;
		}
		else if (stricmp(extension, "frag") == 0)
		{
			*pOutStage = &pDesc->mFrag;
			*pStage = SHADER_STAGE_FRAG;
		}
		else if (stricmp(extension, "comp") == 0)
		{
			*pOutStage = &pDesc->mComp;
			*pStage = SHADER_STAGE_COMP;
		}
		else if ((stricmp(extension, "rgen") == 0) ||
			(stricmp(extension, "rmiss") == 0) ||
			(stricmp(extension, "rchit") == 0) ||
			(stricmp(extension, "rint") == 0) ||
			(stricmp(extension, "rahit") == 0) ||
			(stricmp(extension, "rcall") == 0))
		{
			*pOutStage = &pDesc->mComp;
			*pStage = SHADER_STAGE_COMP;
		}
		else
		{
			return false;
		}

		return true;
	}
	#else
	bool find_shader_stage(const char* extension, BinaryShaderCreateDesc* pBinaryDesc, BinaryShaderStageDesc** pOutStage, ShaderStage* pStage)
	{
		if (stricmp(extension, "vert") == 0)
		{
			*pOutStage = &pBinaryDesc->vert;
			*pStage = SG_SHADER_STAGE_VERT;
		}
		else if (stricmp(extension, "frag") == 0)
		{
			*pOutStage = &pBinaryDesc->frag;
			*pStage = SG_SHADER_STAGE_FRAG;
		}
	#ifndef SG_GRAPHIC_API_METAL
		else if (stricmp(extension, "tesc") == 0)
		{
			*pOutStage = &pBinaryDesc->hull;
			*pStage = SG_SHADER_STAGE_HULL;
		}
		else if (stricmp(extension, "tese") == 0)
		{
			*pOutStage = &pBinaryDesc->domain;
			*pStage = SG_SHADER_STAGE_DOMN;
		}
		else if (stricmp(extension, "geom") == 0)
		{
			*pOutStage = &pBinaryDesc->geom;
			*pStage = SG_SHADER_STAGE_GEOM;
		}
	#endif
		else if (stricmp(extension, "comp") == 0)
		{
			*pOutStage = &pBinaryDesc->comp;
			*pStage = SG_SHADER_STAGE_COMP;
		}
		else if ((stricmp(extension, "rgen") == 0) ||
			(stricmp(extension, "rmiss") == 0) ||
			(stricmp(extension, "rchit") == 0) ||
			(stricmp(extension, "rint") == 0) ||
			(stricmp(extension, "rahit") == 0) ||
			(stricmp(extension, "rcall") == 0))
		{
			*pOutStage = &pBinaryDesc->comp;
	#ifndef SG_GRAPHIC_API_METAL
			* pStage = SG_SHADER_STAGE_RAYTRACING;
	#else
			* pStage = SG_SHADER_STAGE_COMP;
	#endif
		}
		else
		{
			return false;
		}
		return true;
	}
	#endif

	void add_shader(Renderer* pRenderer, const ShaderLoadDesc* pDesc, Shader** ppShader)
	{
	#ifndef SG_GRAPHIC_API_D3D11
		if ((uint32_t)pDesc->target > pRenderer->shaderTarget)
		{
			eastl::string error = eastl::string().sprintf("Requested shader target (%u) is higher than the shader target that the renderer supports (%u). Shader wont be compiled",
				(uint32_t)pDesc->target, (uint32_t)pRenderer->shaderTarget);
			SG_LOG_ERROR(error.c_str());
			return;
		}
	#endif

	#ifndef TARGET_IOS
		BinaryShaderCreateDesc binaryDesc = {};
	#if defined(SG_GRAPHIC_API_METAL)
		char* pSources[SG_SHADER_STAGE_COUNT] = {};
	#endif

		ShaderStage stages = SG_SHADER_STAGE_NONE;
		for (uint32_t i = 0; i < SG_SHADER_STAGE_COUNT; ++i)
		{
			if (pDesc->stages[i].fileName && strlen(pDesc->stages[i].fileName) != 0)
			{
				ShaderStage            stage;
				BinaryShaderStageDesc* pStage = nullptr;
				char ext[SG_MAX_FILEPATH] = { 0 };
				sgfs_get_path_extension(pDesc->stages[i].fileName, ext);
				if (find_shader_stage(ext, &binaryDesc, &pStage, &stage))
					stages |= stage;
			}
		}

		for (uint32_t i = 0; i < SG_SHADER_STAGE_COUNT; ++i)
		{
			if (pDesc->stages[i].fileName && strlen(pDesc->stages[i].fileName) != 0)
			{
				const char* fileName = pDesc->stages[i].fileName;

				ShaderStage            stage;
				BinaryShaderStageDesc* pStage = NULL;
				char ext[SG_MAX_FILEPATH] = { 0 };
				sgfs_get_path_extension(fileName, ext);
				if (find_shader_stage(ext, &binaryDesc, &pStage, &stage))
				{
					const uint32_t macroCount = pDesc->stages[i].macroCount + pRenderer->builtinShaderDefinesCount;

					eastl::vector<ShaderMacro> macros(macroCount);
					for (uint32_t macro = 0; macro < pRenderer->builtinShaderDefinesCount; ++macro)
						macros[macro] = pRenderer->pBuiltinShaderDefines[macro];
					for (uint32_t macro = 0; macro < pDesc->stages[i].macroCount; ++macro)
						macros[pRenderer->builtinShaderDefinesCount + macro] = pDesc->stages[i].pMacros[macro];

					if (!load_shader_stage_byte_code(pRenderer, pDesc->target, stage, stages, pDesc->stages[i], macroCount, macros.data(),
						pStage))
						return;

					binaryDesc.stages |= stage;
	#if defined(SG_GRAPHIC_API_METAL)
					if (pDesc->stages[i].pEntryPointName)
						pStage->pEntryPoint = pDesc->stages[i].pEntryPointName;
					else
						pStage->pEntryPoint = "stageMain";

					char metalFileName[FS_MAX_PATH] = { 0 };
					fsAppendPathExtension(fileName, "metal", metalFileName);

					FileStream fh = {};
					fsOpenStreamFromPath(RD_SHADER_SOURCES, metalFileName, FM_READ_BINARY, &fh);
					size_t metalFileSize = fsGetStreamFileSize(&fh);
					pSources[i] = (char*)tf_malloc(metalFileSize + 1);
					pStage->pSource = pSources[i];
					pStage->mSourceSize = (uint32_t)metalFileSize;
					fsReadFromStream(&fh, pSources[i], metalFileSize);
					pSources[i][metalFileSize] = 0; // Ensure the shader text is null-terminated
					fsCloseStream(&fh);
	#elif !defined(ORBIS) && !defined(PROSPERO)
					if (pDesc->stages[i].entryPointName)
						pStage->pEntryPoint = pDesc->stages[i].entryPointName;
					else
						pStage->pEntryPoint = "main";
	#endif
				}
			}
		}

	#if defined(PROSPERO)
		binaryDesc.mOwnByteCode = true;
	#endif

		add_shader_binary(pRenderer, &binaryDesc, ppShader);
	#if defined(SG_GRAPHIC_API_METAL)
		for (uint32_t i = 0; i < SG_SHADER_STAGE_COUNT; ++i)
		{
			if (pSources[i])
			{
				sg_free(pSources[i]);
			}
		}
	#endif
	#if !defined(PROSPERO)
		if (binaryDesc.stages & SG_SHADER_STAGE_VERT)
			sg_free(binaryDesc.vert.pByteCode);
		if (binaryDesc.stages & SG_SHADER_STAGE_FRAG)
			sg_free(binaryDesc.frag.pByteCode);
		if (binaryDesc.stages & SG_SHADER_STAGE_COMP)
			sg_free(binaryDesc.comp.pByteCode);
	#if !defined(SG_GRAPHIC_API_METAL)
		if (binaryDesc.stages & SG_SHADER_STAGE_TESC)
			sg_free(binaryDesc.hull.pByteCode);
		if (binaryDesc.stages & SG_SHADER_STAGE_TESE)
			sg_free(binaryDesc.domain.pByteCode);
		if (binaryDesc.stages & SG_SHADER_STAGE_GEOM)
			sg_free(binaryDesc.geom.pByteCode);
		if (binaryDesc.stages & SG_SHADER_STAGE_RAYTRACING)
			sg_free(binaryDesc.comp.pByteCode);
	#endif
	#endif
	#else
		// Binary shaders are not supported on iOS.
		ShaderDesc desc = {};
		eastl::string codes[SG_SHADER_STAGE_COUNT] = {};
		ShaderMacro* pMacros[SG_SHADER_STAGE_COUNT] = {};
		for (uint32_t i = 0; i < SG_SHADER_STAGE_COUNT; ++i)
		{
			if (pDesc->stages[i].pFileName && strlen(pDesc->stages[i].pFileName))
			{
				ShaderStage stage;
				ShaderStageDesc* pStage = NULL;
				if (find_shader_stage(pDesc->stages[i].pFileName, &desc, &pStage, &stage))
				{
					char metalFileName[FS_MAX_PATH] = { 0 };
					fsAppendPathExtension(pDesc->stages[i].pFileName, "metal", metalFileName);
					FileStream fh = {};
					bool sourceExists = fsOpenStreamFromPath(RD_SHADER_SOURCES, metalFileName, FM_READ_BINARY, &fh);
					ASSERT(sourceExists);

					pStage->pName = pDesc->stages[i].pFileName;
					time_t timestamp = 0;
					process_source_file(pRenderer->pName, &fh, metalFileName, &fh, timestamp, codes[i]);
					pStage->pCode = codes[i].c_str();
					if (pDesc->stages[i].pEntryPointName)
						pStage->pEntryPoint = pDesc->stages[i].pEntryPointName;
					else
						pStage->pEntryPoint = "stageMain";
					// Apply user specified shader macros
					pStage->mMacroCount = pDesc->stages[i].mMacroCount + pRenderer->mBuiltinShaderDefinesCount;
					pMacros[i] = (ShaderMacro*)alloca(pStage->mMacroCount * sizeof(ShaderMacro));
					pStage->pMacros = pMacros[i];
					for (uint32_t j = 0; j < pDesc->stages[i].mMacroCount; j++)
						pMacros[i][j] = pDesc->stages[i].pMacros[j];
					// Apply renderer specified shader macros
					for (uint32_t j = 0; j < pRenderer->mBuiltinShaderDefinesCount; j++)
					{
						pMacros[i][pDesc->stages[i].mMacroCount + j] = pRenderer->pBuiltinShaderDefines[j];
					}
					fsCloseStream(&fh);
					desc.stages |= stage;
				}
			}
		}

		add_shader(pRenderer, &desc, ppShader);
	#endif
	}

	// pipeline cache save, load
	void add_pipeline_cache(Renderer* pRenderer, const PipelineCacheLoadDesc* pDesc, PipelineCache** ppPipelineCache)
	{
	#if defined(SG_GRAPHIC_API_D3D12) || defined(SG_GRAPHIC_API_VULKAN)
		FileStream stream = {};
		bool success = sgfs_open_stream_from_path(SG_RD_PIPELINE_CACHE, pDesc->fileName, SG_FM_READ_BINARY, &stream);
		ssize_t dataSize = 0;
		void* data = nullptr;
		if (success)
		{
			dataSize = sgfs_get_stream_file_size(&stream);
			data = nullptr;
			if (dataSize)
			{
				data = sg_malloc(dataSize);
				sgfs_read_from_stream(&stream, data, dataSize);
			}

			sgfs_close_stream(&stream);
		}

		PipelineCacheDesc desc = {};
		desc.flags = pDesc->flags;
		desc.pData = data;
		desc.size = dataSize;
		add_pipeline_cache(pRenderer, &desc, ppPipelineCache);

		if (data)
		{
			sg_free(data);
		}
	#endif
	}

	void save_pipeline_cache(Renderer* pRenderer, PipelineCache* pPipelineCache, PipelineCacheSaveDesc* pDesc)
	{
	#if defined(SG_GRAPHIC_API_D3D12) || defined(SG_GRAPHIC_API_VULKAN)
		FileStream stream = {};
		if (sgfs_open_stream_from_path(SG_RD_PIPELINE_CACHE, pDesc->fileName, SG_FM_WRITE_BINARY, &stream))
		{
			size_t dataSize = 0;
			get_pipeline_cache_data(pRenderer, pPipelineCache, &dataSize, nullptr);
			if (dataSize)
			{
				void* data = sg_malloc(dataSize);
				get_pipeline_cache_data(pRenderer, pPipelineCache, &dataSize, data);
				sgfs_write_to_stream(&stream, data, dataSize);
				sg_free(data);
			}

			sgfs_close_stream(&stream);
		}
	#endif
	}

}