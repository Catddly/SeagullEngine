#pragma once

#include "../../../Core/Third-party/Include/tinyImageFormat/include/tinyimageformat_base.h"

#ifdef __cplusplus
#ifndef SG_MAKE_ENUM_FLAG
#define SG_MAKE_ENUM_FLAG(TYPE, ENUM_TYPE)                                                                        \
			static inline ENUM_TYPE operator|(ENUM_TYPE a, ENUM_TYPE b)   { return (ENUM_TYPE)((TYPE)(a) | (TYPE)(b)); } \
			static inline ENUM_TYPE operator&(ENUM_TYPE a, ENUM_TYPE b)   { return (ENUM_TYPE)((TYPE)(a) & (TYPE)(b)); } \
			static inline ENUM_TYPE operator|=(ENUM_TYPE& a, ENUM_TYPE b) { return a = (a | b); }                      \
			static inline ENUM_TYPE operator&=(ENUM_TYPE& a, ENUM_TYPE b) { return a = (a & b); }
#endif
#else
#define SG_MAKE_ENUM_FLAG(TYPE, ENUM_TYPE)
#endif

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

namespace SG
{

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
		sg_shader_target_5_0,
#else
	// 5.1 is supported on all DX12 hardware
		sg_shader_target_5_1,
		sg_shader_target_6_0,
		sg_shader_target_6_1,
		sg_shader_target_6_2,
		sg_shader_target_6_3, //required for Raytracing
#endif
	} ShaderTarget;

#pragma endregion (Shader)

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

