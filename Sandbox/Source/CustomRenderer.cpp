#define SG_GRAPHIC_API_VULKAN
#include "Seagull.h"

using namespace SG;

#define IMAGE_COUNT 2

#define COUNT_OF(a) (sizeof(a) / sizeof(a[0]))

class CustomRenderer : public IApp
{
public:
	virtual bool OnInit() override
	{
		// set the file path
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_SHADER_SOURCES, "../../../Resources/Shaders");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_DEBUG,   SG_RD_SHADER_BINARIES, "CompiledShaders");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_GPU_CONFIG, "GPUcfg");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_TEXTURES, "../../../Resources/Textures");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_MESHES, "Meshes");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_FONTS, "Fonts");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_ANIMATIONS, "Animation");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_SCRIPTS, "Scripts");

		return true;
	}

	virtual void OnExit() override
	{
	}

	virtual bool Load() override
	{
		if (!mRenderer || mSettings.resetGraphic)
		{
			RendererCreateDesc rendererCreate = {};
			init_renderer("Seagull Renderer", &rendererCreate, &mRenderer);

			if (!mRenderer)
			{
				SG_LOG_ERROR("Failed to initialize renderer!");
				return false;
			}
			
			QueueCreateDesc queueCreate = {};
			queueCreate.type = SG_QUEUE_TYPE_GRAPHICS;
			//queueCreate.flag = SG_QUEUE_FLAG_INIT_MICROPROFILE;
			add_queue(mRenderer, &queueCreate, &mGraphicQueue);

			for (uint32_t i = 0; i < IMAGE_COUNT; i++)
			{
				CmdPoolCreateDesc cmdPoolCreate = {};
				cmdPoolCreate.pQueue = mGraphicQueue;
				cmdPoolCreate.transient = false;
				add_command_pool(mRenderer, &cmdPoolCreate, &mCmdPools[i]);

				CmdCreateDesc cmdCreate = {};
				cmdCreate.pPool = mCmdPools[i];
				add_cmd(mRenderer, &cmdCreate, &mCmds[i]);

				add_fence(mRenderer, &mRenderCompleteFences[i]);
				add_semaphore(mRenderer, &mRenderCompleteSemaphores[i]);
			}
			add_semaphore(mRenderer, &mImageAcquiredSemaphore);

			init_resource_loader_interface(mRenderer);

			//TextureLoadDesc textureCreate = {};
			//textureCreate.fileName = "Moon";
			//textureCreate.ppTexture = &mTexture;
			//add_resource(&textureCreate, nullptr);

			ShaderLoadDesc loadBasicShader = {};
			loadBasicShader.stages[0] = { "triangle.vert", nullptr, 0, "main" };
			loadBasicShader.stages[1] = { "triangle.frag", nullptr, 0, "main" };
			add_shader(mRenderer, &loadBasicShader, &mTriangleShader);

			//SamplerCreateDesc samplerCreate = {};
			//samplerCreate.addressU = SG_ADDRESS_MODE_CLAMP_TO_BORDER;
			//samplerCreate.addressV = SG_ADDRESS_MODE_CLAMP_TO_BORDER;
			//samplerCreate.addressW = SG_ADDRESS_MODE_CLAMP_TO_BORDER;
			//samplerCreate.minFilter = SG_FILTER_LINEAR;
			//samplerCreate.magFilter = SG_FILTER_LINEAR;
			//samplerCreate.mipMapMode = SG_MIPMAP_MODE_LINEAR;
			//add_sampler(mRenderer, &samplerCreate, &mSampler);

			Shader* submitShaders[] = { mTriangleShader };
			//const char* staticSamplers[] = { "Sampler" };
			RootSignatureCreateDesc rootSignatureCreate = {};
/*			rootSignatureCreate.staticSamplerCount = COUNT_OF(staticSamplers);
			rootSignatureCreate.ppStaticSamplers = &mSampler;
			rootSignatureCreate.ppStaticSamplerNames = staticSamplers;	*/		
			rootSignatureCreate.staticSamplerCount = 0;
			rootSignatureCreate.ppStaticSamplers = nullptr;
			rootSignatureCreate.ppStaticSamplerNames = nullptr;
			rootSignatureCreate.ppShaders = submitShaders;
			rootSignatureCreate.shaderCount = COUNT_OF(submitShaders);
			add_root_signature(mRenderer, &rootSignatureCreate, &mRootSignature);

			//DescriptorSetCreateDesc descriptorSetCreate = {};
			//descriptorSetCreate.pRootSignature = mRootSignature;
			//descriptorSetCreate.updateFrequency = SG_DESCRIPTOR_UPDATE_FREQ_NONE;
			//descriptorSetCreate.maxSets = 1;
			//add_descriptor_set(mRenderer, &descriptorSetCreate, &mDescriptorSet);

			BufferLoadDesc loadVertexBuffer = {};
			loadVertexBuffer.desc.descriptors = SG_DESCRIPTOR_TYPE_VERTEX_BUFFER;
			loadVertexBuffer.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_GPU_ONLY;
			loadVertexBuffer.desc.size = sizeof(mVertices);
			loadVertexBuffer.pData = mVertices;
			loadVertexBuffer.ppBuffer = &mVertexBuffer;
			add_resource(&loadVertexBuffer, nullptr);

			BufferLoadDesc loadIndexBuffer = {};
			loadIndexBuffer.desc.descriptors = SG_DESCRIPTOR_TYPE_INDEX_BUFFER;
			loadIndexBuffer.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_GPU_ONLY;
			loadIndexBuffer.desc.size = sizeof(mIndices);
			loadIndexBuffer.pData = mIndices;
			loadIndexBuffer.ppBuffer = &mIndexBuffer;
			add_resource(&loadIndexBuffer, nullptr);
		}

		if (!CreateSwapChain())
			return false;

		if (!CreateGraphicPipeline())
			return false;

		wait_for_all_resource_loads();

		return true;
	}

	virtual bool Unload() override
	{
		wait_queue_idle(mGraphicQueue);

		remove_pipeline(mRenderer, mPipeline);
		remove_swapchain(mRenderer, mSwapChain);

		if (mSettings.resetGraphic || mSettings.quit)
		{
			//remove_descriptor_set(mRenderer, mDescriptorSet);

			remove_resource(mVertexBuffer);
			remove_resource(mIndexBuffer);
			//remove_resource(mTexture);

			//remove_sampler(mRenderer, mSampler);
			remove_shader(mRenderer, mTriangleShader);
			remove_root_signature(mRenderer, mRootSignature);

			for (uint32_t i = 0; i < IMAGE_COUNT; ++i)
			{
				remove_fence(mRenderer, mRenderCompleteFences[i]);
				remove_semaphore(mRenderer, mRenderCompleteSemaphores[i]);

				remove_cmd(mRenderer, mCmds[i]);
				remove_command_pool(mRenderer, mCmdPools[i]);
			}
			remove_semaphore(mRenderer, mImageAcquiredSemaphore);

			exit_resource_loader_interface(mRenderer);

			remove_queue(mRenderer, mGraphicQueue);
			remove_renderer(mRenderer);
		}

		return true;
	}

	virtual bool OnUpdate(float deltaTime) override
	{
		//SG_LOG_INFO("Frame: %.1f", 1.0f / deltaTime);
		return true;
	}

	virtual bool OnDraw() override
	{
		uint32_t imageIndex;
		acquire_next_image(mRenderer, mSwapChain, mImageAcquiredSemaphore, nullptr, &imageIndex);

		// get the render target from the swapchain
		RenderTarget* renderTarget = mSwapChain->ppRenderTargets[imageIndex];
		Semaphore* renderCompleteSemaphore = mRenderCompleteSemaphores[mCurrentIndex];
		Fence* renderCompleteFence = mRenderCompleteFences[mCurrentIndex];

		FenceStatus fenceStatus;
		get_fence_status(mRenderer, renderCompleteFence, &fenceStatus);
		if (fenceStatus == SG_FENCE_STATUS_INCOMPLETE)
			wait_for_fences(mRenderer, 1, &renderCompleteFence);

		// reset command pool for this frame 
		reset_command_pool(mRenderer, mCmdPools[mCurrentIndex]);
		
		const uint32_t stride = 4 * sizeof(float);

		Cmd* cmd = mCmds[mCurrentIndex];
		// begin command buffer
		begin_cmd(cmd);

		RenderTargetBarrier renderTargetBarriers[2];

		renderTargetBarriers[0] = { renderTarget, SG_RESOURCE_STATE_PRESENT, SG_RESOURCE_STATE_RENDER_TARGET };
		cmd_resource_barrier(cmd, 0, nullptr, 0, nullptr, 1, renderTargetBarriers);

		LoadActionsDesc loadAction = {};
		loadAction.loadActionsColor[0] = SG_LOAD_ACTION_CLEAR;
		loadAction.clearColorValues[0].r = 0.0f;
		loadAction.clearColorValues[0].g = 0.0f;
		loadAction.clearColorValues[0].b = 0.0f;
		loadAction.clearColorValues[0].a = 0.0f;

		// begin render pass
		cmd_bind_render_targets(cmd, 1, &renderTarget, nullptr, &loadAction, nullptr, nullptr, -1, -1);
		cmd_set_viewport(cmd, 0.0f, 0.0f, (float)renderTarget->width, (float)renderTarget->height, 0.0f, 1.0f);
		cmd_set_scissor(cmd, 0, 0, renderTarget->width, renderTarget->height);
		
		cmd_bind_pipeline(cmd, mPipeline);
		//cmd_bind_descriptor_set(cmd, 0, mDescriptorSet);
		cmd_bind_vertex_buffer(cmd, 1, &mVertexBuffer, &stride, nullptr);
		cmd_bind_index_buffer(cmd, mIndexBuffer, SG_INDEX_TYPE_UINT32, 0);

		cmd_draw_indexed(cmd, 3, 0, 0);

		//loadAction = {};
		//loadAction.loadActionsColor[0] = SG_LOAD_ACTION_LOAD;
		//cmd_bind_render_targets(cmd, 1, &renderTarget, nullptr, &loadAction, nullptr, nullptr, -1, -1);

		// end the render pass
		cmd_bind_render_targets(cmd, 0, nullptr, nullptr, nullptr, nullptr, nullptr, -1, -1);

		renderTargetBarriers[0] = { renderTarget, SG_RESOURCE_STATE_RENDER_TARGET, SG_RESOURCE_STATE_PRESENT };
		cmd_resource_barrier(cmd, 0, nullptr, 0, nullptr, 1, renderTargetBarriers);

		end_cmd(cmd);

		QueueSubmitDesc submitDesc = {};
		submitDesc.cmdCount = 1;
		submitDesc.signalSemaphoreCount = 1;
		submitDesc.waitSemaphoreCount = 1;
		submitDesc.ppCmds = &cmd;
		submitDesc.ppSignalSemaphores = &renderCompleteSemaphore;
		submitDesc.ppWaitSemaphores = &mImageAcquiredSemaphore;
		submitDesc.pSignalFence = renderCompleteFence;
		queue_submit(mGraphicQueue, &submitDesc);

		QueuePresentDesc presentDesc = {};
		presentDesc.index = imageIndex;
		presentDesc.waitSemaphoreCount = 1;
		presentDesc.pSwapChain = mSwapChain;
		presentDesc.ppWaitSemaphores = &renderCompleteSemaphore;
		presentDesc.submitDone = true;
		PresentStatus presentStatus = queue_present(mGraphicQueue, &presentDesc);

		if (presentStatus == SG_PRESENT_STATUS_DEVICE_RESET)
		{
			// Wait for a few seconds to allow the driver to come back online before doing a reset.
			Thread::sleep(5000);
			mSettings.resetGraphic = true;
		}

		mCurrentIndex = (mCurrentIndex + 1) % IMAGE_COUNT;

		return true;
	}

	virtual const char* GetName() override
	{
		return "Custom Renderer";
	}
