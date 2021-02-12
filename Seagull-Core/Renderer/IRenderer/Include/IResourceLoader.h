#pragma once

#include <stdint.h>

#include "IRenderer.h"

namespace SG
{

	typedef struct MappedMemoryRange
	{
		uint8_t* pData;
		Buffer* pBuffer;
		uint64_t offset;
		uint64_t size;
		uint32_t flags;
	} MappedMemoryRange;

	typedef enum TextureContainerType
	{
		/// Use whatever container is designed for that platform
		/// Windows, macOS, Linux - TEXTURE_CONTAINER_DDS
		/// iOS, Android          - TEXTURE_CONTAINER_KTX
		SG_TEXTURE_CONTAINER_DEFAULT = 0,
		/// Explicit container types
		/// .dds
		SG_TEXTURE_CONTAINER_DDS,
		/// .ktx
		SG_TEXTURE_CONTAINER_KTX,
		/// .gnf
		SG_TEXTURE_CONTAINER_GNF,
		/// .basis
		SG_TEXTURE_CONTAINER_BASIS,
		/// .svt
		SG_TEXTURE_CONTAINER_SVT,
	} TextureContainerType;

	typedef struct BufferLoadDesc
	{
		Buffer** ppBuffer;
		const void* pData;
		BufferDesc  desc;
		/// Force Reset buffer to NULL
		bool        forceReset;
	} BufferLoadDesc;

	typedef struct TextureLoadDesc
	{
		Texture** ppTexture;
		/// Load empty texture
		TextureDesc* desc;
		/// Filename without extension. Extension will be determined based on mContainer
		const char* fileName;
		/// The index of the GPU in SLI/Cross-Fire that owns this texture
		uint32_t             nodeIndex;
		/// Following is ignored if pDesc != NULL.  pDesc->mFlags will be considered instead.
		TextureCreationFlags creationFlag;
		/// The texture file format (dds/ktx/...)
		TextureContainerType container;
	} TextureLoadDesc;

	typedef struct Geometry
	{
		struct Hair
		{
			uint32_t vertexCountPerStrand;
			uint32_t guideCountPerStrand;
		};

		struct ShadowData
		{
			void* pIndices;
			void* pAttributes[SG_MAX_VERTEX_ATTRIBS];
		};

		/// Index buffer to bind when drawing this geometry
		Buffer* pIndexBuffer;
		/// The array of vertex buffers to bind when drawing this geometry
		Buffer* pVertexBuffers[SG_MAX_VERTEX_BINDINGS];
		/// The array of traditional draw arguments to draw each subset in this geometry
		IndirectDrawIndexArguments* pDrawArgs;
		/// Shadow copy of the geometry vertex and index data if requested through the load flags
		ShadowData* pShadow;

		/// The array of joint inverse bind-pose matrices ( object-space )
		Matrix4* pInverseBindPoses;
		/// The array of data to remap skin batch local joint ids to global joint ids
		uint32_t* pJointRemaps;
		/// The array of vertex buffer strides to bind when drawing this geometry
		uint32_t vertexStrides[SG_MAX_VERTEX_BINDINGS];
		/// Hair data
		Hair hair;

		/// Number of vertex buffers in this geometry
		uint32_t vertexBufferCount : 8;
		/// Index type (32 or 16 bit)
		uint32_t indexType : 2;
		/// Number of joints in the skinned geometry
		uint32_t jointCount : 16;
		/// Number of draw args in the geometry
		uint32_t drawArgCount;
		/// Number of indices in the geometry
		uint32_t indexCount;
		/// Number of vertices in the geometry
		uint32_t vertexCount;
		uint32_t pad[3];
	} Geometry;
	SG_COMPILE_ASSERT(sizeof(Geometry) % 16 == 0);

