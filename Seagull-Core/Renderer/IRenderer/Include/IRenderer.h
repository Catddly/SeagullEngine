#pragma once

#include "../../../Core/Source/Interface/IOperatingSystem.h"
#include "../../../Core/Source/Interface/IThread.h"
#include "../../../Core/Third-party/Include/tinyImageFormat/include/tinyimageformat_base.h"

#ifdef __cplusplus
#ifndef SG_MAKE_ENUM_FLAG
#define SG_MAKE_ENUM_FLAG(TYPE, ENUM_TYPE)																				\
			static inline ENUM_TYPE operator|(ENUM_TYPE a, ENUM_TYPE b)   { return (ENUM_TYPE)((TYPE)(a) | (TYPE)(b)); }\
			static inline ENUM_TYPE operator&(ENUM_TYPE a, ENUM_TYPE b)   { return (ENUM_TYPE)((TYPE)(a) & (TYPE)(b)); }\
			static inline ENUM_TYPE operator|=(ENUM_TYPE& a, ENUM_TYPE b) { return a = (a | b); }                       \
			static inline ENUM_TYPE operator&=(ENUM_TYPE& a, ENUM_TYPE b) { return a = (a & b); }
#endif
#else
#define SG_MAKE_ENUM_FLAG(TYPE, ENUM_TYPE)
#endif

typedef struct VmaAllocator_T* VmaAllocator;
typedef struct VmaAllocation_T* VmaAllocation;

// default levels for renderer to use
#if !defined(SG_RENDERER_CUSTOM_MAX_CONFIG)
enum
{
	SG_MAX_INSTANCE_EXTENSIONS = 64,
	SG_MAX_DEVICE_EXTENSIONS = 64,
	SG_MAX_LINKED_GPUS = 4,                   /// max number of GPUs in SLI or Cross-Fire
	SG_MAX_RENDER_TARGET_ATTACHMENTS = 8,
	SG_MAX_VERTEX_BINDINGS = 15,
	SG_MAX_VERTEX_ATTRIBS = 15,
	SG_MAX_SEMANTIC_NAME_LENGTH = 128,
	SG_MAX_DEBUG_NAME_LENGTH = 128,
	SG_MAX_MIP_LEVELS = 0xFFFFFFFF,
	SG_MAX_SWAPCHAIN_IMAGES = 3,              /// triple buffer
	SG_MAX_ROOT_CONSTANTS_PER_ROOTSIGNATURE = 4,
	SG_MAX_GPU_VENDOR_STRING_LENGTH = 64,     /// max size for GPUVendorPreset strings
#if defined(SG_GRAPHIC_API_VULKAN)
	MAX_PLANE_COUNT = 3,
#endif
};
#endif

#if defined(SG_GRAPHIC_API_VULKAN)
	#if defined(SG_PLATFORM_WINDOWS)
		#define VK_USE_PLATFORM_WIN32_KHR
	#endif
#endif

#ifdef SG_GRAPHIC_API_VULKAN
//#define VK_NO_PROTOTYPES
	#include "../../Vulkan/include/Vulkan/vulkan_core.h"
	#ifdef SG_PLATFORM_WINDOWS
		#include "../../Vulkan/include/Vulkan/vulkan_win32.h"
	#endif
#endif

namespace SG
{

	typedef enum RendererAPI
	{
		SG_RENDERER_API_VULKAN = 0,
		SG_RENDERER_API_D3D12,
		SG_RENDERER_API_GLES
	} RendererAPI;

	typedef enum LogType
	{
		SG_LOG_TYPE_INFO = 0,
		SG_LOG_TYPE_WARN,
		SG_LOG_TYPE_DEBUG,
		SG_LOG_TYPE_ERROR
	} LogType;

	typedef void(*LogFunc)(LogType, const char*, const char*);

	typedef enum SampleCount
	{
		SG_SAMPLE_COUNT_1 = 1,
		SG_SAMPLE_COUNT_2 = 2,
		SG_SAMPLE_COUNT_4 = 4,
		SG_SAMPLE_COUNT_8 = 8,
		SG_SAMPLE_COUNT_16 = 16,
	} SampleCount;

	typedef union ClearValue
	{
		struct
		{
			float r;
			float g;
			float b;
			float a;
		};
		struct
		{
			float    depth;
			uint32_t stencil;
		};
	} ClearValue;

	typedef enum ResourceState // for barrier purpose
	{
		SG_RESOURCE_STATE_UNDEFINED = 0,
		SG_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER = 0x1,
		SG_RESOURCE_STATE_INDEX_BUFFER = 0x2,
		SG_RESOURCE_STATE_RENDER_TARGET = 0x4,
		SG_RESOURCE_STATE_UNORDERED_ACCESS = 0x8,
		SG_RESOURCE_STATE_DEPTH_WRITE = 0x10,
		SG_RESOURCE_STATE_DEPTH_READ = 0x20,
		SG_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE = 0x40,
		SG_RESOURCE_STATE_PIXEL_SHADER_RESOURCE = 0x80,
		SG_RESOURCE_STATE_SHADER_RESOURCE = 0x40 | 0x80,
		SG_RESOURCE_STATE_STREAM_OUT = 0x100,
		SG_RESOURCE_STATE_INDIRECT_ARGUMENT = 0x200,
		SG_RESOURCE_STATE_COPY_DEST = 0x400,
		SG_RESOURCE_STATE_COPY_SOURCE = 0x800,
		SG_RESOURCE_STATE_GENERIC_READ = (((((0x1 | 0x2) | 0x40) | 0x80) | 0x200) | 0x800),
		SG_RESOURCE_STATE_PRESENT = 0x1000,
		SG_RESOURCE_STATE_COMMON = 0x2000,
		SG_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE = 0x4000,
	} ResourceState;
	SG_MAKE_ENUM_FLAG(uint32_t, ResourceState);

	typedef enum ShaderStage
	{
		SG_SHADER_STAGE_NONE = 0,
		SG_SHADER_STAGE_VERT = 0X00000001,
		SG_SHADER_STAGE_TESC = 0X00000002,
		SG_SHADER_STAGE_TESE = 0X00000004,
		SG_SHADER_STAGE_GEOM = 0X00000008,
		SG_SHADER_STAGE_FRAG = 0X00000010,
		SG_SHADER_STAGE_COMP = 0X00000020,
		SG_SHADER_STAGE_RAYTRACING = 0X00000040,
		SG_SHADER_STAGE_ALL_GRAPHICS = ((uint32_t)SG_SHADER_STAGE_VERT | (uint32_t)SG_SHADER_STAGE_TESC | (uint32_t)SG_SHADER_STAGE_TESE | (uint32_t)SG_SHADER_STAGE_GEOM | (uint32_t)SG_SHADER_STAGE_FRAG),
		SG_SHADER_STAGE_HULL = SG_SHADER_STAGE_TESC,
		SG_SHADER_STAGE_DOMN = SG_SHADER_STAGE_TESE,
		SG_SHADER_STAGE_COUNT = 7,
	} ShaderStage;
	SG_MAKE_ENUM_FLAG(uint32_t, ShaderStage);

	typedef enum DescriptorType
	{
		SG_DESCRIPTOR_TYPE_UNDEFINED = 0,
		SG_DESCRIPTOR_TYPE_SAMPLER = 0x01,
		// SRV Read only texture
		SG_DESCRIPTOR_TYPE_TEXTURE = (SG_DESCRIPTOR_TYPE_SAMPLER << 1),
		/// UAV Texture
		SG_DESCRIPTOR_TYPE_RW_TEXTURE = (SG_DESCRIPTOR_TYPE_TEXTURE << 1),
		// SRV Read only buffer
		SG_DESCRIPTOR_TYPE_BUFFER = (SG_DESCRIPTOR_TYPE_RW_TEXTURE << 1),
		SG_DESCRIPTOR_TYPE_BUFFER_RAW = (SG_DESCRIPTOR_TYPE_BUFFER | (SG_DESCRIPTOR_TYPE_BUFFER << 1)),
		/// UAV Buffer
		SG_DESCRIPTOR_TYPE_RW_BUFFER = (SG_DESCRIPTOR_TYPE_BUFFER << 2),
		SG_DESCRIPTOR_TYPE_RW_BUFFER_RAW = (SG_DESCRIPTOR_TYPE_RW_BUFFER | (SG_DESCRIPTOR_TYPE_RW_BUFFER << 1)),
		/// Uniform buffer
		SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER = (SG_DESCRIPTOR_TYPE_RW_BUFFER << 2),
		/// Push constant / Root constant
		SG_DESCRIPTOR_TYPE_ROOT_CONSTANT = (SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER << 1),
		/// IA
		SG_DESCRIPTOR_TYPE_VERTEX_BUFFER = (SG_DESCRIPTOR_TYPE_ROOT_CONSTANT << 1),
		SG_DESCRIPTOR_TYPE_INDEX_BUFFER = (SG_DESCRIPTOR_TYPE_VERTEX_BUFFER << 1),
		SG_DESCRIPTOR_TYPE_INDIRECT_BUFFER = (SG_DESCRIPTOR_TYPE_INDEX_BUFFER << 1),
		/// Cubemap SRV
		SG_DESCRIPTOR_TYPE_TEXTURE_CUBE = (SG_DESCRIPTOR_TYPE_TEXTURE | (SG_DESCRIPTOR_TYPE_INDIRECT_BUFFER << 1)),
		/// RTV / DSV per mip slice
		SG_DESCRIPTOR_TYPE_RENDER_TARGET_MIP_SLICES = (SG_DESCRIPTOR_TYPE_INDIRECT_BUFFER << 2),
		/// RTV / DSV per array slice
		SG_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES = (SG_DESCRIPTOR_TYPE_RENDER_TARGET_MIP_SLICES << 1),
		/// RTV / DSV per depth slice
		SG_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES = (SG_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES << 1),
		SG_DESCRIPTOR_TYPE_RAY_TRACING = (SG_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES << 1),
#if defined(SG_GRAPHIC_API_VULKAN)
		/// subpass input (descriptor type only available in Vulkan)
		SG_DESCRIPTOR_TYPE_INPUT_ATTACHMENT = (SG_DESCRIPTOR_TYPE_RAY_TRACING << 1),
		SG_DESCRIPTOR_TYPE_TEXEL_BUFFER = (SG_DESCRIPTOR_TYPE_INPUT_ATTACHMENT << 1),
		SG_DESCRIPTOR_TYPE_RW_TEXEL_BUFFER = (SG_DESCRIPTOR_TYPE_TEXEL_BUFFER << 1),
		SG_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = (SG_DESCRIPTOR_TYPE_RW_TEXEL_BUFFER << 1),
#endif
	} DescriptorType;
	SG_MAKE_ENUM_FLAG(uint32_t, DescriptorType);

	/// Choosing Memory Type
	typedef enum ResourceMemoryUsage
	{
		/// No intended memory usage specified.
		SG_RESOURCE_MEMORY_USAGE_UNKNOWN = 0,
		/// Memory will be used on device only, no need to be mapped on host.
		SG_RESOURCE_MEMORY_USAGE_GPU_ONLY = 1,
		/// Memory will be mapped on host. Could be used for transfer to device.
		SG_RESOURCE_MEMORY_USAGE_CPU_ONLY = 2,
		/// Memory will be used for frequent (dynamic) updates from host and reads on device.
		SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU = 3,
		/// Memory will be used for writing on device and readback on host.
		SG_RESOURCE_MEMORY_USAGE_GPU_TO_CPU = 4,
		SG_RESOURCE_MEMORY_USAGE_COUNT,
		SG_RESOURCE_MEMORY_USAGE_MAX_ENUM = 0x7FFFFFFF
	} ResourceMemoryUsage;

	typedef enum LoadActionType
	{
		SG_LOAD_ACTION_DONTCARE = 0,
		SG_LOAD_ACTION_LOAD,
		SG_LOAD_ACTION_CLEAR,
		SG_MAX_LOAD_ACTION
	} LoadActionType;

	typedef struct LoadActionsDesc
	{
		ClearValue     clearColorValues[SG_MAX_RENDER_TARGET_ATTACHMENTS];
		LoadActionType loadActionsColor[SG_MAX_RENDER_TARGET_ATTACHMENTS];
		ClearValue     clearDepth;
		LoadActionType loadActionDepth;
		LoadActionType loadActionStencil;
	} LoadActionsDesc;

	typedef enum PresentStatus
	{
		SG_PRESENT_STATUS_SUCCESS = 0,
		SG_PRESENT_STATUS_DEVICE_RESET = 1,
		SG_PRESENT_STATUS_FAILED = 2
	} PresentStatus;

