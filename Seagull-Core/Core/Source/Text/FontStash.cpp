#include "FontStash.h"

#define FONTSTASH_IMPLEMENTATION
#ifdef SG_PLATFORM_WINDOWS
	#include <windows.h>
#endif
#include <include/fontstash.h>

#include "include/IRenderer.h"
#include "include/IResourceLoader.h"

#include "Interface/IFileSystem.h"

#include <include/tinyimageformat_query.h>

#include <include/EASTL/vector.h>
#include <include/EASTL/string.h>

#include "Interface/IMemory.h"

namespace SG
{

	typedef struct GPURingBuffer
	{
		Renderer* pRenderer;
		Buffer*   pBuffer;

		uint32_t bufferAlignment;
		uint64_t maxBufferSize;
		uint64_t currentBufferOffset;
	} GPURingBuffer;

	typedef struct GPURingBufferOffset
	{
		Buffer*  pBuffer;
		uint64_t offset;
	} GPURingBufferOffset;

	static inline uint32_t round_up(uint32_t value, uint32_t multiple) { return ((value + multiple - 1) / multiple) * multiple; }

	static inline void add_gpu_ring_buffer(Renderer* pRenderer, const BufferCreateDesc* pBufferDesc, GPURingBuffer** ppRingBuffer)
	{
		GPURingBuffer* pRingBuffer = (GPURingBuffer*)sg_calloc(1, sizeof(GPURingBuffer));
		pRingBuffer->pRenderer = pRenderer;
		pRingBuffer->maxBufferSize = pBufferDesc->size;
		pRingBuffer->bufferAlignment = sizeof(float[4]);

		BufferLoadDesc loadDesc = {};
		loadDesc.desc = *pBufferDesc;
		loadDesc.ppBuffer = &pRingBuffer->pBuffer;
		add_resource(&loadDesc, nullptr);

		*ppRingBuffer = pRingBuffer;
	}