private:
	bool CreateSwapChain()
	{
		SwapChainCreateDesc swapChainCreate = {};
		swapChainCreate.windowHandle = mWindow->handle;
		swapChainCreate.presentQueueCount = 1;
		swapChainCreate.ppPresentQueues = &mGraphicQueue;
		swapChainCreate.width = mSettings.width;
		swapChainCreate.height = mSettings.height;
		swapChainCreate.imageCount = IMAGE_COUNT;
		swapChainCreate.colorFormat = get_recommended_swapchain_format(true);
		swapChainCreate.enableVsync = mSettings.defaultVSyncEnable;
		SG::add_swapchain(mRenderer, &swapChainCreate, &mSwapChain);

		return mSwapChain != nullptr;
	}

	bool CreateGraphicPipeline()
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
		vertexLayout.attribs[1].offset = 2 * sizeof(float);

		RasterizerStateDesc rasterizeState = {};
		rasterizeState.cullMode = SG_CULL_MODE_BACK;
		rasterizeState.frontFace = SG_FRONT_FACE_CW;

		DepthStateDesc depthStateDesc = {};

		PipelineCreateDesc pipelineCreate = {};
		pipelineCreate.type = SG_PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& graphicPipe = pipelineCreate.graphicsDesc;
		graphicPipe.primitiveTopo = SG_PRIMITIVE_TOPO_TRI_LIST;
		graphicPipe.renderTargetCount = 1;
		graphicPipe.pColorFormats = &mSwapChain->ppRenderTargets[0]->format;
		graphicPipe.sampleCount = mSwapChain->ppRenderTargets[0]->sampleCount;
		graphicPipe.sampleQuality = mSwapChain->ppRenderTargets[0]->sampleQuality;

		graphicPipe.pRootSignature = mRootSignature;
		graphicPipe.pShaderProgram = mTriangleShader;

		graphicPipe.pVertexLayout = &vertexLayout;
		//graphicPipe.pVertexLayout = nullptr;
		graphicPipe.pRasterizerState = &rasterizeState;
		graphicPipe.pDepthState = &depthStateDesc;
		add_pipeline(mRenderer, &pipelineCreate, &mPipeline);

		return mPipeline != nullptr;
	}