	typedef enum CompareMode
	{
		SG_COMPARE_MODE_NEVER,
		SG_COMPARE_MODE_LESS,
		SG_COMPARE_MODE_EQUAL,
		SG_COMPARE_MODE_LEQUAL,
		SG_COMPARE_MODE_GREATER,
		SG_COMPARE_MODE_NOTEQUAL,
		SG_COMPARE_MODE_GEQUAL,
		SG_COMPARE_MODE_ALWAYS,
		SG_MAX_COMPARE_MODES,
	} CompareMode;

	typedef struct ReadRange
	{
		uint64_t offset;
		uint64_t size;
	} ReadRange;

	typedef enum DefaultResourceAlignment
	{
		SG_RESOURCE_BUFFER_ALIGNMENT = 4U,
	} DefaultResourceAlignment;

#pragma region (Blending)

	static const int BLEND_RED = 0x1;
	static const int BLEND_GREEN = 0x2;
	static const int BLEND_BLUE = 0x4;
	static const int BLEND_ALPHA = 0x8;
	static const int BLEND_ALL = (BLEND_RED | BLEND_GREEN | BLEND_BLUE | BLEND_ALPHA);
	static const int BLEND_NONE = 0;

	static const int BLEND_BS_NONE = -1;
	static const int BLEND_DS_NONE = -1;
	static const int BLEND_RS_NONE = -1;

	typedef enum BlendConstant // for blending
	{
		SG_BLEND_CONST_ZERO = 0,
		SG_BLEND_CONST_ONE,
		SG_BLEND_CONST_SRC_COLOR,
		SG_BLEND_CONST_ONE_MINUS_SRC_COLOR,
		SG_BLEND_CONST_DST_COLOR,
		SG_BLEND_CONST_ONE_MINUS_DST_COLOR,
		SG_BLEND_CONST_SRC_ALPHA,
		SG_BLEND_CONST_ONE_MINUS_SRC_ALPHA,
		SG_BLEND_CONST_DST_ALPHA,
		SG_BLEND_CONST_ONE_MINUS_DST_ALPHA,
		SG_BLEND_CONST_SRC_ALPHA_SATURATE,
		SG_BLEND_CONST_BLEND_FACTOR,
		SG_BLEND_CONST_ONE_MINUS_BLEND_FACTOR,
		SG_MAX_BLEND_CONSTANTS
	} BlendConstant;

	typedef enum BlendMode
	{
		SG_BLEND_MODE_ADD,
		SG_BLEND_MODE_SUBTRACT,
		SG_BLEND_MODE_REVERSE_SUBTRACT,
		SG_BLEND_MODE_MIN,
		SG_BLEND_MODE_MAX,
		SG_MAX_BLEND_MODES,
	} BlendMode;

	// blend states are always attached to one of the eight or more render targets that
	// are in a MRT
	// Mask constants
	typedef enum BlendStateTargets
	{
		SG_BLEND_STATE_TARGET_0 = 0x1,
		SG_BLEND_STATE_TARGET_1 = 0x2,
		SG_BLEND_STATE_TARGET_2 = 0x4,
		SG_BLEND_STATE_TARGET_3 = 0x8,
		SG_BLEND_STATE_TARGET_4 = 0x10,
		SG_BLEND_STATE_TARGET_5 = 0x20,
		SG_BLEND_STATE_TARGET_6 = 0x40,
		SG_BLEND_STATE_TARGET_7 = 0x80,
		SG_BLEND_STATE_TARGET_ALL = 0xFF,
	} BlendStateTargets;
	SG_MAKE_ENUM_FLAG(uint32_t, BlendStateTargets);

	typedef	enum BlendColorMask
	{
		SG_BLEND_COLOR_MASK_R = 0x1,
		SG_BLEND_COLOR_MASK_G = 0x2,
		SG_BLEND_COLOR_MASK_B = 0x4,
		SG_BLEND_COLOR_MASK_A = 0x8,
		SG_BLEND_COLOR_MASK_ALL = SG_BLEND_COLOR_MASK_R | SG_BLEND_COLOR_MASK_B | SG_BLEND_COLOR_MASK_G | SG_BLEND_COLOR_MASK_A,
		SG_BLEND_COLOR_MASK_MAX_ENUM = 0x7FFFFFFF
	} BlendColorMask;
	SG_MAKE_ENUM_FLAG(uint32_t, BlendColorMask);

	typedef struct BlendStateDesc
	{
		/// Source blend factor per render target.
		BlendConstant srcFactors[SG_MAX_RENDER_TARGET_ATTACHMENTS];
		/// Destination blend factor per render target.
		BlendConstant dstFactors[SG_MAX_RENDER_TARGET_ATTACHMENTS];
		/// Source alpha blend factor per render target.
		BlendConstant srcAlphaFactors[SG_MAX_RENDER_TARGET_ATTACHMENTS];
		/// Destination alpha blend factor per render target.
		BlendConstant dstAlphaFactors[SG_MAX_RENDER_TARGET_ATTACHMENTS];
		/// Blend mode per render target.
		BlendMode blendModes[SG_MAX_RENDER_TARGET_ATTACHMENTS];
		/// Alpha blend mode per render target.
		BlendMode blendAlphaModes[SG_MAX_RENDER_TARGET_ATTACHMENTS];
		/// Write mask per render target.
		BlendColorMask masks[SG_MAX_RENDER_TARGET_ATTACHMENTS];
		/// Mask that identifies the render targets affected by the blend state.
		BlendStateTargets renderTargetMask;
		/// Set whether alpha to coverage should be enabled.
		bool alphaToCoverage;
		/// Set whether each render target has an unique blend function. When false the blend function in slot 0 will be used for all render targets.
		bool independentBlend;
	} BlendStateDesc;

#pragma endregion (Blending)

#pragma region (Depth)

	typedef enum StencilOp
	{
		SG_STENCIL_OP_KEEP,
		SG_STENCIL_OP_SET_ZERO,
		SG_STENCIL_OP_REPLACE,
		SG_STENCIL_OP_INVERT,
		SG_STENCIL_OP_INCR,
		SG_STENCIL_OP_DECR,
		SG_STENCIL_OP_INCR_SAT,
		SG_STENCIL_OP_DECR_SAT,
		SG_MAX_STENCIL_OPS,
	} StencilOp;

	typedef struct DepthStateDesc
	{
		bool        depthTest;
		bool        depthWrite;
		CompareMode depthFunc;
		bool        stencilTest;
		uint8_t     stencilReadMask;
		uint8_t     stencilWriteMask;
		CompareMode stencilFrontFunc;
		StencilOp   stencilFrontFail;
		StencilOp   depthFrontFail;
		StencilOp   stencilFrontPass;
		CompareMode stencilBackFunc;
		StencilOp   stencilBackFail;
		StencilOp   depthBackFail;
		StencilOp   stencilBackPass;
	} DepthStateDesc;

#pragma endregion (Depth)

#pragma region (Rasterization)

	typedef enum CullMode
	{
		SG_CULL_MODE_NONE = 0,
		SG_CULL_MODE_BACK,
		SG_CULL_MODE_FRONT,
		SG_CULL_MODE_BOTH,
		SG_MAX_CULL_MODES
	} CullMode;

	typedef enum FrontFace
	{
		SG_FRONT_FACE_CCW = 0,
		SG_FRONT_FACE_CW
	} FrontFace;

	typedef enum FillMode
	{
		SG_FILL_MODE_SOLID,
		SG_FILL_MODE_WIREFRAME,
		SG_MAX_FILL_MODES
	} FillMode;

	typedef struct RasterizerStateDesc
	{
		CullMode  cullMode;
		int32_t   depthBias;
		float     slopeScaledDepthBias;
		FillMode  fillMode;
		bool      multiSample;
		bool      scissor;
		FrontFace frontFace;
		bool      depthClampEnable;
	} RasterizerStateDesc;

#pragma endregion (Rasterization)

} // namespace SG

#include "IShaderReflection.h"

namespace SG
{

	// forward declarations
	typedef struct Renderer              Renderer;
	typedef struct Queue                 Queue;
	typedef struct Pipeline              Pipeline;
	typedef struct Buffer                Buffer;
	typedef struct Texture               Texture;
	typedef struct RenderTarget          RenderTarget;
	typedef struct Shader                Shader;
	typedef struct DescriptorSet         DescriptorSet;
	typedef struct DescriptorIndexMap    DescriptorIndexMap;
	typedef struct PipelineCache         PipelineCache;

	// Raytracing
	typedef struct Raytracing            Raytracing;
	typedef struct RaytracingHitGroup    RaytracingHitGroup;
	typedef struct AccelerationStructure AccelerationStructure;

	typedef struct EsramManager          EsramManager;

#pragma region (Indirect Draw)

	typedef struct IndirectDrawArguments // vertex data
	{
		uint32_t vertexCount;
		uint32_t instanceCount;
		uint32_t startVertex;
		uint32_t startInstance;
	} IndirectDrawArguments;

	typedef struct IndirectDrawIndexArguments // index data(for drawing call)
	{
		uint32_t indexCount;
		uint32_t instanceCount;
		uint32_t startIndex;
		uint32_t vertexOffset;
		uint32_t startInstance;
	} IndirectDrawIndexArguments;

	typedef struct IndirectDispatchArguments
	{
		uint32_t groupCountX;
		uint32_t groupCountY;
		uint32_t groupCountZ;
	} IndirectDispatchArguments;

	typedef enum IndirectArgumentType
	{
		SG_INDIRECT_DRAW,
		SG_INDIRECT_DRAW_INDEX,
		SG_INDIRECT_DISPATCH,
		SG_INDIRECT_VERTEX_BUFFER,
		SG_INDIRECT_INDEX_BUFFER,
		SG_INDIRECT_CONSTANT,
		SG_INDIRECT_DESCRIPTOR_TABLE,        // only for vulkan
		SG_INDIRECT_PIPELINE,                // only for vulkan now, probally will add to dx when it comes to xbox
		SG_INDIRECT_CONSTANT_BUFFER_VIEW,    // only for dx
		SG_INDIRECT_SHADER_RESOURCE_VIEW,    // only for dx
		SG_INDIRECT_UNORDERED_ACCESS_VIEW,   // only for dx
	} IndirectArgumentType;

#pragma endregion (Indirect Draw)

#pragma region (GPU relative)

	typedef enum GPUMode
	{
		SG_GPU_MODE_SINGLE = 0,
		SG_GPU_MODE_LINKED,
		// #TODO GPU_MODE_UNLINKED,
	} GPUMode;

	typedef enum GPUPresetLevel
	{
		SG_GPU_PRESET_NONE = 0,
		SG_GPU_PRESET_OFFICE,    // this means unsupported
		SG_GPU_PRESET_LOW,
		SG_GPU_PRESET_MEDIUM,
		SG_GPU_PRESET_HIGH,
		SG_GPU_PRESET_ULTRA,
		SG_GPU_PRESET_COUNT
	} GPUPresetLevel;

	typedef enum WaveOpsSupportFlags
	{
		SG_WAVE_OPS_SUPPORT_FLAG_NONE = 0x0,
		SG_WAVE_OPS_SUPPORT_FLAG_BASIC_BIT = 0x00000001,
		SG_WAVE_OPS_SUPPORT_FLAG_VOTE_BIT = 0x00000002,
		SG_WAVE_OPS_SUPPORT_FLAG_ARITHMETIC_BIT = 0x00000004,
		SG_WAVE_OPS_SUPPORT_FLAG_BALLOT_BIT = 0x00000008,
		SG_WAVE_OPS_SUPPORT_FLAG_SHUFFLE_BIT = 0x00000010,
		SG_WAVE_OPS_SUPPORT_FLAG_SHUFFLE_RELATIVE_BIT = 0x00000020,
		SG_WAVE_OPS_SUPPORT_FLAG_CLUSTERED_BIT = 0x00000040,
		SG_WAVE_OPS_SUPPORT_FLAG_QUAD_BIT = 0x00000080,
		SG_WAVE_OPS_SUPPORT_FLAG_PARTITIONED_BIT_NV = 0x00000100,
		SG_WAVE_OPS_SUPPORT_FLAG_ALL = 0x7FFFFFFF
	} WaveOpsSupportFlags;
	SG_MAKE_ENUM_FLAG(uint32_t, WaveOpsSupportFlags);

	typedef struct GPUVendorPreset
	{
		char           vendorId[SG_MAX_GPU_VENDOR_STRING_LENGTH];
		char           modelId[SG_MAX_GPU_VENDOR_STRING_LENGTH];
		char           revisionId[SG_MAX_GPU_VENDOR_STRING_LENGTH];    // optional as not all gpus' have that. Default is : 0x00
		GPUPresetLevel presetLevel;
		char           gpuName[SG_MAX_GPU_VENDOR_STRING_LENGTH];       // if GPU Name is missing then value will be empty string
		char           gpuDriverVersion[SG_MAX_GPU_VENDOR_STRING_LENGTH];
		char           gpuDriverDate[SG_MAX_GPU_VENDOR_STRING_LENGTH];
	} GPUVendorPreset;