	static inline void add_uniform_gpu_ring_buffer(Renderer* pRenderer, uint32_t requiredUniformBufferSize, GPURingBuffer** ppRingBuffer, bool const ownMemory = false, ResourceMemoryUsage memoryUsage = SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
	{
		GPURingBuffer* pRingBuffer = (GPURingBuffer*)sg_calloc(1, sizeof(GPURingBuffer));
		pRingBuffer->pRenderer = pRenderer;

		const uint32_t uniformBufferAlignment = (uint32_t)pRenderer->pActiveGpuSettings->uniformBufferAlignment;
		const uint32_t maxUniformBufferSize = requiredUniformBufferSize;
		pRingBuffer->bufferAlignment = uniformBufferAlignment;
		pRingBuffer->maxBufferSize = maxUniformBufferSize;

		BufferCreateDesc ubDesc = {};
#if defined(SG_GRAPHIC_API_D3D11)
		ubDesc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_CPU_ONLY;
		ubDesc.flags = SG_BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
#else
		ubDesc.descriptors = SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.memoryUsage = memoryUsage;
		ubDesc.flags = (ubDesc.memoryUsage != SG_RESOURCE_MEMORY_USAGE_GPU_ONLY ? SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT : SG_BUFFER_CREATION_FLAG_NONE) |
			SG_BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
#endif
		if (ownMemory)
			ubDesc.flags |= SG_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
		ubDesc.size = maxUniformBufferSize;

		BufferLoadDesc loadDesc = {};
		loadDesc.desc = ubDesc;
		loadDesc.ppBuffer = &pRingBuffer->pBuffer;
		add_resource(&loadDesc, nullptr);

		*ppRingBuffer = pRingBuffer;
	}

	static inline void remove_gpu_ring_buffer(GPURingBuffer* pRingBuffer)
	{
		remove_resource(pRingBuffer->pBuffer);
		sg_free(pRingBuffer);
	}

	static inline void reset_gpu_ring_buffer(GPURingBuffer* pRingBuffer)
	{
		pRingBuffer->currentBufferOffset = 0;
	}

	static inline GPURingBufferOffset get_gpu_ring_buffer_offset(GPURingBuffer* pRingBuffer, uint32_t memoryRequirement, uint32_t alignment = 0)
	{
		uint32_t alignedSize = round_up(memoryRequirement, alignment ? alignment : pRingBuffer->bufferAlignment);

		if (alignedSize > pRingBuffer->maxBufferSize)
		{
			ASSERT(false && "Ring Buffer too small for memory requirement");
			return { nullptr, 0 };
		}

		if (pRingBuffer->currentBufferOffset + alignedSize >= pRingBuffer->maxBufferSize) // exceed the max buffer size bound
		{
			pRingBuffer->currentBufferOffset = 0;
		}

		GPURingBufferOffset ret = { pRingBuffer->pBuffer, pRingBuffer->currentBufferOffset };
		pRingBuffer->currentBufferOffset += alignedSize;

		return ret;
	}

	class FontStash_Impl
	{
	public:
		FontStash_Impl()
		{
			pCurrentTexture = {};
			width = 0;
			height = 0;
			pContext = nullptr;
			isText3D = false;
		}

		bool OnInit(Renderer* renderer, int width, int height, uint32_t ringSizeBytes)
		{
			pRenderer = renderer;

			// create image for font atlas texture
			TextureCreateDesc desc = {};
			desc.arraySize = 1;
			desc.depth = 1;
			desc.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE;
			desc.format = TinyImageFormat_R8_UNORM;
			desc.height = height;
			desc.mipLevels = 1;
			desc.sampleCount = SG_SAMPLE_COUNT_1;
			desc.startState =  SG_RESOURCE_STATE_COMMON;
			desc.width = width;
			desc.name = "FontStash Texture";
			TextureLoadDesc loadDesc = {};
			loadDesc.ppTexture = &pCurrentTexture;
			loadDesc.desc = &desc;
			add_resource(&loadDesc, nullptr);

			// create FONS context
			FONSparams params;
			memset(&params, 0, sizeof(params));

			params.width = width;
			params.height = height;
			params.flags = (unsigned char)FONS_ZERO_TOPLEFT;
			params.renderCreate = FonsImplGenerateTexture;
			params.renderUpdate = FonsImplModifyTexture;
			params.renderDelete = FonsImplRemoveTexture;
			params.renderDraw = FonsImplRenderText;
			params.userPtr = this;

			pContext = fonsCreateInternal(&params);

			// Rendering resources
			SamplerCreateDesc samplerDesc = { SG_FILTER_LINEAR,
										SG_FILTER_LINEAR,
										SG_MIPMAP_MODE_NEAREST,
										SG_ADDRESS_MODE_CLAMP_TO_EDGE,
										SG_ADDRESS_MODE_CLAMP_TO_EDGE,
										SG_ADDRESS_MODE_CLAMP_TO_EDGE };
			add_sampler(pRenderer, &samplerDesc, &pDefaultSampler);

#ifdef USE_TEXT_PRECOMPILED_SHADERS
			BinaryShaderCreateDesc binaryShaderDesc = {};
			binaryShaderDesc.stages = SG_SHADER_STAGE_VERT | SG_SHADER_STAGE_FRAG;
			binaryShaderDesc.vert.byteCodeSize = sizeof(gShaderFontstash2DVert);
			binaryShaderDesc.vert.pByteCode = (char*)gShaderFontstash2DVert;
			binaryShaderDesc.vert.pEntryPoint = "main";
			binaryShaderDesc.frag.byteCodeSize = sizeof(gShaderFontstashFrag);
			binaryShaderDesc.frag.pByteCode = (char*)gShaderFontstashFrag;
			binaryShaderDesc.frag.pEntryPoint = "main";
			add_shader_binary(pRenderer, &binaryShaderDesc, &pShaders[0]);
			binaryShaderDesc.vert.byteCodeSize = sizeof(gShaderFontstash3DVert);
			binaryShaderDesc.vert.pByteCode = (char*)gShaderFontstash3DVert;
			binaryShaderDesc.vert.pEntryPoint = "main";
			add_shader_binary(pRenderer, &binaryShaderDesc, &pShaders[1]);
#else
			ShaderLoadDesc text2DShaderDesc = {};
			text2DShaderDesc.stages[0] = { "FontsShader/fontstash2D.vert", nullptr, 0, nullptr };
			text2DShaderDesc.stages[1] = { "FontsShader/fontstash.frag", nullptr, 0, nullptr };
			ShaderLoadDesc text3DShaderDesc = {};
			text3DShaderDesc.stages[0] = { "FontsShader/fontstash3D.vert", nullptr, 0, nullptr };
			text3DShaderDesc.stages[1] = { "FontsShader/fontstash.frag", nullptr, 0, nullptr };

			add_shader(pRenderer, &text2DShaderDesc, &pShaders[0]);
			add_shader(pRenderer, &text3DShaderDesc, &pShaders[1]);
#endif

			RootSignatureCreateDesc textureRootDesc = { pShaders, 2 };
			const char* pStaticSamplers[] = { "uSampler0" };
			textureRootDesc.staticSamplerCount = 1;
			textureRootDesc.ppStaticSamplerNames = pStaticSamplers;
			textureRootDesc.ppStaticSamplers = &pDefaultSampler;
			add_root_signature(pRenderer, &textureRootDesc, &pRootSignature);

			add_uniform_gpu_ring_buffer(pRenderer, 65536, &pUniformRingBuffer, true);

			uint64_t size = sizeof(Matrix4);
			DescriptorSetCreateDesc setDesc = { pRootSignature, SG_DESCRIPTOR_UPDATE_FREQ_NONE, 2 };
			add_descriptor_set(pRenderer, &setDesc, &pDescriptorSets);
			DescriptorData setParams[2] = {};
			setParams[0].name = "uniformBlock_rootcbv";
			setParams[0].ppBuffers = &pUniformRingBuffer->pBuffer;
			setParams[0].sizes = &size;
			setParams[1].name = "uTex0";
			setParams[1].ppTextures = &pCurrentTexture;
			update_descriptor_set(pRenderer, 0, pDescriptorSets, 2, setParams);
			update_descriptor_set(pRenderer, 1, pDescriptorSets, 2, setParams);

			BufferCreateDesc vbDesc = {};
			vbDesc.descriptors = SG_DESCRIPTOR_TYPE_VERTEX_BUFFER;
			vbDesc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
			vbDesc.size = ringSizeBytes;
			vbDesc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
			add_gpu_ring_buffer(pRenderer, &vbDesc, &pMeshRingBuffer);

			return true;
		}

		void OnExit()
		{
			// unload fontstash context
			fonsDeleteInternal(pContext);

			remove_resource(pCurrentTexture);

			// unload font buffers
			for (unsigned int i = 0; i < (uint32_t)fontBuffers.size(); i++)
				sg_free(fontBuffers[i]);

			remove_descriptor_set(pRenderer, pDescriptorSets);
			remove_root_signature(pRenderer, pRootSignature);

			for (uint32_t i = 0; i < 2; ++i)
			{
				remove_shader(pRenderer, pShaders[i]);
			}

			remove_gpu_ring_buffer(pMeshRingBuffer);
			remove_gpu_ring_buffer(pUniformRingBuffer);
			remove_sampler(pRenderer, pDefaultSampler);
		}

		bool OnLoad(RenderTarget** pRts, uint32_t count, PipelineCache* pCache)
		{
			VertexLayout vertexLayout = {};
			vertexLayout.attribCount = 2;
			vertexLayout.attribs[0].semantic = SG_SEMANTIC_POSITION;
			vertexLayout.attribs[0].format = TinyImageFormat_R32G32_SFLOAT;
			vertexLayout.attribs[0].binding = 0;
			vertexLayout.attribs[0].location = 0;
			vertexLayout.attribs[0].offset = 0;
			
			vertexLayout.attribs[1].semantic = SG_SEMANTIC_TEXCOORD0;
			vertexLayout.attribs[1].format = TinyImageFormat_R32G32_SFLOAT;
			vertexLayout.attribs[1].binding = 0;
			vertexLayout.attribs[1].location = 1;
			vertexLayout.attribs[1].offset = TinyImageFormat_BitSizeOfBlock(vertexLayout.attribs[0].format) / 8;

			BlendStateDesc blendStateDesc = {};
			blendStateDesc.srcFactors[0] = SG_BLEND_CONST_SRC_ALPHA;
			blendStateDesc.dstFactors[0] = SG_BLEND_CONST_ONE_MINUS_SRC_ALPHA;
			blendStateDesc.srcAlphaFactors[0] = SG_BLEND_CONST_SRC_ALPHA;
			blendStateDesc.dstAlphaFactors[0] = SG_BLEND_CONST_ONE_MINUS_SRC_ALPHA;
			blendStateDesc.masks[0] = SG_BLEND_COLOR_MASK_ALL;
			blendStateDesc.renderTargetMask = SG_BLEND_STATE_TARGET_ALL;
			blendStateDesc.independentBlend = false;

			DepthStateDesc depthStateDesc[2] = {}; // one for 2D, one for 3D
			depthStateDesc[0].depthTest = false;
			depthStateDesc[0].depthWrite = false;

			depthStateDesc[1].depthTest = true;
			depthStateDesc[1].depthWrite = true;
			depthStateDesc[1].depthFunc = SG_COMPARE_MODE_LEQUAL;

			RasterizerStateDesc rasterizerStateDesc[2] = {};
			rasterizerStateDesc[0].cullMode = SG_CULL_MODE_NONE;
			rasterizerStateDesc[0].scissor = true;

			rasterizerStateDesc[1].cullMode = SG_CULL_MODE_BACK;
			rasterizerStateDesc[1].scissor = true;

			PipelineCreateDesc pipelineDesc = {};
			pipelineDesc.pCache = pCache;
			pipelineDesc.type = SG_PIPELINE_TYPE_GRAPHICS;
			pipelineDesc.graphicsDesc.primitiveTopo = SG_PRIMITIVE_TOPO_TRI_LIST;
			pipelineDesc.graphicsDesc.renderTargetCount = 1;
			pipelineDesc.graphicsDesc.sampleCount = SG_SAMPLE_COUNT_1;
			pipelineDesc.graphicsDesc.pBlendState = &blendStateDesc;
			pipelineDesc.graphicsDesc.pRootSignature = pRootSignature;
			pipelineDesc.graphicsDesc.pVertexLayout = &vertexLayout;
			pipelineDesc.graphicsDesc.renderTargetCount = 1;
			pipelineDesc.graphicsDesc.sampleCount = pRts[0]->sampleCount;
			pipelineDesc.graphicsDesc.sampleQuality = pRts[0]->sampleQuality;
			pipelineDesc.graphicsDesc.pColorFormats = &pRts[0]->format;

			for (uint32_t i = 0; i < eastl::min(count, 2U); ++i)
			{
				pipelineDesc.graphicsDesc.depthStencilFormat = (i > 0) ? pRts[1]->format : TinyImageFormat_UNDEFINED;
				pipelineDesc.graphicsDesc.pShaderProgram = pShaders[i];
				pipelineDesc.graphicsDesc.pDepthState = &depthStateDesc[i];
				pipelineDesc.graphicsDesc.pRasterizerState = &rasterizerStateDesc[i];
				add_pipeline(pRenderer, &pipelineDesc, &pPipelines[i]);
			}

			scaleBias = { 2.0f / (float)pRts[0]->width, -2.0f / (float)pRts[0]->height };

			return true;
		}

		void OnUnload()
		{
			for (uint32_t i = 0; i < 2; ++i)
			{
				if (pPipelines[i])
					remove_pipeline(pRenderer, pPipelines[i]);

				pPipelines[i] = {};
			}
		}

		static int  FonsImplGenerateTexture(void* userPtr, int width, int height);
		static void FonsImplModifyTexture(void* userPtr, int* rect, const unsigned char* data);
		static void FonsImplRenderText(void* userPtr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts);
		static void FonsImplRemoveTexture(void* userPtr);

		Renderer* pRenderer;
		FONScontext* pContext;

		const uint8_t* pPixels;
		Texture* pCurrentTexture;
		bool     hadUpdateTexture;

		uint32_t width;
		uint32_t height;
		Vec2     scaleBias;

		eastl::vector<void*>         fontBuffers;
		eastl::vector<uint32_t>      fontBufferSizes;
		eastl::vector<eastl::string> fontNames;

		Matrix4 projViewMat;
		Matrix4 worldMat;
		Cmd* pCmd;

		Shader* pShaders[2];
		RootSignature* pRootSignature;
		DescriptorSet* pDescriptorSets;
		Pipeline* pPipelines[2];

		Sampler* pDefaultSampler;
		GPURingBuffer* pUniformRingBuffer;
		GPURingBuffer* pMeshRingBuffer;
		Vec2           dpiScale;
		float          dpiScaleMin;
		bool           isText3D;
	};

	bool FontStash::OnInit(Renderer* pRenderer, uint32_t width, uint32_t height, uint32_t ringSizeBytes)
	{
		impl = sg_placement_new<FontStash_Impl>(sg_calloc(1, sizeof(FontStash_Impl)));
		impl->dpiScale = get_dpi_scale();
		impl->dpiScaleMin = eastl::min(impl->dpiScale.x, impl->dpiScale.y);

		width = width * (int)ceilf(impl->dpiScale.x);
		height = height * (int)ceilf(impl->dpiScale.y);

		bool success = impl->OnInit(pRenderer, width, height, ringSizeBytes);
		fontMaxSize = eastl::min(width, height) / 10.0f;    // see fontstash.h, line 1271, for fontSize calculation

		return success;
	}

	void FontStash::OnExit()
	{
		impl->OnExit();
		impl->~FontStash_Impl();
		sg_free(impl);
	}

	bool FontStash::OnLoad(RenderTarget** pRts, uint32_t count, PipelineCache* pCache)
	{
		return impl->OnLoad(pRts, count, pCache);
	}

	void FontStash::OnUnload()
	{
		impl->OnUnload();
	}

	// read the font's data from the filestream and use Fons to 
	int FontStash::DefineFont(const char* identification, const char* pFontPath)
	{
		FONScontext* fs = impl->pContext;

		FileStream fh = {};
		if (sgfs_open_stream_from_path(SG_RD_FONTS, pFontPath, SG_FM_READ_BINARY, &fh))
		{
			ssize_t bytes = sgfs_get_stream_file_size(&fh);
			void* buffer = sg_malloc(bytes);
			sgfs_read_from_stream(&fh, buffer, bytes);

			// add buffer to font buffers for cleanup
			impl->fontBuffers.emplace_back(buffer);
			impl->fontBufferSizes.emplace_back((uint32_t)bytes);
			impl->fontNames.emplace_back(pFontPath);

			sgfs_close_stream(&fh);

			return fonsAddFontMem(fs, identification, (unsigned char*)buffer, (int)bytes, 0);
		}
	}

	void* FontStash::GetFontBuffer(uint32_t fontId)
	{
		if (fontId < impl->fontBuffers.size())
			return impl->fontBuffers[fontId];
		return nullptr;
	}

	uint32_t FontStash::GetFontBufferSize(uint32_t fontId)
	{
		if (fontId < impl->fontBufferSizes.size())
			return impl->fontBufferSizes[fontId];
		return UINT_MAX;
	}

	void FontStash::OnDrawText(Cmd* pCmd, const char* message, float x, float y, int fontID, unsigned int color, float size, float spacing, float blur)
	{
		impl->isText3D = false;
		impl->pCmd = pCmd;
		// clamp the font size to max size.
		// precomputed font texture puts limitation to the maximum size.
		size = eastl::min(size, fontMaxSize);

		FONScontext* fs = impl->pContext;
		fonsSetSize(fs, size * impl->dpiScaleMin);
		fonsSetFont(fs, fontID);
		fonsSetColor(fs, color);
		fonsSetSpacing(fs, spacing * impl->dpiScaleMin);
		fonsSetBlur(fs, blur);
		fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

		// considering the retina scaling:
		// the render target is already scaled up (w/ retina) and the (x,y) position given to this function
		// is expected to be in the render target's area. Hence, we don't scale up the position again.
		fonsDrawText(fs, x /** impl->mDpiScale.x*/, y /** impl->mDpiScale.y*/, message, nullptr);
	}

	void FontStash::OnDrawText(struct Cmd* pCmd, const char* message, const Matrix4& projView, const Matrix4& worldMat, int fontID, unsigned int color /*= 0xffffffff*/, float size /*= 16.0f*/, float spacing /*= 0.0f*/, float blur /*= 0.0f*/)
	{
		impl->isText3D = true;
		impl->projViewMat = projView;
		impl->worldMat = worldMat;
		impl->pCmd = pCmd;
		// clamp the font size to max size.
		// Precomputed font texture puts limitation to the maximum size.
		size = eastl::min(size, fontMaxSize);

		FONScontext* fs = impl->pContext;
		fonsSetSize(fs, size * impl->dpiScaleMin);
		fonsSetFont(fs, fontID);
		fonsSetColor(fs, color);
		fonsSetSpacing(fs, spacing * impl->dpiScaleMin);
		fonsSetBlur(fs, blur);
		fonsSetAlign(fs, FONS_ALIGN_CENTER | FONS_ALIGN_MIDDLE);

		fonsDrawText(fs, 0.0f, 0.0f, message, nullptr);
	}

	float FontStash::MeasureText(float* outBounds, const char* message, float x, float y, int fontID, unsigned int color /*= 0xffffffff*/, float size /*= 16.0f*/, float spacing /*= 0.0f*/, float blur /*= 0.0f*/)
	{
		if (outBounds == nullptr)
			return 0;

		const int    messageLength = (int)strlen(message);
		FONScontext* fs = impl->pContext;
		fonsSetSize(fs, size * impl->dpiScaleMin);
		fonsSetFont(fs, fontID);
		fonsSetColor(fs, color);
		fonsSetSpacing(fs, spacing * impl->dpiScaleMin);
		fonsSetBlur(fs, blur);
		fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

		// considering the retina scaling:
		// the render target is already scaled up (w/ retina) and the (x,y) position given to this function
		// is expected to be in the render target's area. Hence, we don't scale up the position again.
		return fonsTextBounds(fs, x /** impl->mDpiScale.x*/, y /** impl->mDpiScale.y*/, message, message + messageLength, outBounds);
	}

	/// FONS renderer implementation overwritten
	int  FontStash_Impl::FonsImplGenerateTexture(void* userPtr, int width, int height)
	{
		FontStash_Impl* ctx = (FontStash_Impl*)userPtr;
		ctx->width = width;
		ctx->height = height;

		ctx->hadUpdateTexture = true;

		return 1;
	}

	void FontStash_Impl::FonsImplModifyTexture(void* userPtr, int* rect, const unsigned char* data)
	{
		UNREF_PARAM(rect);

		FontStash_Impl* ctx = (FontStash_Impl*)userPtr;

		ctx->pPixels = data;
		ctx->hadUpdateTexture = true;
	}

	void FontStash_Impl::FonsImplRenderText(void* userPtr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts)
	{
		FontStash_Impl* ctx = (FontStash_Impl*)userPtr;
		if (!ctx->pCurrentTexture)
			return;

		Cmd* pCmd = ctx->pCmd;

		if (ctx->hadUpdateTexture)
		{
			// #TODO: Investigate - Causes hang on low-mid end Android phones (tested on Samsung Galaxy A50s)
#ifndef __ANDROID__
			wait_queue_idle(pCmd->pQueue);
#endif
			SyncToken token = {};
			TextureUpdateDesc updateDesc = {};
			updateDesc.pTexture = ctx->pCurrentTexture;
			begin_update_resource(&updateDesc);
			for (uint32_t r = 0; r < updateDesc.rowCount; ++r)
			{
				memcpy(updateDesc.pMappedData + r * updateDesc.dstRowStride,
					ctx->pPixels + r * updateDesc.srcRowStride, updateDesc.srcRowStride);
			}
			end_update_resource(&updateDesc, &token);
			wait_for_token(&token);

			ctx->hadUpdateTexture = false;
		}

		GPURingBufferOffset buffer = get_gpu_ring_buffer_offset(ctx->pMeshRingBuffer, nverts * sizeof(Vec4));
		BufferUpdateDesc update = { buffer.pBuffer, buffer.offset };
		begin_update_resource(&update);
		Vec4* vtx = (Vec4*)update.pMappedData;
		// build vertices
		for (int impl = 0; impl < nverts; impl++)
		{
			vtx[impl].x = verts[impl * 2 + 0];
			vtx[impl].y = verts[impl * 2 + 1];
			vtx[impl].z = tcoords[impl * 2 + 0];
			vtx[impl].w = tcoords[impl * 2 + 1];
		}
		end_update_resource(&update, nullptr);

		// extract color
		uint8_t* colorByte = (uint8_t*)colors;
		Vec4 color;
		for (int i = 0; i < 4; i++)
			color[i] = ((float)colorByte[i]) / 255.0f;

		uint32_t pipelineIndex = ctx->isText3D ? 1 : 0;
		Pipeline* pPipeline = ctx->pPipelines[pipelineIndex];
		ASSERT(pPipeline);

		cmd_bind_pipeline(pCmd, pPipeline);

		struct UniformData
		{
			Vec4 color;
			Vec2 scaleBias;
		} data;

		data.color = color;
		data.scaleBias = ctx->scaleBias;

		if (ctx->isText3D)
		{
			Matrix4 mvp = ctx->projViewMat * ctx->worldMat;
			data.color = color;
			data.scaleBias.x = -data.scaleBias.x;

			// update the matrix
			GPURingBufferOffset uniformBlock = {};
			uniformBlock = get_gpu_ring_buffer_offset(ctx->pUniformRingBuffer, sizeof(mvp));
			BufferUpdateDesc updateDesc = { uniformBlock.pBuffer, uniformBlock.offset };
			begin_update_resource(&updateDesc);
			*((Matrix4*)updateDesc.pMappedData) = mvp;
			end_update_resource(&updateDesc, nullptr);

			const uint64_t size = sizeof(mvp);
			const uint32_t stride = sizeof(Vec4);

			DescriptorData params[1] = {};
			params[0].name = "uniformBlock_rootcbv";
			params[0].ppBuffers = &uniformBlock.pBuffer;
			params[0].offsets = &uniformBlock.offset;
			params[0].sizes = &size;
			update_descriptor_set(ctx->pRenderer, pipelineIndex, ctx->pDescriptorSets, 1, params);
			cmd_bind_descriptor_set(pCmd, pipelineIndex, ctx->pDescriptorSets);
			cmd_bind_push_constants(pCmd, ctx->pRootSignature, "uRootConstants", &data);
			cmd_bind_vertex_buffer(pCmd, 1, &buffer.pBuffer, &stride, &buffer.offset);
			cmd_draw(pCmd, nverts, 0);
		}
		else
		{
			const uint32_t stride = sizeof(Vec4);
			cmd_bind_descriptor_set(pCmd, pipelineIndex, ctx->pDescriptorSets);
			cmd_bind_push_constants(pCmd, ctx->pRootSignature, "uRootConstants", &data);
			cmd_bind_vertex_buffer(pCmd, 1, &buffer.pBuffer, &stride, &buffer.offset);
			cmd_draw(pCmd, nverts, 0);
		}
	}

	void FontStash_Impl::FonsImplRemoveTexture(void* userPtr)
	{
		UNREF_PARAM(userPtr);
	}

}