	typedef enum GeometryLoadFlags
	{
		/// Keep shadow copy of indices and vertices for CPU
		SG_GEOMETRY_LOAD_FLAG_SHADOWED = 0x1,
		/// Use structured buffers instead of raw buffers
		SG_GEOMETRY_LOAD_FLAG_STRUCTURED_BUFFERS = 0x2,
	} GeometryLoadFlags;
	SG_MAKE_ENUM_FLAG(uint32_t, GeometryLoadFlags);

	typedef struct GeometryLoadDesc
	{
		/// Output geometry
		Geometry** ppGeometry;
		/// Filename of geometry container
		const char* fileName;
		/// Loading flags
		GeometryLoadFlags flags;
		/// Linked gpu node
		uint32_t          nodeIndex;
		/// Specifies how to arrange the vertex data loaded from the file into GPU memory
		VertexLayout* pVertexLayout;
	} GeometryLoadDesc;

	typedef struct VirtualTexturePageInfo
	{
		unsigned pageAlive;
		unsigned texID;
		unsigned mipLevel;
		unsigned padding;
	} VirtualTexturePageInfo;

	typedef struct BufferUpdateDesc
	{
		Buffer*   pBuffer;
		uint64_t  dstOffset;
		uint64_t  size;

		/// To be filled by the caller
		/// Example:
		/// BufferUpdateDesc update = { pBuffer, bufferDstOffset };
		/// beginUpdateResource(&update);
		/// ParticleVertex* vertices = (ParticleVertex*)update.pMappedData;
		///   for (uint32_t i = 0; i < particleCount; ++i)
		///	    vertices[i] = { rand() };
		/// endUpdateResource(&update, &token);
		void* pMappedData;

		/// Internal
		struct
		{
			MappedMemoryRange mappedRange;
		} internal;
	} BufferUpdateDesc;

	/// #NOTE: Only use for procedural textures which are created on CPU (noise textures, font texture, ...)
	typedef struct TextureUpdateDesc
	{
		Texture* pTexture;
		uint32_t mipLevel;
		uint32_t arrayLayer;

		/// To be filled by the caller
		/// Example:
		/// BufferUpdateDesc update = { pTexture, 2, 1 };
		/// beginUpdateResource(&update);
		/// Row by row copy is required if mDstRowStride > mSrcRowStride. Single memcpy will work if mDstRowStride == mSrcRowStride
		/// 2D
		/// for (uint32_t r = 0; r < update.mRowCount; ++r)
		///     memcpy(update.pMappedData + r * update.mDstRowStride, srcPixels + r * update.mSrcRowStride, update.mSrcRowStride);
		/// 3D
		/// for (uint32_t z = 0; z < depth; ++z)
		/// {
		///     uint8_t* dstData = update.pMappedData + update.mDstSliceStride * z;
		///     uint8_t* srcData = srcPixels + update.mSrcSliceStride * z;
		///     for (uint32_t r = 0; r < update.mRowCount; ++r)
		///         memcpy(dstData + r * update.mDstRowStride, srcData + r * update.mSrcRowStride, update.mSrcRowStride);
		/// }
		/// endUpdateResource(&update, &token);
		uint8_t* pMappedData;
		/// Size of each row in destination including padding - Needs to be respected otherwise texture data will be corrupted if dst row stride is not the same as src row stride
		uint32_t dstRowStride;
		/// Number of rows in this slice of the texture
		uint32_t rowCount;
		/// Src row stride for convenience (mRowCount * width * texture format size)
		uint32_t srcRowStride;
		/// Size of each slice in destination including padding - Use for offsetting dst data updating 3D textures
		uint32_t dstSliceStride;
		/// Size of each slice in src - Use for offsetting src data when updating 3D textures
		uint32_t srcSliceStride;

		/// Internal
		struct
		{
			MappedMemoryRange mMappedRange;
		} internal;
	} TextureUpdateDesc;

	typedef enum ShaderStageLoadFlags
	{
		SG_SHADER_STAGE_LOAD_FLAG_NONE = 0x0,
		/// D3D12 only - Enable passing primitive id to pixel shader
		SG_SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID = 0x1,
	} ShaderStageLoadFlags;
	SG_MAKE_ENUM_FLAG(uint32_t, ShaderStageLoadFlags);