	typedef struct GPUCapBits
	{
		bool canShaderReadFrom[TinyImageFormat_Count];
		bool canShaderWriteTo[TinyImageFormat_Count];
		bool canRenderTargetWriteTo[TinyImageFormat_Count];
	} GPUCapBits;

	typedef struct GPUSettings
	{
		uint32_t            uniformBufferAlignment;
		uint32_t            uploadBufferTextureAlignment;
		uint32_t            uploadBufferTextureRowAlignment;
		uint32_t            maxVertexInputBindings;
		uint32_t            maxRootSignatureDWORDS;
		uint32_t            waveLaneCount;
		WaveOpsSupportFlags waveOpsSupportFlags;
		GPUVendorPreset     gpuVendorPreset;

		uint32_t            multiDrawIndirect : 1;
		uint32_t            ROVsSupported : 1;
		uint32_t            tessellationSupported : 1;
		uint32_t            geometryShaderSupported : 1;
#if defined(SG_GRAPHIC_API_GLES)
		uint32_t            maxTextureImageUnits;
#endif
	} GPUSettings;

#pragma endregion (GPU relative)

#pragma region (Texture)

	typedef enum TextureCreationFlags
	{
		/// Default flag (Texture will use default allocation strategy decided by the api specific allocator)
		SG_TEXTURE_CREATION_FLAG_NONE = 0,
		/// Texture will allocate its own memory (COMMITTED resource)
		SG_TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT = 0x01,
		/// Texture will be allocated in memory which can be shared among multiple processes
		SG_TEXTURE_CREATION_FLAG_EXPORT_BIT = 0x02,
		/// Texture will be allocated in memory which can be shared among multiple gpus
		SG_TEXTURE_CREATION_FLAG_EXPORT_ADAPTER_BIT = 0x04,
		/// Texture will be imported from a handle created in another process
		SG_TEXTURE_CREATION_FLAG_IMPORT_BIT = 0x08,
		/// Use ESRAM to store this texture
		SG_TEXTURE_CREATION_FLAG_ESRAM = 0x10,
		/// Use on-tile memory to store this texture
		SG_TEXTURE_CREATION_FLAG_ON_TILE = 0x20,
		/// Prevent compression meta data from generating (XBox)
		SG_TEXTURE_CREATION_FLAG_NO_COMPRESSION = 0x40,
		/// Force 2D instead of automatically determining dimension based on width, height, depth
		SG_TEXTURE_CREATION_FLAG_FORCE_2D = 0x80,
		/// Force 3D instead of automatically determining dimension based on width, height, depth
		SG_TEXTURE_CREATION_FLAG_FORCE_3D = 0x100,
		/// Display target
		SG_TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET = 0x200,
		/// Create an sRGB texture.
		SG_TEXTURE_CREATION_FLAG_SRGB = 0x400,
	} TextureCreationFlags;
	SG_MAKE_ENUM_FLAG(uint32_t, TextureCreationFlags);

	/// Data structure holding necessary info to create a Texture
	typedef struct TextureCreateDesc
	{
		/// Texture creation flags (decides memory allocation strategy, sharing access,...)
		TextureCreationFlags flags;
		/// Width
		uint32_t width;
		/// Height
		uint32_t height;
		/// Depth (Should be 1 if not a mType is not TEXTURE_TYPE_3D)
		uint32_t depth;
		/// Texture array size (Should be 1 if texture is not a texture array or cubemap)
		uint32_t arraySize;
		/// Number of mip levels
		uint32_t mipLevels;
		/// Number of multisamples per pixel (currently Textures created with mUsage TEXTURE_USAGE_SAMPLED_IMAGE only support SAMPLE_COUNT_1)
		SampleCount sampleCount;
		/// The image quality level. The higher the quality, the lower the performance. The valid range is between zero and the value appropriate for mSampleCount
		uint32_t sampleQuality;
		///  image format
		TinyImageFormat format;
		/// Optimized clear value (recommended to use this same value when clearing the rendertarget)
		ClearValue clearValue;
		/// What state will the texture get created in
		ResourceState startState;
		/// Descriptor creation
		DescriptorType descriptors;
		/// Pointer to native texture handle if the texture does not own underlying resource
		const void* nativeHandle;
		/// Debug name used in gpu profile
		const char* name;
		/// GPU indices to share this texture
		uint32_t* pSharedNodeIndices;
		/// Number of GPUs to share this texture
		uint32_t sharedNodeIndexCount;
		/// GPU which will own this texture
		uint32_t nodeIndex;
#if defined(SG_GRAPHIC_API_VULKAN)
		VkSamplerYcbcrConversionInfo* pVkSamplerYcbcrConversionInfo;
#endif
	} TextureCreateDesc;

	// virtual texture page as a part of the partially resident texture
	// contains memory bindings, offsets and status information
	struct VirtualTexturePage
	{
		/// Buffer which contains the image data and be used for copying it to Virtual texture
		Buffer* pIntermediateBuffer;
		/// Miplevel for this page
		uint32_t mipLevel;
		/// Array layer for this page
		uint32_t layer;
		/// Index for this page
		uint32_t index;
#if defined(SG_GRAPHIC_API_D3D12)
		/// Offset for this page
		D3D12_TILED_RESOURCE_COORDINATE offset;
		/// Size for this page
		D3D12_TILED_RESOURCE_COORDINATE extent;
		/// Byte size for this page
		uint32_t size;
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		/// Offset for this page
		VkOffset3D offset;
		/// Size for this page
		VkExtent3D extent;
		/// Sparse image memory bind for this page
		VkSparseImageMemoryBind imageMemoryBind;
		/// Byte size for this page
		VkDeviceSize size;
#endif
	};

	typedef struct VirtualTexture
	{
#if defined(SG_GRAPHIC_API_D3D12)
		ID3D12Heap* pSparseImageMemory;
		/// Array for Sparse texture's pages
		void* pSparseCoordinates;
		/// Array for heap memory offsets
		void* pHeapRangeStartOffsets;
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		/// Sparse queue binding information
		VkBindSparseInfo bindSparseInfo;
		/// Sparse image memory bindings of all memory-backed virtual tables
		void* pSparseImageMemoryBinds;
		/// Sparse ?aque memory bindings for the mip tail (if present)	
		void* pOpaqueMemoryBinds;
		/// First mip level in mip tail
		uint32_t mipTailStart;
		/// Lstly filled mip level in mip tail
		uint32_t lastFilledMip;
		/// Memory type for Sparse texture's memory
		uint32_t sparseMemoryTypeIndex;
		/// Sparse image memory bind info 
		VkSparseImageMemoryBindInfo imageMemoryBindInfo;
		/// Sparse image opaque memory bind info (mip tail)
		VkSparseImageOpaqueMemoryBindInfo opaqueMemoryBindInfo;
#endif
		/// Virtual Texture members
		/// Contains all virtual pages of the texture
		void* pPages;
		/// Visibility data
		Buffer* visibility;
		/// PrevVisibility data
		Buffer* prevVisibility;
		/// Alive Page's Index
		Buffer* alivePage;
		/// Page's Index which should be removed
		Buffer* removePage;
		/// a { uint alive; uint remove; } count of pages which are alive or should be removed
		Buffer* pageCounts;
		/// Original Pixel image data
		void* virtualImageData;
		///  Total pages count
		uint32_t virtualPageTotalCount;
		/// Sparse Virtual Texture Width
		uint64_t sparseVirtualTexturePageWidth;
		/// Sparse Virtual Texture Height
		uint64_t sparseVirtualTexturePageHeight;
	} VirtualTexture;

	typedef struct SG_ALIGN(Texture, 64)
	{
#if defined(SG_GRAPHIC_API_D3D12)
		/// Descriptor handle of the SRV in a CPU visible descriptor heap (applicable to TEXTURE_USAGE_SAMPLED_IMAGE)
		D3D12_CPU_DESCRIPTOR_HANDLE  mDxDescriptorHandles;
		/// Native handle of the underlying resource
		ID3D12Resource* pDxResource;
		/// Contains resource allocation info such as parent heap, offset in heap
		D3D12MA::Allocation* pDxAllocation;
		uint64_t                     mHandleCount : 24;
		uint64_t                     mUavStartIndex : 1;
		uint32_t                     mDescriptorSize;
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		/// Opaque handle used by shaders for doing read/write operations on the texture
		VkImageView                  pVkSRVDescriptor;
		/// Opaque handle used by shaders for doing read/write operations on the texture
		VkImageView* pVkUAVDescriptors;
		/// Opaque handle used by shaders for doing read/write operations on the texture
		VkImageView                  pVkSRVStencilDescriptor;
		/// Native handle of the underlying resource
		VkImage                      pVkImage;
		union
		{
			/// Contains resource allocation info such as parent heap, offset in heap
			VmaAllocation			vkAllocation;
			VkDeviceMemory          pVkDeviceMemory;
		};
#endif
#if defined(SG_GRAPHIC_API_D3D11)
		ID3D11Resource* pDxResource;
		ID3D11ShaderResourceView* pDxSRVDescriptor;
		ID3D11UnorderedAccessView** pDxUAVDescriptors;
		uint64_t                     mPadA;
		uint64_t                     mPadB;
#endif
#if defined(SG_GRAPHIC_API_GLES)
		GLuint						 mTexture;
		GLenum						 mTarget;
		GLenum						 mGlFormat;
		GLenum						 mInternalFormat;
		GLenum						 mType;
#endif
		VirtualTexture* pSvt;
		/// Current state of the buffer
		uint32_t                     width : 16;
		uint32_t                     height : 16;
		uint32_t                     depth : 16;
		uint32_t                     mipLevels : 5;
		uint32_t                     arraySizeMinusOne : 11;
		uint32_t                     format : 8;
		/// Flags specifying which aspects (COLOR,DEPTH,STENCIL) are included in the pVkImageView
		uint32_t                     aspectMask : 4;
		uint32_t                     nodeIndex : 4;
		uint32_t                     uav : 1;
		/// This value will be false if the underlying resource is not owned by the texture (swapchain textures,...)
		uint32_t                     ownsImage : 1;
	} Texture;
	SG_COMPILE_ASSERT(sizeof(Texture) == 8 * sizeof(uint64_t));

#pragma endregion (Texture)

#pragma region (Renderer Target)

	typedef struct RenderTargetCreateDesc
	{
		/// Texture creation flags (decides memory allocation strategy, sharing access,...)
		TextureCreationFlags flags;
		/// Width
		uint32_t width;
		/// Height
		uint32_t height;
		/// Depth (Should be 1 if not a mType is not TEXTURE_TYPE_3D)
		uint32_t depth;
		/// Texture array size (Should be 1 if texture is not a texture array or cubemap)
		uint32_t arraySize;
		/// Number of mip levels
		uint32_t mipLevels;
		/// MSAA
		SampleCount sampleCount;
		/// Internal image format
		TinyImageFormat format;
		/// What state will the texture get created in
		ResourceState startState;
		/// Optimized clear value (recommended to use this same value when clearing the rendertarget)
		ClearValue clearValue;
		/// The image quality level. The higher the quality, the lower the performance. The valid range is between zero and the value appropriate for mSampleCount
		uint32_t sampleQuality;
		/// Descriptor creation
		DescriptorType descriptors;

		const void* pNativeHandle;
		/// Debug name used in gpu profile
		const char* name;
		/// GPU indices to share this texture
		uint32_t* pSharedNodeIndices;
		/// Number of GPUs to share this texture
		uint32_t sharedNodeIndexCount;
		/// GPU which will own this texture
		uint32_t nodeIndex;
	} RenderTargetCreateDesc;

	typedef struct SG_ALIGN(RenderTarget, 64)
	{
		Texture* pTexture;
#if defined(SG_GRAPHIC_API_D3D12)
		D3D12_CPU_DESCRIPTOR_HANDLE   mDxDescriptors;
		uint32_t                      mDxDescriptorSize;
		uint32_t                      mPadA;
		uint64_t                      mPadB;
		uint64_t                      mPadC;
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		VkImageView                   pVkDescriptor;
		VkImageView*	              pVkSliceDescriptors;
		uint32_t                      id;
		volatile uint32_t             used;
#endif
#if defined(SG_GRAPHIC_API_D3D11)
		union
		{
			/// Resources
			ID3D11RenderTargetView* pDxRtvDescriptor;
			ID3D11DepthStencilView* pDxDsvDescriptor;
		};
		union
		{
			/// Resources
			ID3D11RenderTargetView** pDxRtvSliceDescriptors;
			ID3D11DepthStencilView** pDxDsvSliceDescriptors;
		};
		uint64_t                      mPadA;
#endif
#if defined(SG_GRAPHIC_API_GLES)
		GLuint						  mRenderBuffer;
		GLuint						  mType;
#endif
		ClearValue                    clearValue;
		uint16_t                      arraySize;
		uint16_t                      depth;
		uint16_t                      width;
		uint16_t                      height;
		uint32_t                      descriptors : 20;
		uint32_t                      mipLevels : 10;
		uint32_t                      sampleQuality : 5;
		TinyImageFormat               format;
		SampleCount                   sampleCount;
	} RenderTarget;
	SG_COMPILE_ASSERT(sizeof(RenderTarget) <= 32 * sizeof(uint64_t));

#pragma endregion (Renderer Target)

#pragma region (Semaphore And Fence)