//	typedef enum ResourceState // for barrier purpose
//	{
//		SG_RESOURCE_STATE_UNDEFINED = 0,
//		SG_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER = 0x1,
//		SG_RESOURCE_STATE_INDEX_BUFFER = 0x2,
//		SG_RESOURCE_STATE_RENDER_TARGET = 0x4,
//		SG_RESOURCE_STATE_UNORDERED_ACCESS = 0x8,
//		SG_RESOURCE_STATE_DEPTH_WRITE = 0x10,
//		SG_RESOURCE_STATE_DEPTH_READ = 0x20,
//		SG_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE = 0x40,
//		SG_RESOURCE_STATE_PIXEL_SHADER_RESOURCE = 0x80,
//		SG_RESOURCE_STATE_SHADER_RESOURCE = 0x40 | 0x80,
//		SG_RESOURCE_STATE_STREAM_OUT = 0x100,
//		SG_RESOURCE_STATE_INDIRECT_ARGUMENT = 0x200,
//		SG_RESOURCE_STATE_COPY_DEST = 0x400,
//		SG_RESOURCE_STATE_COPY_SOURCE = 0x800,
//		SG_RESOURCE_STATE_GENERIC_READ = (((((0x1 | 0x2) | 0x40) | 0x80) | 0x200) | 0x800),
//		SG_RESOURCE_STATE_PRESENT = 0x1000,
//		SG_RESOURCE_STATE_COMMON = 0x2000,
//		SG_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE = 0x4000,
//	} ResourceState;
//	SG_MAKE_ENUM_FLAG(uint32_t, ResourceState);
//
//	typedef enum DescriptorType
//	{
//		SG_DESCRIPTOR_TYPE_UNDEFINED = 0,
//		SG_DESCRIPTOR_TYPE_SAMPLER = 0x01,
//		// SRV Read only texture
//		SG_DESCRIPTOR_TYPE_TEXTURE = (SG_DESCRIPTOR_TYPE_SAMPLER << 1),
//		/// UAV Texture
//		SG_DESCRIPTOR_TYPE_RW_TEXTURE = (SG_DESCRIPTOR_TYPE_TEXTURE << 1),
//		// SRV Read only buffer
//		SG_DESCRIPTOR_TYPE_BUFFER = (SG_DESCRIPTOR_TYPE_RW_TEXTURE << 1),
//		SG_DESCRIPTOR_TYPE_BUFFER_RAW = (SG_DESCRIPTOR_TYPE_BUFFER | (SG_DESCRIPTOR_TYPE_BUFFER << 1)),
//		/// UAV Buffer
//		SG_DESCRIPTOR_TYPE_RW_BUFFER = (SG_DESCRIPTOR_TYPE_BUFFER << 2),
//		SG_DESCRIPTOR_TYPE_RW_BUFFER_RAW = (SG_DESCRIPTOR_TYPE_RW_BUFFER | (SG_DESCRIPTOR_TYPE_RW_BUFFER << 1)),
//		/// Uniform buffer
//		SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER = (SG_DESCRIPTOR_TYPE_RW_BUFFER << 2),
//		/// Push constant / Root constant
//		SG_DESCRIPTOR_TYPE_ROOT_CONSTANT = (SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER << 1),
//		/// IA
//		SG_DESCRIPTOR_TYPE_VERTEX_BUFFER = (SG_DESCRIPTOR_TYPE_ROOT_CONSTANT << 1),
//		SG_DESCRIPTOR_TYPE_INDEX_BUFFER = (SG_DESCRIPTOR_TYPE_VERTEX_BUFFER << 1),
//		SG_DESCRIPTOR_TYPE_INDIRECT_BUFFER = (SG_DESCRIPTOR_TYPE_INDEX_BUFFER << 1),
//		/// Cubemap SRV
//		SG_DESCRIPTOR_TYPE_TEXTURE_CUBE = (SG_DESCRIPTOR_TYPE_TEXTURE | (SG_DESCRIPTOR_TYPE_INDIRECT_BUFFER << 1)),
//		/// RTV / DSV per mip slice
//		SG_DESCRIPTOR_TYPE_RENDER_TARGET_MIP_SLICES = (SG_DESCRIPTOR_TYPE_INDIRECT_BUFFER << 2),
//		/// RTV / DSV per array slice
//		SG_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES = (SG_DESCRIPTOR_TYPE_RENDER_TARGET_MIP_SLICES << 1),
//		/// RTV / DSV per depth slice
//		SG_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES = (SG_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES << 1),
//		SG_DESCRIPTOR_TYPE_RAY_TRACING = (SG_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES << 1),
//#if defined(SG_GRAPHIC_API_VULKAN)
//		/// subpass input (descriptor type only available in Vulkan)
//		SG_DESCRIPTOR_TYPE_INPUT_ATTACHMENT = (SG_DESCRIPTOR_TYPE_RAY_TRACING << 1),
//		SG_DESCRIPTOR_TYPE_TEXEL_BUFFER = (SG_DESCRIPTOR_TYPE_INPUT_ATTACHMENT << 1),
//		SG_DESCRIPTOR_TYPE_RW_TEXEL_BUFFER = (SG_DESCRIPTOR_TYPE_TEXEL_BUFFER << 1),
//		SG_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = (SG_DESCRIPTOR_TYPE_RW_TEXEL_BUFFER << 1),
//#endif
//	} DescriptorType;
//	SG_MAKE_ENUM_FLAG(uint32_t, DescriptorType);
//
	typedef enum QueueType
	{
		SG_QUEUE_TYPE_GRAPHICS = 0,
		SG_QUEUE_TYPE_TRANSFER,
		SG_QUEUE_TYPE_COMPUTE,
		SG_MAX_QUEUE_TYPE
	} QueueType;