	typedef struct ShaderStageLoadDesc
	{
		const char* fileName;
		ShaderMacro* pMacros;
		uint32_t     macroCount;
		const char* entryPointName;
		ShaderStageLoadFlags flags;
	} ShaderStageLoadDesc;

	typedef struct ShaderLoadDesc
	{
		ShaderStageLoadDesc stages[SG_SHADER_STAGE_COUNT];
		ShaderTarget        target;
	} ShaderLoadDesc;

	typedef struct PipelineCacheLoadDesc
	{
		const char*		   fileName;
		PipelineCacheFlags flags;
	} PipelineCacheLoadDesc;

	typedef struct PipelineCacheSaveDesc
	{
		const char* fileName;
	} PipelineCacheSaveDesc;

	typedef uint64_t SyncToken;

	typedef struct ResourceLoaderDesc
	{
		uint64_t bufferSize;
		uint32_t bufferCount;
		bool     singleThreaded;
	} ResourceLoaderDesc;

	extern ResourceLoaderDesc gDefaultResourceLoaderDesc;
	
	// MARK: - Resource Loader Functions

	void init_resource_loader_interface(Renderer* pRenderer, ResourceLoaderDesc* pDesc = nullptr);
	void exit_resource_loader_interface(Renderer* pRenderer);

	// MARK: addResource and updateResource

	/// Adding and updating resources can be done using a addResource or
	/// beginUpdateResource/endUpdateResource pair.
	/// if addResource(BufferLoadDesc) is called with a data size larger than the ResourceLoader's staging buffer, the ResourceLoader
	/// will perform multiple copies/flushes rather than failing the copy. ( slicing the buffer )

	/// If token is NULL, the resource will be available when allResourceLoadsCompleted() returns true.
	/// If token is non NULL, the resource will be available after isTokenCompleted(token) returns true.
	void add_resource(BufferLoadDesc* pBufferDesc, SyncToken* token);
	void add_resource(TextureLoadDesc* pTextureDesc, SyncToken* token);
	void add_resource(GeometryLoadDesc* pGeomDesc, SyncToken* token);

	void begin_update_resource(BufferUpdateDesc* pBufferDesc);
	void begin_update_resource(TextureUpdateDesc* pTextureDesc);
	void end_update_resource(BufferUpdateDesc* pBuffer, SyncToken* token);
	void end_update_resource(TextureUpdateDesc* pTexture, SyncToken* token);

	// MARK: removeResource

	void remove_resource(Buffer* pBuffer);
	void remove_resource(Texture* pTexture);
	void remove_resource(Geometry* pGeom);

	// MARK: Waiting for Loads

	/// Returns whether all submitted resource loads and updates have been completed.
	bool all_resource_loads_completed();

	/// Blocks the calling thread until allResourceLoadsCompleted() returns true.
	/// Note that if more resource loads or updates are submitted from a different thread while
	/// while the calling thread is blocked, those loads or updates are not guaranteed to have
	/// completed when this function returns.
	void wait_for_all_resource_loads();

	/// A SyncToken is an array of monotonically(单调地，无变化地) increasing integers.
	/// getLastTokenCompleted() returns the last value for which
	/// isTokenCompleted(token) is guaranteed to return true.
	SyncToken get_last_token_completed();
	bool is_token_completed(const SyncToken* token);
	void wait_for_token(const SyncToken* token);

	/// Either loads the cached shader bytecode or compiles the shader to create new bytecode depending on whether source is newer than binary
	void add_shader(Renderer* pRenderer, const ShaderLoadDesc* pDesc, Shader** pShader);

	/// Save/Load pipeline cache from disk
	void add_pipeline_cache(Renderer* pRenderer, const PipelineCacheLoadDesc* pDesc, PipelineCache** ppPipelineCache);
	void save_pipeline_cache(Renderer* pRenderer, PipelineCache* pPipelineCache, PipelineCacheSaveDesc* pDesc);

}