	typedef enum FenceStatus
	{
		SG_FENCE_STATUS_COMPLETE = 0,
		SG_FENCE_STATUS_INCOMPLETE,
		SG_FENCE_STATUS_NOTSUBMITTED,
	} FenceStatus;

	typedef struct Fence
	{
#if defined(SG_GRAPHIC_API_D3D12)
		ID3D12Fence* pDxFence;
		HANDLE               pDxWaitIdleFenceEvent;
		uint64_t             fenceValue;
		uint64_t             padA;
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		VkFence              pVkFence;
		uint32_t             submitted : 1;
		uint32_t             padA;
		uint64_t             padB;
		uint64_t             padC;
#endif
#if defined(SG_GRAPHIC_API_D3D11)
		ID3D11Query* pDX11Query;
		uint32_t             submitted : 1;
		uint32_t             padA;
		uint64_t             padB;
		uint64_t             padC;
#endif
#if defined(SG_GRAPHIC_API_GLES)
		uint32_t             submitted : 1;
#endif
	} Fence;

	typedef struct Semaphore
	{
#if defined(SG_GRAPHIC_API_D3D12)
		// DirectX12 does not have a concept of semaphores
		// All synchronization is done using fences
		// simulate semaphore signal and wait using DirectX12 fences

		// Semaphores used in DirectX12 only in queueSubmit
		// queueSubmit -> How the semaphores work in DirectX12

		// pp_wait_semaphores -> queue->Wait is manually called on each fence in this
		// array before calling ExecuteCommandLists to make the fence work like a wait semaphore

		// pp_signal_semaphores -> Manually call queue->Signal on each fence in this array after
		// calling ExecuteCommandLists and increment the underlying fence value

		// queuePresent does not use the wait semaphore since the swapchain Present function
		// already does the synchronization in this case
		ID3D12Fence* pDxFence;
		HANDLE               pDxWaitIdleFenceEvent;
		uint64_t             fenceValue;
		uint64_t             padA;
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		VkSemaphore          pVkSemaphore;
		uint32_t             currentNodeIndex : 5;
		uint32_t             signaled : 1;
		uint32_t             padA;
		uint64_t             padB;
		uint64_t             padC;
#endif
#if defined(SG_GRAPHIC_API_GLES)
		uint32_t             signaled : 1;
#endif
	} Semaphore;

#pragma endregion (Semaphore And Fence)

#pragma region (SwapChain)

	typedef struct SwapChainCreateDesc
	{
		/// Window handle
		WindowHandle windowHandle;
		/// Queues which should be allowed to present
		Queue** ppPresentQueues;
		/// Number of present queues
		uint32_t presentQueueCount;
		/// Number of backbuffers in this swapchain
		uint32_t imageCount;
		/// Width of the swapchain
		uint32_t width;
		/// Height of the swapchain
		uint32_t height;
		/// Color format of the swapchain
		TinyImageFormat colorFormat;
		/// Clear value
		ClearValue colorClearValue;
		/// Set whether swap chain will be presented using vsync
		bool enableVsync;
		/// We can toggle to using FLIP model if app desires.
		bool useFlipSwapEffect;
	} SwapChainCreateDesc;

	typedef struct SwapChain
	{
		/// Render targets created from the swapchain back buffers
		RenderTarget** ppRenderTargets;
#if defined(XBOX)
		uint64_t                 mFramePipelineToken;
		/// Sync interval to specify how interval for vsync
		uint32_t                 mDxSyncInterval : 3;
		uint32_t                 mFlags : 10;
		uint32_t                 mImageCount : 3;
		uint32_t                 mEnableVsync : 1;
		uint32_t                 mIndex;
		void* pWindow;
		Queue* pPresentQueue;
		uint64_t                 mPadB[5];
#elif defined(SG_GRAPHIC_API_D3D12)
		/// Use IDXGISwapChain3 for now since IDXGISwapChain4
		/// isn't supported by older devices.
		IDXGISwapChain3* pDxSwapChain;
		/// Sync interval to specify how interval for vsync
		uint32_t                 mDxSyncInterval : 3;
		uint32_t                 mFlags : 10;
		uint32_t                 mImageCount : 3;
		uint32_t                 mEnableVsync : 1;
		uint32_t                 mPadA;
		uint64_t                 mPadB[5];
#endif
#if defined(SG_GRAPHIC_API_D3D11)
		/// Use IDXGISwapChain3 for now since IDXGISwapChain4
		/// isn't supported by older devices.
		IDXGISwapChain* pDxSwapChain;
		/// Sync interval to specify how interval for vsync
		uint32_t                 mDxSyncInterval : 3;
		uint32_t                 mFlags : 10;
		uint32_t                 mImageCount : 3;
		uint32_t                 mImageIndex : 3;
		uint32_t                 mEnableVsync : 1;
		DXGI_SWAP_EFFECT         mSwapEffect;
		uint32_t                 mPadA;
		uint64_t                 mPadB[5];
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		/// Present queue if one exists (queuePresent will use this queue if the hardware has a dedicated present queue)
		VkQueue                  pPresentQueue;
		VkSwapchainKHR           pSwapChain;
		VkSurfaceKHR             pVkSurface;
		SwapChainCreateDesc*	 pDesc;
		uint32_t                 presentQueueFamilyIndex : 5;
		uint32_t                 imageCount : 3;
		uint32_t                 enableVsync : 1;
		uint32_t                 padA;
#endif
#if defined(SG_GRAPHIC_API_GLES)
		GLSurface				 pSurface;
		uint32_t                 mImageCount : 3;
		uint32_t                 mImageIndex : 3;
		uint32_t                 mEnableVsync : 1;
#endif
	} SwapChain;

#pragma endregion (SwapChain)

#pragma region (Command Pool And Cmd)

	typedef struct CmdPoolCreateDesc
	{
		Queue* pQueue;
		bool   transient;
	} CmdPoolCreateDesc;

	typedef struct CmdPool
	{
#if defined(SG_GRAPHIC_API_D3D12)
		ID3D12CommandAllocator* pDxCmdAlloc;
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		VkCommandPool           pVkCmdPool;
#endif
#if defined(SG_GRAPHIC_API_GLES)
		struct CmdCache* pCmdCache;
#endif
		Queue* pQueue;
	} CmdPool;

	typedef struct CmdCreateDesc
	{
		CmdPool* pPool;
		bool     secondary;
	} CmdCreateDesc;

	typedef struct SG_ALIGN(Cmd, 64)
	{
#if defined(SG_GRAPHIC_API_D3D12)
#if defined(XBOX)
		DmaCmd                       mDma;
#endif
		ID3D12GraphicsCommandList* pDxCmdList;

		// Cached in beginCmd to avoid fetching them during rendering
		struct DescriptorHeap* pBoundHeaps[2];
		D3D12_GPU_DESCRIPTOR_HANDLE  mBoundHeapStartHandles[2];

		// Command buffer state
		const ID3D12RootSignature* pBoundRootSignature;
		DescriptorSet* pBoundDescriptorSets[DESCRIPTOR_UPDATE_FREQ_COUNT];
		uint16_t                     boundDescriptorSetIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
		uint32_t                     nodeIndex : 4;
		uint32_t                     type : 3;
		CmdPool* pCmdPool;
		uint32_t                     padA;
#if !defined(XBOX)
		uint64_t                     padB;
#endif
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		VkCommandBuffer              pVkCmdBuf;
		VkRenderPass                 pVkActiveRenderPass;
		VkPipelineLayout             pBoundPipelineLayout;
		uint32_t                     nodeIndex : 4;
		uint32_t                     type : 3;
		uint32_t                     padA;
		CmdPool* pCmdPool;
		uint64_t                     padB[9];
#endif
#if defined(SG_GRAPHIC_API_D3D11)
		ID3D11Buffer* pRootConstantBuffer;
		ID3D11Buffer* pTransientConstantBuffer;
		uint8_t* pDescriptorCache;
		uint32_t                     mDescriptorCacheOffset;
		uint32_t                     mPadA;
		uint64_t                     mPadB[10];
#endif
#if defined(SG_GRAPHIC_API_GLES)
		CmdPool* pCmdPool;
#endif
		Renderer* pRenderer;
		Queue* pQueue;
	} Cmd;
	SG_COMPILE_ASSERT(sizeof(Cmd) <= 64 * sizeof(uint64_t));

#pragma endregion (Command Pool And Cmd)

#pragma region (Queue)

	typedef enum QueueType
	{
		SG_QUEUE_TYPE_GRAPHICS = 0,
		SG_QUEUE_TYPE_TRANSFER,
		SG_QUEUE_TYPE_COMPUTE,
		SG_MAX_QUEUE_TYPE
	} QueueType;

	typedef enum QueueFlag
	{
		SG_QUEUE_FLAG_NONE = 0x0,
		SG_QUEUE_FLAG_DISABLE_GPU_TIMEOUT = 0x1,
		SG_QUEUE_FLAG_INIT_MICROPROFILE = 0x2,
		SG_MAX_QUEUE_FLAG = 0xFFFFFFFF
	} QueueFlag;
	SG_MAKE_ENUM_FLAG(uint32_t, QueueFlag);

	typedef enum QueuePriority
	{
		SG_QUEUE_PRIORITY_NORMAL = 0,
		SG_QUEUE_PRIORITY_HIGH,
		SG_QUEUE_PRIORITY_GLOBAL_REALTIME,
		SG_MAX_QUEUE_PRIORITY
	} QueuePriority;

	typedef struct QueueCreateDesc
	{
		QueueType     type;
		QueueFlag     flag;
		QueuePriority priority;
		uint32_t      nodeIndex;
	} QueueCreateDesc;

	typedef struct Queue
	{
#if defined(SG_GRAPHIC_API_D3D12)
		ID3D12CommandQueue* pDxQueue;
		uint32_t             type : 3;
		uint32_t             nodeIndex : 4;
		Fence* pFence;
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		VkQueue             pVkQueue;
		Mutex*				pSubmitMutex;
		uint32_t            vkQueueFamilyIndex : 5;
		uint32_t            vkQueueIndex : 5;
		uint32_t            type : 3;
		uint32_t            nodeIndex : 4;
		uint32_t            gpuMode : 3;
		uint32_t            flags;
		float               timestampPeriod;
#endif
#if defined(SG_GRAPHIC_API_D3D11)
		ID3D11Device* pDxDevice;
		ID3D11DeviceContext* pDxContext;
		uint32_t             mType : 3;
		uint32_t             nodeIndex : 4;
		Fence* pFence;
#endif
#if defined(SG_GRAPHIC_API_GLES)
		uint32_t             mType : 3;
		uint32_t             nodeIndex : 4;
#endif
	} Queue;

#pragma region (Submit And Present)

	typedef struct QueueSubmitDesc
	{
		uint32_t    cmdCount;
		Cmd** ppCmds;
		Fence* pSignalFence;
		uint32_t    waitSemaphoreCount;
		Semaphore** ppWaitSemaphores;
		uint32_t    signalSemaphoreCount;
		Semaphore** ppSignalSemaphores;
		bool        submitDone;
	} QueueSubmitDesc;

	typedef struct QueuePresentDesc
	{
		SwapChain* pSwapChain;
		uint32_t    waitSemaphoreCount;
		Semaphore** ppWaitSemaphores;
		uint8_t     index;
		bool        submitDone;
	} QueuePresentDesc;

#pragma endregion (Submit And Present)

#pragma endregion (Queue)

#pragma region (Buffer)

	typedef enum BufferCreationFlags
	{
		/// Default flag (Buffer will use aliased memory, buffer will not be cpu accessible until mapBuffer is called)
		SG_BUFFER_CREATION_FLAG_NONE = 0x01,
		/// Buffer will allocate its own memory (COMMITTED resource)
		SG_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT = 0x02,
		/// Buffer will be persistently mapped
		SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT = 0x04,
		/// Use ESRAM to store this buffer
		SG_BUFFER_CREATION_FLAG_ESRAM = 0x08,
		/// Flag to specify not to allocate descriptors for the resource
		SG_BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION = 0x10,
	} BufferCreationFlags;
	SG_MAKE_ENUM_FLAG(uint32_t, BufferCreationFlags);