private:
	Renderer* mRenderer = nullptr;

	Queue* mGraphicQueue = nullptr;

	CmdPool* mCmdPools[IMAGE_COUNT] = { 0 };
	Cmd* mCmds[IMAGE_COUNT] = { 0 };

	SwapChain* mSwapChain = nullptr;
	Fence* mRenderCompleteFences[IMAGE_COUNT] = { 0 };
	Semaphore* mRenderCompleteSemaphores[IMAGE_COUNT] = { 0 };
	Semaphore* mImageAcquiredSemaphore = { 0 };

	Shader* mTriangleShader = nullptr;
	//Sampler* mSampler = nullptr;

	RootSignature* mRootSignature = nullptr;
	//DescriptorSet* mDescriptorSet = nullptr;
	Pipeline* mPipeline = nullptr;

	//Texture* mTexture = nullptr;
	Buffer* mVertexBuffer = nullptr;
	Buffer* mIndexBuffer = nullptr;

	//float mVertices[20] = {
	//	1.0f,  1.0f, 1.0f, 0.0f, // 0 top_right 
	//   -1.0f,  1.0f, 0.0f, 0.0f, // 1 top_left
	//   -1.0f, -1.0f, 0.0f, 1.0f, // 2 bot_left
	//	1.0f, -1.0f, 1.0f, 1.0f, // 3 bot_right
	//};

	float mVertices[12] = {
		0.0, -0.5, 1.0f, 0.0f, // 0 top_right 
		0.5,  0.5, 0.0f, 0.0f, // 1 top_left
		-0.5, 0.5, 0.0f, 1.0f, // 2 bot_left
	};

	const uint32_t mIndices[3] = {
		0, 1, 2,
	};

	uint32_t mCurrentIndex = 0;
};

SG_DEFINE_APPLICATION_MAIN(CustomRenderer)