//
//	/// Choosing Memory Type
//	typedef enum ResourceMemoryUsage
//	{
//		/// No intended memory usage specified.
//		SG_RESOURCE_MEMORY_USAGE_UNKNOWN = 0,
//		/// Memory will be used on device only, no need to be mapped on host.
//		SG_RESOURCE_MEMORY_USAGE_GPU_ONLY = 1,
//		/// Memory will be mapped on host. Could be used for transfer to device.
//		SG_RESOURCE_MEMORY_USAGE_CPU_ONLY = 2,
//		/// Memory will be used for frequent (dynamic) updates from host and reads on device.
//		SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU = 3,
//		/// Memory will be used for writing on device and readback on host.
//		SG_RESOURCE_MEMORY_USAGE_GPU_TO_CPU = 4,
//		SG_RESOURCE_MEMORY_USAGE_COUNT,
//		SG_RESOURCE_MEMORY_USAGE_MAX_ENUM = 0x7FFFFFFF
//	} ResourceMemoryUsage;
//
//	typedef enum IndirectArgumentType
//	{
//		SG_INDIRECT_DRAW,
//		SG_INDIRECT_DRAW_INDEX,
//		SG_INDIRECT_DISPATCH,
//		SG_INDIRECT_VERTEX_BUFFER,
//		SG_INDIRECT_INDEX_BUFFER,
//		SG_INDIRECT_CONSTANT,
//		SG_INDIRECT_DESCRIPTOR_TABLE,        // only for vulkan
//		SG_INDIRECT_PIPELINE,                // only for vulkan now, probally will add to dx when it comes to xbox
//		SG_INDIRECT_CONSTANT_BUFFER_VIEW,    // only for dx
//		SG_INDIRECT_SHADER_RESOURCE_VIEW,    // only for dx
//		SG_INDIRECT_UNORDERED_ACCESS_VIEW,   // only for dx
//	} IndirectArgumentType;
//
//#pragma region (Buffer)
//
//	typedef enum BufferCreationFlags
//	{
//		/// Default flag (Buffer will use aliased memory, buffer will not be cpu accessible until mapBuffer is called)
//		SG_BUFFER_CREATION_FLAG_NONE = 0x01,
//		/// Buffer will allocate its own memory (COMMITTED resource)
//		SG_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT = 0x02,
//		/// Buffer will be persistently mapped
//		SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT = 0x04,
//		/// Use ESRAM to store this buffer
//		SG_BUFFER_CREATION_FLAG_ESRAM = 0x08,
//		/// Flag to specify not to allocate descriptors for the resource
//		SG_BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION = 0x10,
//	} BufferCreationFlags;
//	SG_MAKE_ENUM_FLAG(uint32_t, BufferCreationFlags);
//
//	/// Data structure holding necessary info to create a Buffer
//	typedef struct BufferCreateDesc
//	{
//		/// Size of the buffer (in bytes)
//		uint64_t size;
//		/// Alignment
//		uint32_t alignment;
//		/// Decides which memory heap buffer will use (default, upload, readback)
//		ResourceMemoryUsage memoryUsage;
//		/// Creation flags of the buffer
//		BufferCreationFlags flags;
//		/// What type of queue the buffer is owned by
//		QueueType queueType;
//		/// What state will the buffer get created in
//		ResourceState startState;
//		/// Index of the first element accessible by the SRV/UAV (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
//		uint64_t firstElement;
//		/// Number of elements in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
//		uint64_t elementCount;
//		/// Size of each element (in bytes) in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
//		uint64_t structStride;
//		/// ICB draw type
//		IndirectArgumentType ICBDrawType;
//		/// ICB max vertex buffers slots count
//		uint32_t ICBMaxVertexBufferBind;
//		/// ICB max vertex buffers slots count
//		uint32_t ICBMaxFragmentBufferBind;
//		/// Set this to specify a counter buffer for this buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
//		struct Buffer* counterBuffer;
//		/// Format of the buffer (applicable to typed storage buffers (Buffer<T>)
//		TinyImageFormat format;
//		/// Flags specifying the suitable usage of this buffer (Uniform buffer, Vertex Buffer, Index Buffer,...)
//		DescriptorType descriptors;
//		/// Debug name used in gpu profile
//		const char* name;
//		uint32_t* pSharedNodeIndices;
//		uint32_t       nodeIndex;
//		uint32_t       sharedNodeIndexCount;
//	} BufferCreateDesc;
//
//	typedef struct SG_ALIGN(Buffer, 64)
//	{
//		/// CPU address of the mapped buffer (appliacable to buffers created in CPU accessible heaps (CPU, CPU_TO_GPU, GPU_TO_CPU)
//		void* pCpuMappedAddress;
//#if defined(SG_GRAPHIC_API_D3D12)
//		/// GPU Address - Cache to avoid calls to ID3D12Resource::GetGpuVirtualAddress
//		D3D12_GPU_VIRTUAL_ADDRESS        mDxGpuAddress;
//		/// Descriptor handle of the CBV in a CPU visible descriptor heap (applicable to BUFFER_USAGE_UNIFORM)
//		D3D12_CPU_DESCRIPTOR_HANDLE      mDxDescriptorHandles;
//		/// Offset from mDxDescriptors for srv descriptor handle
//		uint64_t                         mDxSrvOffset : 8;
//		/// Offset from mDxDescriptors for uav descriptor handle
//		uint64_t                         mDxUavOffset : 8;
//		/// Native handle of the underlying resource
//		ID3D12Resource* pDxResource;
//		/// Contains resource allocation info such as parent heap, offset in heap
//		D3D12MA::Allocation* pDxAllocation;
//#endif
//#if defined(SG_GRAPHIC_API_D3D11)
//		ID3D11Buffer* pDxResource;
//		ID3D11ShaderResourceView* pDxSrvHandle;
//		ID3D11UnorderedAccessView* pDxUavHandle;
//		uint64_t                         mPadA;
//		uint64_t                         mPadB;
//#endif
//#if defined(SG_GRAPHIC_API_VULKAN)
//		/// Native handle of the underlying resource
//		VkBuffer                         pVkBuffer;
//		/// Buffer view
//		VkBufferView                     pVkStorageTexelView;
//		VkBufferView                     pVkUniformTexelView;
//		/// Contains resource allocation info such as parent heap, offset in heap
//		struct VmaAllocation_T*		     pVkAllocation;
//		uint64_t                         offset;
//#endif
//#if defined(SG_GRAPHIC_API_GLES)
//		GLuint							 buffer;
//		GLenum							 target;
//		void* pGLCpuMappedAddress;
//#endif
//		uint64_t                         size : 32;
//		uint64_t                         descriptors : 20;
//		uint64_t                         memoryUsage : 3;
//		uint64_t                         nodeIndex : 4;
//	} Buffer;
//	// check for the alignment
//	SG_COMPILE_ASSERT(sizeof(Buffer) == 8 * sizeof(uint64_t));
//
//#pragma endregion (Buffer)
//
//#pragma region (Texture)
//
//	typedef enum TextureCreationFlags
//	{
//		/// Default flag (Texture will use default allocation strategy decided by the api specific allocator)
//		SG_TEXTURE_CREATION_FLAG_NONE = 0,
//		/// Texture will allocate its own memory (COMMITTED resource)
//		SG_TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT = 0x01,
//		/// Texture will be allocated in memory which can be shared among multiple processes
//		SG_TEXTURE_CREATION_FLAG_EXPORT_BIT = 0x02,
//		/// Texture will be allocated in memory which can be shared among multiple gpus
//		SG_TEXTURE_CREATION_FLAG_EXPORT_ADAPTER_BIT = 0x04,
//		/// Texture will be imported from a handle created in another process
//		SG_TEXTURE_CREATION_FLAG_IMPORT_BIT = 0x08,
//		/// Use ESRAM to store this texture
//		SG_TEXTURE_CREATION_FLAG_ESRAM = 0x10,
//		/// Use on-tile memory to store this texture
//		SG_TEXTURE_CREATION_FLAG_ON_TILE = 0x20,
//		/// Prevent compression meta data from generating (XBox)
//		SG_TEXTURE_CREATION_FLAG_NO_COMPRESSION = 0x40,
//		/// Force 2D instead of automatically determining dimension based on width, height, depth
//		SG_TEXTURE_CREATION_FLAG_FORCE_2D = 0x80,
//		/// Force 3D instead of automatically determining dimension based on width, height, depth
//		SG_TEXTURE_CREATION_FLAG_FORCE_3D = 0x100,
//		/// Display target
//		SG_TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET = 0x200,
//		/// Create an sRGB texture.
//		SG_TEXTURE_CREATION_FLAG_SRGB = 0x400,
//	} TextureCreationFlags;
//	SG_MAKE_ENUM_FLAG(uint32_t, TextureCreationFlags);
//
//	/// Data structure holding necessary info to create a Texture
//	typedef struct TextureCreateDesc
//	{
//		/// Texture creation flags (decides memory allocation strategy, sharing access,...)
//		TextureCreationFlags flags;
//		/// Width
//		uint32_t width;
//		/// Height
//		uint32_t height;
//		/// Depth (Should be 1 if not a mType is not TEXTURE_TYPE_3D)
//		uint32_t depth;
//		/// Texture array size (Should be 1 if texture is not a texture array or cubemap)
//		uint32_t arraySize;
//		/// Number of mip levels
//		uint32_t mipLevels;
//		/// Number of multisamples per pixel (currently Textures created with mUsage TEXTURE_USAGE_SAMPLED_IMAGE only support SAMPLE_COUNT_1)
//		SampleCount sampleCount;
//		/// The image quality level. The higher the quality, the lower the performance. The valid range is between zero and the value appropriate for mSampleCount
//		uint32_t sampleQuality;
//		///  image format
//		TinyImageFormat format;
//		/// Optimized clear value (recommended to use this same value when clearing the rendertarget)
//		ClearValue clearValue;
//		/// What state will the texture get created in
//		ResourceState startState;
//		/// Descriptor creation
//		DescriptorType descriptors;
//		/// Pointer to native texture handle if the texture does not own underlying resource
//		const void* pNativeHandle;
//		/// Debug name used in gpu profile
//		const char* name;
//		/// GPU indices to share this texture
//		uint32_t* pSharedNodeIndices;
//		/// Number of GPUs to share this texture
//		uint32_t sharedNodeIndexCount;
//		/// GPU which will own this texture
//		uint32_t nodeIndex;
//#if defined(SG_GRAPHIC_API_VULKAN)
//		VkSamplerYcbcrConversionInfo* pVkSamplerYcbcrConversionInfo;
//#endif
//	} TextureCreateDesc;
//
//	typedef struct VirtualTexture
//	{
//#if defined(SG_GRAPHIC_API_D3D12)
//		ID3D12Heap* pSparseImageMemory;
//		/// Array for Sparse texture's pages
//		void* pSparseCoordinates;
//		/// Array for heap memory offsets
//		void* pHeapRangeStartOffsets;
//#endif
//#if defined(SG_GRAPHIC_API_VULKAN)
//		/// Sparse queue binding information
//		VkBindSparseInfo bindSparseInfo;
//		/// Sparse image memory bindings of all memory-backed virtual tables
//		void* pSparseImageMemoryBinds;
//		/// Sparse ?aque memory bindings for the mip tail (if present)	
//		void* pOpaqueMemoryBinds;
//		/// First mip level in mip tail
//		uint32_t mipTailStart;
//		/// Lstly filled mip level in mip tail
//		uint32_t lastFilledMip;
//		/// Memory type for Sparse texture's memory
//		uint32_t sparseMemoryTypeIndex;
//		/// Sparse image memory bind info 
//		VkSparseImageMemoryBindInfo imageMemoryBindInfo;
//		/// Sparse image opaque memory bind info (mip tail)
//		VkSparseImageOpaqueMemoryBindInfo opaqueMemoryBindInfo;
//		/// First mip level in mip tail
//		uint32_t mipTailStart;
//#endif
//		/// Virtual Texture members
//		/// Contains all virtual pages of the texture
//		void* pPages;
//		/// Visibility data
//		Buffer* visibility;
//		/// PrevVisibility data
//		Buffer* prevVisibility;
//		/// Alive Page's Index
//		Buffer* alivePage;
//		/// Page's Index which should be removed
//		Buffer* removePage;
//		/// a { uint alive; uint remove; } count of pages which are alive or should be removed
//		Buffer* pageCounts;
//		/// Original Pixel image data
//		void* virtualImageData;
//		///  Total pages count
//		uint32_t virtualPageTotalCount;
//		/// Sparse Virtual Texture Width
//		uint64_t sparseVirtualTexturePageWidth;
//		/// Sparse Virtual Texture Height
//		uint64_t sparseVirtualTexturePageHeight;
//	} VirtualTexture;
//
//	typedef struct SG_ALIGN(Texture, 64)
//	{
//#if defined(SG_GRAPHIC_API_D3D12)
//		/// Descriptor handle of the SRV in a CPU visible descriptor heap (applicable to TEXTURE_USAGE_SAMPLED_IMAGE)
//		D3D12_CPU_DESCRIPTOR_HANDLE  mDxDescriptorHandles;
//		/// Native handle of the underlying resource
//		ID3D12Resource* pDxResource;
//		/// Contains resource allocation info such as parent heap, offset in heap
//		D3D12MA::Allocation* pDxAllocation;
//		uint64_t                     mHandleCount : 24;
//		uint64_t                     mUavStartIndex : 1;
//		uint32_t                     mDescriptorSize;
//#endif
//#if defined(SG_GRAPHIC_API_VULKAN)
//		/// Opaque handle used by shaders for doing read/write operations on the texture
//		VkImageView                  pVkSRVDescriptor;
//		/// Opaque handle used by shaders for doing read/write operations on the texture
//		VkImageView* pVkUAVDescriptors;
//		/// Opaque handle used by shaders for doing read/write operations on the texture
//		VkImageView                  pVkSRVStencilDescriptor;
//		/// Native handle of the underlying resource
//		VkImage                      pVkImage;
//		union
//		{
//			/// Contains resource allocation info such as parent heap, offset in heap
//			struct VmaAllocation_T* pVkAllocation;
//			VkDeviceMemory          pVkDeviceMemory;
//		};
//#endif
//#if defined(SG_GRAPHIC_API_D3D11)
//		ID3D11Resource* pDxResource;
//		ID3D11ShaderResourceView* pDxSRVDescriptor;
//		ID3D11UnorderedAccessView** pDxUAVDescriptors;
//		uint64_t                     mPadA;
//		uint64_t                     mPadB;
//#endif
//#if defined(SG_GRAPHIC_API_GLES)
//		GLuint						 mTexture;
//		GLenum						 mTarget;
//		GLenum						 mGlFormat;
//		GLenum						 mInternalFormat;
//		GLenum						 mType;
//#endif
//		VirtualTexture*			     pSvt;
//		/// Current state of the buffer
//		uint32_t                     width : 16;
//		uint32_t                     height : 16;
//		uint32_t                     depth : 16;
//		uint32_t                     mipLevels : 5;
//		uint32_t                     arraySizeMinusOne : 11;
//		uint32_t                     format : 8;
//		/// Flags specifying which aspects (COLOR,DEPTH,STENCIL) are included in the pVkImageView
//		uint32_t                     aspectMask : 4;
//		uint32_t                     nodeIndex : 4;
//		uint32_t                     uav : 1;
//		/// This value will be false if the underlying resource is not owned by the texture (swapchain textures,...)
//		uint32_t                     ownsImage : 1;
//	} Texture;
//	SG_COMPILE_ASSERT(sizeof(Texture) == 8 * sizeof(uint64_t));
//
//#pragma endregion (Texture)

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
	} RendererDesc;

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
		//struct VmaAllocator_T*			pVmaAllocator;
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

#define RENDERER_API_TEST	

	RENDERER_API_TEST void SG_CALLCONV renderer_test_func(const char* msg);
	RENDERER_API_TEST void SG_CALLCONV renderer_add_func(int a, size_t num);
	RENDERER_API_TEST void SG_CALLCONV renderer_end_func(const char* msg, bool end);

	//RENDERER_API_TEST void SG_CALLCONV init_renderer(const char* appName, const RendererCreateDesc* pDesc, Renderer** ppRenderer);
	//RENDERER_API_TEST void SG_CALLCONV remove_renderer(Renderer* pRenderer);

	RENDERER_API_TEST void SG_CALLCONV init_renderer_test(const char* appName, const RendererCreateDesc* pDesc, Renderer** ppRenderer);
	RENDERER_API_TEST void SG_CALLCONV remove_renderer_test(Renderer* pRenderer);

}