	/// Data structure holding necessary info to create a Buffer
	typedef struct BufferCreateDesc
	{
		/// Size of the buffer (in bytes)
		uint64_t size;
		/// Alignment
		uint32_t alignment;
		/// Decides which memory heap buffer will use (default, upload, readback)
		ResourceMemoryUsage memoryUsage;
		/// Creation flags of the buffer
		BufferCreationFlags flags;
		/// What type of queue the buffer is owned by
		QueueType queueType;
		/// What state will the buffer get created in
		ResourceState startState;
		/// Index of the first element accessible by the SRV/UAV (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
		uint64_t firstElement;
		/// Number of elements in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
		uint64_t elementCount;
		/// Size of each element (in bytes) in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
		uint64_t structStride;
		/// ICB draw type
		IndirectArgumentType ICBDrawType;
		/// ICB max vertex buffers slots count
		uint32_t ICBMaxVertexBufferBind;
		/// ICB max vertex buffers slots count
		uint32_t ICBMaxFragmentBufferBind;
		/// Set this to specify a counter buffer for this buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
		struct Buffer* counterBuffer;
		/// Format of the buffer (applicable to typed storage buffers (Buffer<T>)
		TinyImageFormat format;
		/// Flags specifying the suitable usage of this buffer (Uniform buffer, Vertex Buffer, Index Buffer,...)
		DescriptorType descriptors;
		/// Debug name used in gpu profile
		const char* name;
		uint32_t* pSharedNodeIndices;
		uint32_t       nodeIndex;
		uint32_t       sharedNodeIndexCount;
	} BufferCreateDesc;

	typedef struct SG_ALIGN(Buffer, 64)
	{
		/// CPU address of the mapped buffer (appliacable to buffers created in CPU accessible heaps (CPU, CPU_TO_GPU, GPU_TO_CPU)
		void* pCpuMappedAddress;
#if defined(SG_GRAPHIC_API_D3D12)
		/// GPU Address - Cache to avoid calls to ID3D12Resource::GetGpuVirtualAddress
		D3D12_GPU_VIRTUAL_ADDRESS        mDxGpuAddress;
		/// Descriptor handle of the CBV in a CPU visible descriptor heap (applicable to BUFFER_USAGE_UNIFORM)
		D3D12_CPU_DESCRIPTOR_HANDLE      mDxDescriptorHandles;
		/// Offset from mDxDescriptors for srv descriptor handle
		uint64_t                         mDxSrvOffset : 8;
		/// Offset from mDxDescriptors for uav descriptor handle
		uint64_t                         mDxUavOffset : 8;
		/// Native handle of the underlying resource
		ID3D12Resource* pDxResource;
		/// Contains resource allocation info such as parent heap, offset in heap
		D3D12MA::Allocation* pDxAllocation;
#endif
#if defined(SG_GRAPHIC_API_D3D11)
		ID3D11Buffer* pDxResource;
		ID3D11ShaderResourceView* pDxSrvHandle;
		ID3D11UnorderedAccessView* pDxUavHandle;
		uint64_t                         mPadA;
		uint64_t                         mPadB;
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		/// Native handle of the underlying resource
		VkBuffer                         pVkBuffer;
		/// Buffer view
		VkBufferView                     pVkStorageTexelView;
		VkBufferView                     pVkUniformTexelView;
		/// Contains resource allocation info such as parent heap, offset in heap
		VmaAllocation				     vkAllocation;
		uint64_t                         offset;
#endif
#if defined(SG_GRAPHIC_API_GLES)
		GLuint							 buffer;
		GLenum							 target;
		void* pGLCpuMappedAddress;
#endif
		uint64_t                         size : 32;
		uint64_t                         descriptors : 20;
		uint64_t                         memoryUsage : 3;
		uint64_t                         nodeIndex : 4;
	} Buffer;
	// check for the alignment
	SG_COMPILE_ASSERT(sizeof(Buffer) == 8 * sizeof(uint64_t));

#pragma endregion (Buffer)

#pragma region (Barriers)

	// barriers for resource barrier transform
	typedef struct BufferBarrier
	{
		Buffer* pBuffer;
		ResourceState  currentState;
		ResourceState  newState;
		uint8_t		   beginOnly : 1;
		uint8_t        endOnly : 1;
		uint8_t        acquire : 1;
		uint8_t        release : 1;
		uint8_t        queueType : 5;
	} BufferBarrier;

	typedef struct TextureBarrier
	{
		Texture* pTexture;
		ResourceState  currentState;
		ResourceState  newState;
		uint8_t        beginOnly : 1;
		uint8_t        endOnly : 1;
		uint8_t        acquire : 1;
		uint8_t        release : 1;
		uint8_t        queueType : 5;
		/// specified whether following barrier targets particular subresource
		uint8_t        subresourceBarrier : 1;
		/// following values are ignored if subresourceBarrier is false
		uint8_t        mipLevel : 7;
		uint16_t       arrayLayer;
	} TextureBarrier;

	typedef struct RenderTargetBarrier
	{
		RenderTarget* pRenderTarget;
		ResourceState  currentState;
		ResourceState  newState;
		uint8_t        beginOnly : 1;
		uint8_t        endOnly : 1;
		uint8_t        acquire : 1;
		uint8_t        release : 1;
		uint8_t        queueType : 5;
		/// specified whether following barrier targets particular subresource
		uint8_t        subresourceBarrier : 1;
		/// following values are ignored if subresourceBarrier is false
		uint8_t        mipLevel : 7;
		uint16_t       arrayLayer;
	} RenderTargetBarrier;

#pragma endregion (Barriers)

#pragma region (Query)

	typedef enum QueryType
	{
		SG_QUERY_TYPE_TIMESTAMP = 0,
		SG_QUERY_TYPE_PIPELINE_STATISTICS,
		SG_QUERY_TYPE_OCCLUSION,
		SG_QUERY_TYPE_COUNT,
	} QueryType;

	typedef struct QueryPoolCreateDesc
	{
		QueryType type;
		uint32_t  queryCount;
		uint32_t  nodeIndex;
	} QueryPoolCreateDesc;

	typedef struct QueryDesc
	{
		uint32_t index;
	} QueryDesc;

	///  Each query pool is a collection of a specific number of queries of a particular type.

	typedef struct QueryPool
	{
#if defined(SG_GRAPHIC_API_D3D12)
		ID3D12QueryHeap* pDxQueryHeap;
		D3D12_QUERY_TYPE  type;
		uint32_t          count;
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		VkQueryPool       pVkQueryPool;
		VkQueryType       type;
		uint32_t          count;
#endif
#if defined(SG_GRAPHIC_API_D3D11)
		ID3D11Query** ppDxQueries;
		D3D11_QUERY       type;
		uint32_t          count;
#endif
	} QueryPool;

#if defined (SG_GRAPHIC_API_VULKAN)
	// for YCbCr conversion
	typedef enum SamplerRange
	{
		SG_SAMPLER_RANGE_FULL = 0,
		SG_SAMPLER_RANGE_NARROW = 1,
	} SamplerRange;

	typedef enum SamplerModelConversion
	{
		SG_SAMPLER_MODEL_CONVERSION_RGB_IDENTITY = 0,
		SG_SAMPLER_MODEL_CONVERSION_YCBCR_IDENTITY = 1,
		SG_SAMPLER_MODEL_CONVERSION_YCBCR_709 = 2,
		SG_SAMPLER_MODEL_CONVERSION_YCBCR_601 = 3,
		SG_SAMPLER_MODEL_CONVERSION_YCBCR_2020 = 4,
	} SamplerModelConversion;

	typedef enum SampleLocation
	{
		SG_SAMPLE_LOCATION_COSITED = 0,
		SG_SAMPLE_LOCATION_MIDPOINT = 1,
	} SampleLocation;
	//////////////////////////////////////////////////////
#endif

#pragma endregion (Query)

#pragma region (Sampler)

	typedef enum FilterType
	{
		SG_FILTER_NEAREST = 0,
		SG_FILTER_LINEAR,
	} FilterType;

	typedef enum AddressMode
	{
		SG_ADDRESS_MODE_MIRROR,
		SG_ADDRESS_MODE_REPEAT,
		SG_ADDRESS_MODE_CLAMP_TO_EDGE,
		SG_ADDRESS_MODE_CLAMP_TO_BORDER
	} AddressMode;

	typedef enum MipMapMode
	{
		SG_MIPMAP_MODE_NEAREST = 0,
		SG_MIPMAP_MODE_LINEAR
	} MipMapMode;

	typedef struct SamplerCreateDesc
	{
		FilterType  minFilter;
		FilterType  magFilter;
		MipMapMode  mipMapMode;
		AddressMode addressU;
		AddressMode addressV;
		AddressMode addressW;
		float       mipLodBias;
		float       maxAnisotropy;
		CompareMode compareFunc;
#if defined(SG_GRAPHIC_API_VULKAN)
		struct
		{
			TinyImageFormat         format;
			SamplerModelConversion  model;
			SamplerRange            range;
			SampleLocation          chromaOffsetX;
			SampleLocation          chromaOffsetY;
			FilterType              chromaFilter;
			bool                    forceExplicitReconstruction;
		} mSamplerConversionDesc;
#endif
	} SamplerCreateDesc;

	typedef struct SG_ALIGN(Sampler, 16)
	{
#if defined(SG_GRAPHIC_API_D3D12)
		/// Description for creating the Sampler descriptor for this sampler
		D3D12_SAMPLER_DESC          mDxDesc;
		/// Descriptor handle of the Sampler in a CPU visible descriptor heap
		D3D12_CPU_DESCRIPTOR_HANDLE mDxHandle;
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		/// Native handle of the underlying resource
		VkSampler                       pVkSampler;
		VkSamplerYcbcrConversion        pVkSamplerYcbcrConversion;
		VkSamplerYcbcrConversionInfo    mVkSamplerYcbcrConversionInfo;
#endif
#if defined(SG_GRAPHIC_API_D3D11)
		/// Native handle of the underlying resource
		ID3D11SamplerState* pSamplerState;
#endif
#if defined(SG_GRAPHIC_API_GLES)
		GLenum						mMinFilter;
		GLenum						mMagFilter;
		GLenum						mMipMapMode;
		GLenum						mAddressS;
		GLenum						mAddressT;
		GLenum						mCompareFunc;
#endif
	} Sampler;

#if defined(SG_GRAPHIC_API_D3D12)
	SG_COMPILE_ASSERT(sizeof(Sampler) == 8 * sizeof(uint64_t));
#elif defined(SG_GRAPHIC_API_VULKAN)
	SG_COMPILE_ASSERT(sizeof(Sampler) <= 8 * sizeof(uint64_t));
#elif defined(SG_GRAPHIC_API_GLES)
	SG_COMPILE_ASSERT(sizeof(Sampler) == 4 * sizeof(uint64_t));
#else
	SG_COMPILE_ASSERT(sizeof(Sampler) == 2 * sizeof(uint64_t));
#endif

#pragma endregion (Sampler)

#pragma region (Shader)

	typedef struct ShaderMacro
	{
		const char* definition;
		const char* value;
	} ShaderMacro;

	typedef enum ShaderTarget
	{
		// We only need SM 5.0 for supporting D3D11 fallback
#if defined(SG_GRAPHIC_API_D3D11)
		SG_SHADER_TARGET_5_0,
#else
	// 5.1 is supported on all DX12 hardware
		SG_SHADER_TARGET_5_1,
		SG_SHADER_TARGET_6_0,
		SG_SHADER_TARGET_6_1,
		SG_SHADER_TARGET_6_2,
		SG_SHADER_TARGET_6_3, //required for Raytracing
#endif
	} ShaderTarget;

	typedef enum ShaderSemantic
	{
		SG_SEMANTIC_UNDEFINED = 0,
		SG_SEMANTIC_POSITION,
		SG_SEMANTIC_NORMAL,
		SG_SEMANTIC_COLOR,
		SG_SEMANTIC_TANGENT,
		SG_SEMANTIC_BITANGENT,
		SG_SEMANTIC_JOINTS,
		SG_SEMANTIC_WEIGHTS,
		SG_SEMANTIC_TEXCOORD0,
		SG_SEMANTIC_TEXCOORD1,
		SG_SEMANTIC_TEXCOORD2,
		SG_SEMANTIC_TEXCOORD3,
		SG_SEMANTIC_TEXCOORD4,
		SG_SEMANTIC_TEXCOORD5,
		SG_SEMANTIC_TEXCOORD6,
		SG_SEMANTIC_TEXCOORD7,
		SG_SEMANTIC_TEXCOORD8,
		SG_SEMANTIC_TEXCOORD9,
	} ShaderSemantic;

	typedef struct BinaryShaderStageDesc
	{
		/// Byte code array
		void* pByteCode;
		uint32_t byteCodeSize;
		const char* pEntryPoint; // for glsl is commonly "main"
#if defined(SG_GRAPHIC_API_GLES)
		GLuint		    shader;
#endif
	} BinaryShaderStageDesc;

	typedef struct BinaryShaderCreateDesc // all the shaders in the pipeline
	{
		ShaderStage           stages;
		/// Specify whether shader will own byte code memory
		uint32_t              ownByteCode : 1;
		BinaryShaderStageDesc vert;
		BinaryShaderStageDesc frag;
		BinaryShaderStageDesc geom;
		BinaryShaderStageDesc hull;
		BinaryShaderStageDesc domain;
		BinaryShaderStageDesc comp;
	} BinaryShaderCreateDesc;

	typedef struct Shader
	{
		ShaderStage    stages;
		uint32_t       numThreadsPerGroup[3];
#if defined(SG_GRAPHIC_API_D3D12)
		IDxcBlobEncoding** pShaderBlobs;
		LPCWSTR* pEntryNames;
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		VkShaderModule* pShaderModules;
		char** pEntryNames;
#endif
#if defined(SG_GRAPHIC_API_D3D11)
		union
		{
			struct
			{
				ID3D11VertexShader* pDxVertexShader;
				ID3D11PixelShader* pDxPixelShader;
				ID3D11GeometryShader* pDxGeometryShader;
				ID3D11DomainShader* pDxDomainShader;
				ID3D11HullShader* pDxHullShader;
			};
			ID3D11ComputeShader* pDxComputeShader;
		};
		ID3DBlob* pDxInputSignature;
#endif
#if defined(SG_GRAPHIC_API_GLES)
		GLuint	        program;
		GLuint	        vertexShader;
		GLuint	        fragmentShader;
#endif
		PipelineReflection* pReflection;
	} Shader;
	
#pragma endregion (Shader)

#pragma region (Descriptor)

	typedef enum DescriptorUpdateFrequency
	{
		SG_DESCRIPTOR_UPDATE_FREQ_NONE = 0,
		SG_DESCRIPTOR_UPDATE_FREQ_PER_FRAME,
		SG_DESCRIPTOR_UPDATE_FREQ_PER_BATCH,
		SG_DESCRIPTOR_UPDATE_FREQ_PER_DRAW,
		SG_DESCRIPTOR_UPDATE_FREQ_COUNT,
	} DescriptorUpdateFrequency;

	/// Data structure holding the layout for a descriptor
	typedef struct SG_ALIGN(DescriptorInfo, 16)
	{
		const char* name;
		uint32_t                  type : 21;
		uint32_t                  dim : 4;
		uint32_t                  rootDescriptor : 1;
		uint32_t                  updateFrequency : 3;
		uint32_t                  size;
		/// Index in the descriptor set
		uint32_t                  indexInParent;
		uint32_t                  handleIndex;
#if defined(SG_GRAPHIC_API_VULKAN)
		uint32_t                  vkType;
		uint32_t                  reg : 20; // the binding
		uint32_t                  rootDescriptorIndex : 3;
		uint32_t                  vkStages : 8;
#elif defined(SG_GRAPHIC_API_D3D11)
		uint32_t                  usedStages : 6;
		uint32_t                  reg : 20;
		uint32_t                  padA;
#elif defined(SG_GRAPHIC_API_D3D12)
		uint64_t                  padA;
#elif defined(SG_GRAPHIC_API_GLES)
		union
		{
			uint32_t			  mGlType;
			uint32_t			  mUBOSize;
		};
#endif
	} DescriptorInfo;
	SG_COMPILE_ASSERT(sizeof(DescriptorInfo) == 4 * sizeof(uint64_t));

	typedef struct DescriptorData
	{
		/// user can either set name of descriptor or index (index in pRootSignature->pDescriptors array)
		/// name of descriptor
		const char* name;
		union
		{
			struct
			{
				/// offset to bind the buffer descriptor
				const uint64_t* offsets;
				const uint64_t* sizes;
			};

			/// descriptor set buffer extraction options
			struct
			{
				uint32_t descriptorSetBufferIndex;
				Shader* descriptorSetShader;
				ShaderStage descriptorSetShaderStage;
			};

			struct
			{
				uint32_t UAVMipSlice;
				bool bindMipChain;
			};

			bool bindStencilResource;
		};
		/// Array of resources containing descriptor handles or constant to be used in ring buffer memory - DescriptorRange can hold only one resource type array
		union
		{
			/// Array of texture descriptors (srv and uav textures)
			Texture** ppTextures;
			/// Array of sampler descriptors
			Sampler** ppSamplers;
			/// Array of buffer descriptors (srv, uav and cbv buffers)
			Buffer** ppBuffers;
			/// Array of pipline descriptors
			Pipeline** ppPipelines;
			/// DescriptorSet buffer extraction
			DescriptorSet** ppDescriptorSet;
			/// Custom binding (raytracing acceleration structure ...)
			AccelerationStructure** ppAccelerationStructures;
		};
		/// Number of resources in the descriptor(applies to array of textures, buffers,...)
		uint32_t count;
		uint32_t index = (uint32_t)-1;
		bool     extractBuffer = false;
		} DescriptorData;

#pragma endregion (Descriptor)

#pragma region (Root Signature)

	typedef enum PipelineType
	{
		SG_PIPELINE_TYPE_UNDEFINED = 0,
		SG_PIPELINE_TYPE_COMPUTE,
		SG_PIPELINE_TYPE_GRAPHICS,
		SG_PIPELINE_TYPE_RAYTRACING,
		SG_PIPELINE_TYPE_COUNT,
	} PipelineType;

	typedef enum RootSignatureFlags
	{
		/// default flag
		SG_ROOT_SIGNATURE_FLAG_NONE = 0,
		/// local root signature used mainly in raytracing shaders
		SG_ROOT_SIGNATURE_FLAG_LOCAL_BIT = 0x1,
	} RootSignatureFlags;
	SG_MAKE_ENUM_FLAG(uint32_t, RootSignatureFlags);

	typedef struct RootSignatureCreateDesc
	{
		Shader** ppShaders;
		uint32_t shaderCount;
		uint32_t  maxBindlessTextures;
		const char** ppStaticSamplerNames;
		Sampler** ppStaticSamplers;
		uint32_t staticSamplerCount;
		RootSignatureFlags flags;
	} RootSignatureCreateDesc;

	typedef struct SG_ALIGN(RootSignature, 64)
	{
		/// Number of descriptors declared in the root signature layout
		uint32_t        descriptorCount;
		/// Graphics or Compute
		PipelineType    pipelineType;
		/// Array of all descriptors declared in the root signature layout
		DescriptorInfo* descriptors;
		/// Translates hash of descriptor name to descriptor index in pDescriptors array
		DescriptorIndexMap* descriptorNameToIndexMap;
#if defined(SG_GRAPHIC_API_D3D12)
		ID3D12RootSignature* pDxRootSignature;
		uint8_t                    DxRootConstantRootIndices[SG_MAX_ROOT_CONSTANTS_PER_ROOTSIGNATURE];
		uint8_t                    DxViewDescriptorTableRootIndices[SG_DESCRIPTOR_UPDATE_FREQ_COUNT];
		uint8_t                    DxSamplerDescriptorTableRootIndices[SG_DESCRIPTOR_UPDATE_FREQ_COUNT];
		uint8_t                    DxRootDescriptorRootIndices[SG_DESCRIPTOR_UPDATE_FREQ_COUNT];
		uint32_t                   DxCumulativeViewDescriptorCounts[SG_DESCRIPTOR_UPDATE_FREQ_COUNT];
		uint32_t                   DxCumulativeSamplerDescriptorCounts[SG_DESCRIPTOR_UPDATE_FREQ_COUNT];
		uint16_t                   DxViewDescriptorCounts[SG_DESCRIPTOR_UPDATE_FREQ_COUNT];
		uint16_t                   DxSamplerDescriptorCounts[SG_DESCRIPTOR_UPDATE_FREQ_COUNT];
		uint8_t                    DxRootDescriptorCounts[SG_DESCRIPTOR_UPDATE_FREQ_COUNT];
		uint32_t                   DxRootConstantCount;
		uint64_t                   PadA;
		uint64_t                   PadB;
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		VkDescriptorSetLayout      vkDescriptorSetLayouts[SG_DESCRIPTOR_UPDATE_FREQ_COUNT];
		uint32_t                   vkCumulativeDescriptorCounts[SG_DESCRIPTOR_UPDATE_FREQ_COUNT];
		uint16_t                   vkDescriptorCounts[SG_DESCRIPTOR_UPDATE_FREQ_COUNT];
		uint8_t                    vkDynamicDescriptorCounts[SG_DESCRIPTOR_UPDATE_FREQ_COUNT];
		uint8_t                    vkRaytracingDescriptorCounts[SG_DESCRIPTOR_UPDATE_FREQ_COUNT];
		VkPipelineLayout           pPipelineLayout;
		VkDescriptorUpdateTemplate updateTemplates[SG_DESCRIPTOR_UPDATE_FREQ_COUNT];
		VkDescriptorSet            vkEmptyDescriptorSets[SG_DESCRIPTOR_UPDATE_FREQ_COUNT];
		void** pUpdateTemplateData[SG_DESCRIPTOR_UPDATE_FREQ_COUNT];
		uint32_t                   vkPushConstantCount;
		uint32_t                   padA;
		uint64_t                   padB[7];
#endif
#if defined(SG_GRAPHIC_API_D3D11)
		ID3D11SamplerState** ppStaticSamplers;
		uint32_t* pStaticSamplerSlots;
		ShaderStage* pStaticSamplerStages;
		uint32_t                   staticSamplerCount;
		uint32_t                   srvCount : 10;
		uint32_t                   uavCount : 10;
		uint32_t                   cbvCount : 10;
		uint32_t                   samplerCount : 10;
		uint32_t                   dynamicCbvCount : 10;
		uint32_t                   PadA;
#endif
#if defined(SG_GRAPHIC_API_GLES)
		uint32_t				   programCount : 6;
		uint32_t				   variableCount : 10;
		uint32_t* pProgramTargets;
		int32_t* pDescriptorGlLocations;
		struct GlVariable* pVariables;
		Sampler* pSampler;
#endif
	} RootSignature;

#if defined(SG_GRAPHIC_API_VULKAN)
	// 4 cache lines
	SG_COMPILE_ASSERT(sizeof(RootSignature) == 32 * sizeof(uint64_t));
#elif defined(SG_GRAPHIC_API_D3D11) || defined(SG_GRAPHIC_API_GLES)
	// 1 cache line
	SG_COMPILE_ASSERT(sizeof(RootSignature) == 8 * sizeof(uint64_t));
#else
	// 2 cache lines
	SG_COMPILE_ASSERT(sizeof(RootSignature) <= 16 * sizeof(uint64_t));
#endif

	typedef struct DescriptorSetCreateDesc
	{
		RootSignature* pRootSignature;
		DescriptorUpdateFrequency updateFrequency;
		uint32_t maxSets;
		uint32_t nodeIndex;
	} DescriptorSetCreateDesc;

#pragma endregion (Root Signature)

#pragma region (Descriptor Set)

	typedef struct SG_ALIGN(DescriptorSet, 64)
	{
#if defined(SG_GRAPHIC_API_D3D12)
		/// Start handle to cbv srv uav descriptor table
		uint64_t                      cbvSrvUavHandle;
		/// Start handle to sampler descriptor table
		uint64_t                      samplerHandle;
		/// Stride of the cbv srv uav descriptor table (number of descriptors * descriptor size)
		uint32_t                      cbvSrvUavStride;
		/// Stride of the sampler descriptor table (number of descriptors * descriptor size)
		uint32_t                      samplerStride;
		const RootSignature* pRootSignature;
		D3D12_GPU_VIRTUAL_ADDRESS* pRootAddresses;
		ID3D12RootSignature* pRootSignatureHandle;
		uint64_t                      maxSets : 16;
		uint64_t                      updateFrequency : 3;
		uint64_t                      nodeIndex : 4;
		uint64_t                      rootAddressCount : 1;
		uint64_t                      cbvSrvUavRootIndex : 4;
		uint64_t                      samplerRootIndex : 4;
		uint64_t                      rootDescriptorRootIndex : 4;
		uint64_t                      pipelineType : 3;
#elif defined(SG_GRAPHIC_API_VULKAN)
		VkDescriptorSet* pHandles;
		const RootSignature* pRootSignature;
		/// Values passed to vkUpdateDescriptorSetWithTemplate. Initialized to default descriptor values.
		union DescriptorUpdateData** ppUpdateData;
		struct SizeOffset* pDynamicSizeOffsets;
		uint32_t maxSets;
		uint8_t dynamicOffsetCount;
		uint8_t updateFrequency;
		uint8_t nodeIndex;
		uint8_t padA;
#elif defined(SG_GRAPHIC_API_D3D11)
		struct DescriptorDataArray* pHandles;
		struct CBV** pDynamicCBVs;
		uint32_t* pDynamicCBVsCapacity;
		uint32_t* pDynamicCBVsCount;
		uint32_t* pDynamicCBVsPrevCount;
		const RootSignature* pRootSignature;
		uint16_t                      mMaxSets;
#elif defined(SG_GRAPHIC_API_GLES)
		struct DescriptorDataArray* pHandles;
		uint8_t                       mUpdateFrequency;
		const RootSignature* pRootSignature;
		uint16_t                      mMaxSets;
#endif
	} DescriptorSet;

#pragma endregion (Descriptor Set)

#pragma region (Pipeline Cache)

	typedef enum PipelineCacheFlags
	{
		SG_PIPELINE_CACHE_FLAG_NONE = 0x0,
		SG_PIPELINE_CACHE_FLAG_EXTERNALLY_SYNCHRONIZED = 0x1,
	} PipelineCacheFlags;
	SG_MAKE_ENUM_FLAG(uint32_t, PipelineCacheFlags);

	typedef struct PipelineCacheDesc
	{
		/// Initial pipeline cache data (can be NULL which means empty pipeline cache)
		void* pData;
		/// Initial pipeline cache size
		size_t             size;
		PipelineCacheFlags flags;
	} PipelineCacheDesc;

	typedef struct PipelineCache
	{
#if defined(SG_GRAPHIC_API_D3D12)
		ID3D12PipelineLibrary* pLibrary;
		void* pData;
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		VkPipelineCache        cache;
#endif
	} PipelineCache;

#pragma endregion (Pipeline Cache)

#pragma region (Pipeline)

	typedef enum VertexAttribRate
	{
		SG_VERTEX_ATTRIB_RATE_VERTEX = 0,
		SG_VERTEX_ATTRIB_RATE_INSTANCE = 1,
		SG_VERTEX_ATTRIB_RATE_COUNT,
	} VertexAttribRate;

	typedef struct VertexAttrib
	{
		ShaderSemantic    semantic; // for what purpose
		uint32_t          semanticNameLength;
		char              semanticName[SG_MAX_SEMANTIC_NAME_LENGTH];
		TinyImageFormat	  format;
		uint32_t          binding;
		uint32_t          location;
		uint32_t          offset;
		VertexAttribRate  rate;
	} VertexAttrib;

	typedef struct VertexLayout
	{
		uint32_t     attribCount;
		VertexAttrib attribs[SG_MAX_VERTEX_ATTRIBS];
	} VertexLayout;

	typedef enum IndexType
	{
		SG_INDEX_TYPE_UINT32 = 0,
		SG_INDEX_TYPE_UINT16,
	} IndexType;

	typedef enum PrimitiveTopology
	{
		SG_PRIMITIVE_TOPO_POINT_LIST = 0,
		SG_PRIMITIVE_TOPO_LINE_LIST,
		SG_PRIMITIVE_TOPO_LINE_STRIP,
		SG_PRIMITIVE_TOPO_TRI_LIST,
		SG_PRIMITIVE_TOPO_TRI_STRIP,
		SG_PRIMITIVE_TOPO_PATCH_LIST,
		SG_PRIMITIVE_TOPO_COUNT,
	} PrimitiveTopology;

/************************************************************************/
// #pGlobalRootSignature - Root Signature used by all shaders in the ppShaders array
// #ppShaders - Array of all shaders which can be called during the raytracing operation
//	  This includes the ray generation shader, all miss, any hit, closest hit shaders
// #pHitGroups - Name of the hit groups which will tell the pipeline about which combination of hit shaders to use
// #mPayloadSize - Size of the payload(��Ч�غ�) struct for passing data to and from the shaders.
//	  Example - float4 payload sent by raygen shader which will be filled by miss shader as a skybox color
//				  or by hit shader as shaded color
// #mAttributeSize - Size of the intersection attribute. As long as user uses the default intersection shader
//	  this size is sizeof(float2) which represents the ZW of the barycentric coordinates of the intersection
/************************************************************************/
	typedef struct RaytracingPipelineDesc
	{
		Raytracing* pRaytracing;
		RootSignature* pGlobalRootSignature;
		Shader* pRayGenShader;
		RootSignature* pRayGenRootSignature;
		Shader** ppMissShaders;
		RootSignature** ppMissRootSignatures;
		RaytracingHitGroup* pHitGroups;
		RootSignature* pEmptyRootSignature;
		unsigned			missShaderCount;
		unsigned			hitGroupCount;
		// #TODO : Remove this after adding shader reflection for raytracing shaders
		unsigned			payloadSize;
		// #TODO : Remove this after adding shader reflection for raytracing shaders
		unsigned			attributeSize;
		unsigned			maxTraceRecursionDepth;
		unsigned            maxRaysCount;
	} RaytracingPipelineDesc;

	typedef struct GraphicsPipelineDesc
	{
		Shader* pShaderProgram;
		RootSignature* pRootSignature;
		VertexLayout* pVertexLayout;
		BlendStateDesc* pBlendState;
		DepthStateDesc* pDepthState;
		RasterizerStateDesc* pRasterizerState;
		TinyImageFormat* pColorFormats;
		uint32_t               renderTargetCount;
		SampleCount            sampleCount;
		uint32_t               sampleQuality;
		TinyImageFormat  	   depthStencilFormat;
		PrimitiveTopology      primitiveTopo;
		bool                   supportIndirectCommandBuffer;
	} GraphicsPipelineDesc;

	typedef struct ComputePipelineDesc
	{
		Shader* pShaderProgram;
		RootSignature* pRootSignature;
	} ComputePipelineDesc;

	typedef struct PipelineCreateDesc
	{
		PipelineType                type;
		union
		{
			ComputePipelineDesc     computeDesc;
			GraphicsPipelineDesc    graphicsDesc;
			RaytracingPipelineDesc  raytracingDesc;
		};
		PipelineCache* pCache;
		void* pPipelineExtensions;
		uint32_t                    extensionCount;
		const char* name;
	} PipelineCreateDesc;

	typedef struct SG_ALIGN(Pipeline, 64)
	{
#if defined(SG_GRAPHIC_API_D3D12)
		ID3D12PipelineState* pDxPipelineState;
#ifdef ENABLE_RAYTRACING
		ID3D12StateObject* pDxrPipeline;
#endif
		ID3D12RootSignature* pRootSignature;
		PipelineType                mType;
		D3D_PRIMITIVE_TOPOLOGY      mDxPrimitiveTopology;
		uint64_t                    mPadB[3];
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		VkPipeline                  pVkPipeline;
		PipelineType                type;
		uint32_t                    shaderStageCount;
		// in DX12 this information is stored in ID3D12StateObject.
		// but for Vulkan we need to store it manually
		const char** ppShaderStageNames;
		uint64_t                    padB[4];
#endif
#if defined(SG_GRAPHIC_API_D3D11)
		ID3D11VertexShader* pDxVertexShader;
		ID3D11PixelShader* pDxPixelShader;
		ID3D11GeometryShader* pDxGeometryShader;
		ID3D11DomainShader* pDxDomainShader;
		ID3D11HullShader* pDxHullShader;
		ID3D11ComputeShader* pDxComputeShader;
		ID3D11InputLayout* pDxInputLayout;
		ID3D11BlendState* pBlendState;
		ID3D11DepthStencilState* pDepthState;
		ID3D11RasterizerState* pRasterizerState;
		PipelineType                mType;
		D3D_PRIMITIVE_TOPOLOGY      mDxPrimitiveTopology;
		uint32_t                    mPadA;
		uint64_t                    mPadB[4];
#endif
#if defined(SG_GRAPHIC_API_GLES)
		uint32_t				    mVertexLayoutSize;
		struct GlVertexAttrib* pVertexLayout;
		struct GLRasterizerState* pRasterizerState;
		struct GLDepthStencilState* pDepthStencilState;
		struct GLBlendState* pBlendState;
		GLuint						mShaderProgram;
		PipelineType				mType;
		GLenum					    mGlPrimitiveTopology;
#endif
	} Pipeline;

#if defined(SG_GRAPHIC_API_D3D11)
	// Requires more cache lines due to no concept of an encapsulated pipeline state object
	SG_COMPILE_ASSERT(sizeof(Pipeline) <= 64 * sizeof(uint64_t));
#else
	// One cache line
	SG_COMPILE_ASSERT(sizeof(Pipeline) == 8 * sizeof(uint64_t));
#endif

#pragma endregion (Pipeline)

#pragma region (Renderer)

	typedef struct RendererCreateDesc
	{
		LogFunc      pLogFn;
		RendererAPI  api;
		ShaderTarget shaderTarget;
		GPUMode      gpuMode;
#if defined(SG_GRAPHIC_API_VULKAN)
		const char** ppInstanceLayers;
		const char** ppInstanceExtensions;
		const char** ppDeviceExtensions;
		uint32_t     instanceLayerCount;
		uint32_t     instanceExtensionCount;
		uint32_t     deviceExtensionCount;
		/// Flag to specify whether to request all queues from the gpu or just one of each type
		/// This will affect memory usage - Around 200 MB more used if all queues are requested
		bool         requestAllAvailableQueues;
#endif
#if defined(SG_GRAPHIC_API_D3D12)
		D3D_FEATURE_LEVEL            mDxFeatureLevel;
#endif
#if defined(SG_GRAPHIC_API_D3D11)
		/// Set whether to force feature level 10 for compatibility
		bool                         mUseDx10;
		/// Set whether to pick the first valid GPU or use our GpuConfig
		bool                         mUseDefaultGpu;
#endif 
#if defined(SG_GRAPHIC_API_GLES)
		const char** ppDeviceExtensions;
		uint32_t                     mDeviceExtensionCount;
#endif
		/// This results in new validation not possible during API calls on the CPU, by creating patched shaders that have validation added directly to the shader.
		/// However, it can slow things down a lot, especially for applications with numerous PSOs. Time to see the first render frame may take several minutes
		bool                         enableGPUBasedValidation;
	} RendererCreateDesc;

	typedef struct SG_ALIGN(Renderer, 64)
	{
#if defined(SG_GRAPHIC_API_D3D12)
		// API specific descriptor heap and memory allocator
		struct DescriptorHeap** pCPUDescriptorHeaps;
		struct DescriptorHeap** pCbvSrvUavHeaps;
		struct DescriptorHeap** pSamplerHeaps;
		class  D3D12MA::Allocator* pResourceAllocator;
#if defined(XBOX)
		IDXGIFactory2* pDXGIFactory;
		IDXGIAdapter* pDxActiveGPU;
		ID3D12Device* pDxDevice;
		EsramManager* pESRAMManager;
#elif defined(SG_GRAPHIC_API_D3D12)
		IDXGIFactory6* pDXGIFactory;
		IDXGIAdapter4* pDxActiveGPU;
		ID3D12Device* pDxDevice;
#if defined(SG_PLATFORM_WINDOWS) && defined(DRED)
		ID3D12DeviceRemovedExtendedDataSettings* pDredSettings;
#else
		uint64_t                        mPadA;
#endif
#endif
		ID3D12Debug* pDXDebug;
#if defined(SG_PLATFORM_WINDOWS)
		ID3D12InfoQueue* pDxDebugValidation;
#endif

#endif
#if defined(SG_GRAPHIC_API_D3D11)
		IDXGIFactory1* pDXGIFactory;
		IDXGIAdapter1* pDxActiveGPU;
		ID3D11Device* pDxDevice;
		ID3D11DeviceContext* pDxContext;
		ID3D11BlendState* pDefaultBlendState;
		ID3D11DepthStencilState* pDefaultDepthState;
		ID3D11RasterizerState* pDefaultRasterizerState;
		uint32_t                        mPartialUpdateConstantBufferSupported : 1;
		D3D_FEATURE_LEVEL               mFeatureLevel;
#if defined(ENABLE_PERFORMANCE_MARKER)
		ID3DUserDefinedAnnotation* pUserDefinedAnnotation;
#else
		uint64_t                        mPadB;
#endif
		uint32_t                        mPadA;
#endif
#if defined(SG_GRAPHIC_API_VULKAN)
		VkInstance                      pVkInstance;
		VkPhysicalDevice                pVkActiveGPU;
		VkPhysicalDeviceProperties2*    pVkActiveGPUProperties;
		VkDevice                        pVkDevice;
#ifdef SG_VULKAN_USE_DEBUG_UTILS_EXTENSION
		VkDebugUtilsMessengerEXT        pVkDebugUtilsMessenger;
#else
		VkDebugReportCallbackEXT        pVkDebugReport;
#endif
		uint32_t**						pAvailableQueueCount;
		uint32_t**						pUsedQueueCount;
		struct DescriptorPool*			pDescriptorPool;
		//struct VmaAllocator_T*	    pVmaAllocator;
		VmaAllocator					vmaAllocator;
		uint32_t                        raytracingExtension : 1;
		union
		{
			struct
			{
				uint8_t                 graphicsQueueFamilyIndex;
				uint8_t                 transferQueueFamilyIndex;
				uint8_t                 computeQueueFamilyIndex;
			};
			uint8_t                     queueFamilyIndices[3];
		};
#endif
#if defined(USE_NSIGHT_AFTERMATH)
		// GPU crash dump tracker using Nsight Aftermath instrumentation
		AftermathTracker                aftermathTracker;
		bool                            aftermathSupport;
		bool                            diagnosticsConfigSupport;
		bool                            diagnosticCheckPointsSupport;
#endif
#if defined(SG_GRAPHIC_API_GLES)
		GLContext						pContext;
		GLConfig						pConfig;
#endif
		struct NullDescriptors*         pNullDescriptors;
		char*						    name;
		GPUSettings*					pActiveGpuSettings;
		ShaderMacro*					pBuiltinShaderDefines;
		GPUCapBits*						pCapBits;
		uint32_t                        linkedNodeCount : 4;
		uint32_t                        gpuMode : 3;
		uint32_t                        shaderTarget : 4;
		uint32_t                        api : 5;
		uint32_t                        enableGpuBasedValidation : 1;
		uint32_t                        builtinShaderDefinesCount;
	} Renderer;
	// 3 cache lines
	SG_COMPILE_ASSERT(sizeof(Renderer) <= 24 * sizeof(uint64_t));

#pragma endregion (Renderer)

#define SG_RENDER_API

	SG_RENDER_API void SG_CALLCONV init_renderer(const char* appName, const RendererCreateDesc* pDesc, Renderer** ppRenderer);
	SG_RENDER_API void SG_CALLCONV remove_renderer(Renderer* pRenderer);

	SG_RENDER_API void SG_CALLCONV add_sampler(Renderer* pRenderer, const SamplerCreateDesc* pDesc, Sampler** pSampler);
	SG_RENDER_API void SG_CALLCONV remove_sampler(Renderer* pRenderer, Sampler* pSampler);

	SG_RENDER_API void SG_CALLCONV add_fence(Renderer* pRenderer, Fence** pFence);
	SG_RENDER_API void SG_CALLCONV remove_fence(Renderer* pRenderer, Fence* pFence);

	SG_RENDER_API void SG_CALLCONV add_semaphore(Renderer* pRenderer, Semaphore** pSemaphore);
	SG_RENDER_API void SG_CALLCONV remove_semaphore(Renderer* pRenderer, Semaphore* pSemaphore);

	SG_RENDER_API void SG_CALLCONV add_queue(Renderer* pRenderer, QueueCreateDesc* pQueueDesc, Queue** pQueue);
	SG_RENDER_API void SG_CALLCONV remove_queue(Renderer* pRenderer, Queue* pQueue);

	SG_RENDER_API void SG_CALLCONV add_swapchain(Renderer* pRenderer, const SwapChainCreateDesc* pDesc, SwapChain** pSwapChain);
	SG_RENDER_API void SG_CALLCONV remove_swapchain(Renderer* pRenderer, SwapChain* pSwapChain);

	SG_RENDER_API void SG_CALLCONV add_render_target(Renderer* pRenderer, const RenderTargetCreateDesc* pDesc, RenderTarget** ppRenderTarget);
	SG_RENDER_API void SG_CALLCONV remove_render_target(Renderer* pRenderer, RenderTarget* pRenderTarget);

	// command pool functions
	SG_RENDER_API void SG_CALLCONV add_command_pool(Renderer* pRenderer, const CmdPoolCreateDesc* pDesc, CmdPool** pCmdPool);
	SG_RENDER_API void SG_CALLCONV remove_command_pool(Renderer* pRenderer, CmdPool* pCmdPool);
	SG_RENDER_API void SG_CALLCONV add_cmd(Renderer* pRenderer, const CmdCreateDesc* pDesc, Cmd** pCmd);
	SG_RENDER_API void SG_CALLCONV remove_cmd(Renderer* pRenderer, Cmd* pCmd);
	SG_RENDER_API void SG_CALLCONV add_cmd_n(Renderer* pRenderer, const CmdCreateDesc* pDesc, uint32_t cmdCount, Cmd*** pCmds);
	SG_RENDER_API void SG_CALLCONV remove_cmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** pCmds);

	// command buffer functions
	SG_RENDER_API void SG_CALLCONV reset_command_pool(Renderer* pRenderer, CmdPool* pCmdPool);
	SG_RENDER_API void SG_CALLCONV begin_cmd(Cmd* pCmd);
	SG_RENDER_API void SG_CALLCONV end_cmd(Cmd* pCmd);
	SG_RENDER_API void SG_CALLCONV cmd_bind_render_targets(Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** pRenderTargets, RenderTarget* pDepthStencil, const LoadActionsDesc* loadActions, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice, uint32_t depthMipSlice);
	SG_RENDER_API void SG_CALLCONV cmd_set_viewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth);
	SG_RENDER_API void SG_CALLCONV cmd_set_scissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
	SG_RENDER_API void SG_CALLCONV cmd_set_stencil_reference_value(Cmd* pCmd, uint32_t value);
	SG_RENDER_API void SG_CALLCONV cmd_bind_pipeline(Cmd* pCmd, Pipeline* pPipeline);
	SG_RENDER_API void SG_CALLCONV cmd_bind_descriptor_set(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet);
	SG_RENDER_API void SG_CALLCONV cmd_bind_push_constants(Cmd* pCmd, RootSignature* pRootSignature, const char* pName, const void* pConstants);
	SG_RENDER_API void SG_CALLCONV cmd_bind_push_constants_by_index(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants);
	SG_RENDER_API void SG_CALLCONV cmd_bind_vertex_buffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets);
	SG_RENDER_API void SG_CALLCONV cmd_bind_index_buffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset);
	SG_RENDER_API void SG_CALLCONV cmd_draw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex);
	SG_RENDER_API void SG_CALLCONV cmd_draw_instanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance);
	SG_RENDER_API void SG_CALLCONV cmd_draw_indexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex);
	SG_RENDER_API void SG_CALLCONV cmd_draw_indexed_instanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
	SG_RENDER_API void SG_CALLCONV cmd_dispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

	// barrier transition command
	SG_RENDER_API void SG_CALLCONV cmd_resource_barrier(Cmd* pCmd, uint32_t bufferBarrierCount, BufferBarrier* pBufferBarriers, uint32_t textureBarrierCount, TextureBarrier* pTextureBarriers, uint32_t rtBarrierCount, RenderTargetBarrier* pRtBarriers);

	SG_RENDER_API void SG_CALLCONV add_shader_binary(Renderer* pRenderer, const BinaryShaderCreateDesc* pDesc, Shader** pShader);
	SG_RENDER_API void SG_CALLCONV remove_shader(Renderer* pRenderer, Shader* pShader);

	SG_RENDER_API void SG_CALLCONV add_root_signature(Renderer* pRenderer, const RootSignatureCreateDesc* pDesc, RootSignature** pRootSignature);
	SG_RENDER_API void SG_CALLCONV remove_root_signature(Renderer* pRenderer, RootSignature* pRootSignature);

	// pipeline functions
	SG_RENDER_API void SG_CALLCONV add_pipeline(Renderer* pRenderer, const PipelineCreateDesc* pPipelineSettings, Pipeline** pPipeline);
	SG_RENDER_API void SG_CALLCONV remove_pipeline(Renderer* pRenderer, Pipeline* pPipeline);
	SG_RENDER_API void SG_CALLCONV add_pipeline_cache(Renderer* pRenderer, const PipelineCacheDesc* pDesc, PipelineCache** ppPipelineCache);
	SG_RENDER_API void SG_CALLCONV get_pipeline_cache_data(Renderer* pRenderer, PipelineCache* pPipelineCache, size_t* pSize, void* pData);
	SG_RENDER_API void SG_CALLCONV remove_pipeline_cache(Renderer* pRenderer, PipelineCache* pPipelineCache);

	// descriptor Set functions
	SG_RENDER_API void SG_CALLCONV add_descriptor_set(Renderer* pRenderer, const DescriptorSetCreateDesc* pDesc, DescriptorSet** pDescriptorSet);
	SG_RENDER_API void SG_CALLCONV remove_descriptor_set(Renderer* pRenderer, DescriptorSet* pDescriptorSet);
	SG_RENDER_API void SG_CALLCONV update_descriptor_set(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pData);

	// virtual Textures
	//SG_RENDER_API void SG_CALLCONV cmd_update_virtual_texture(Cmd* pCmd, Texture* pTexture);

	// all buffer, texture update handled by resource system -> IResourceLoader.h

	// queue/fence/swapchain functions
	SG_RENDER_API void SG_CALLCONV acquire_next_image(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pImageIndex);
	SG_RENDER_API void SG_CALLCONV queue_submit(Queue* pQueue, const QueueSubmitDesc* pDesc);
	SG_RENDER_API PresentStatus SG_CALLCONV queue_present(Queue* pQueue, const QueuePresentDesc* pDesc);
	SG_RENDER_API void SG_CALLCONV wait_queue_idle(Queue* pQueue);
	SG_RENDER_API void SG_CALLCONV get_fence_status(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus);
	SG_RENDER_API void SG_CALLCONV wait_for_fences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences);
	SG_RENDER_API void SG_CALLCONV toggle_VSync(Renderer* pRenderer, SwapChain** ppSwapchain);

	// returns the recommended format for the swapchain.
	// if true is passed for the hintHDR parameter, it will return an HDR format IF the platform supports it
	// if false is passed or the platform does not support HDR a non HDR format is returned.
	SG_RENDER_API TinyImageFormat SG_CALLCONV get_recommended_swapchain_format(bool hintHDR);

	//indirect Draw functions
	//SG_RENDER_API void SG_CALLCONV add_indirect_command_signature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature);
	//SG_RENDER_API void SG_CALLCONV remove_indirect_command_signature(Renderer* pRenderer, CommandSignature* pCommandSignature);
	//SG_RENDER_API void SG_CALLCONV cmd_execute_indirect(Cmd* pCmd, CommandSignature* pCommandSignature, uint32_t maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset, Buffer* pCounterBuffer, uint64_t counterBufferOffset);

	// GPU Query Interface
	SG_RENDER_API void SG_CALLCONV get_timestamp_frequency(Queue* pQueue, double* pFrequency);
	SG_RENDER_API void SG_CALLCONV add_query_pool(Renderer* pRenderer, const QueryPoolCreateDesc* pDesc, QueryPool** ppQueryPool);
	SG_RENDER_API void SG_CALLCONV remove_query_pool(Renderer* pRenderer, QueryPool* pQueryPool);
	SG_RENDER_API void SG_CALLCONV cmd_reset_query_pool(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount);
	SG_RENDER_API void SG_CALLCONV cmd_begin_query(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery);
	SG_RENDER_API void SG_CALLCONV cmd_end_query(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery);
	SG_RENDER_API void SG_CALLCONV cmd_resolve_query(Cmd* pCmd, QueryPool* pQueryPool, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount);

	// stats Info Interface
	SG_RENDER_API void SG_CALLCONV calculate_memory_stats(Renderer* pRenderer, char** stats);
	SG_RENDER_API void SG_CALLCONV calculate_memory_use(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes);
	SG_RENDER_API void SG_CALLCONV free_memory_stats(Renderer* pRenderer, char* stats);

	// debug Marker Interface
	SG_RENDER_API void SG_CALLCONV cmd_begin_debug_marker(Cmd* pCmd, float r, float g, float b, const char* pName);
	SG_RENDER_API void SG_CALLCONV cmd_end_debug_marker(Cmd* pCmd);
	SG_RENDER_API void SG_CALLCONV cmd_add_debug_marker(Cmd* pCmd, float r, float g, float b, const char* pName);
#if defined(SG_GRAPHIC_API_D3D12)
	SG_RENDER_API uint32_t SG_CALLCONV cmd_write_marker(Cmd* pCmd, MarkerType markerType, uint32_t markerValue, Buffer* pBuffer, size_t offset, bool useAutoFlags = false);
#endif

	// resource debug naming interface
	SG_RENDER_API void SG_CALLCONV set_buffer_name(Renderer* pRenderer, Buffer* pBuffer, const char* pName);
	SG_RENDER_API void SG_CALLCONV set_texture_name(Renderer* pRenderer, Texture* pTexture, const char* pName);
	SG_RENDER_API void SG_CALLCONV set_render_target_name(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName);
	SG_RENDER_API void SG_CALLCONV set_pipeline_name(Renderer* pRenderer, Pipeline* pPipeline, const char* pName);

}