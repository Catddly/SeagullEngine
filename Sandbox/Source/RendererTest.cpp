//#define SG_GRAPHIC_API_VULKAN
//#include "Seagull.h"
//
//#include "../../Seagull-Core/Renderer/IRenderer/Include/IRenderer.h"
//
//using namespace SG;
//
//struct Vertex
//{
//	Vec3 position;
//	Vec4 color;
//};
//
//class CustomRenderer : public IApp
//{
//	virtual bool OnInit() override
//	{
//		RendererCreateDesc desc{};
//		desc.api = SG_RENDERER_API_VULKAN;
//		desc.gpuMode = SG_GPU_MODE_SINGLE;
//		desc.pLogFn = CustomLogFunc;
//		desc.shaderTarget = SG_SHADER_TARGET_6_0;
//
//		desc.ppInstanceLayers = nullptr;
//		desc.instanceLayerCount = 0;
//		desc.ppDeviceExtensions = const_cast<const char**>(mRequiredDeviceExtensions.data());
//		desc.deviceExtensionCount = static_cast<uint32_t>(mRequiredDeviceExtensions.size());
//		desc.ppInstanceExtensions = nullptr;
//		desc.instanceExtensionCount = 0;
//
//		desc.requestAllAvailableQueues = false;
//
//		init_renderer("CustomRenderer", &desc, &mRenderer);
//
//		SG_LOG_INFO("");
//		SG_LOG_INFO("");
//		SG_LOG_INFO("Renderer Iniialized successfully!");
//
//		QueueCreateDesc queueCreate;
//		queueCreate.nodeIndex = 0;
//		queueCreate.type = SG_QUEUE_TYPE_GRAPHICS;
//		queueCreate.priority = SG_QUEUE_PRIORITY_HIGH;
//		add_queue(mRenderer, &queueCreate, &mGraphicQueue);
//		SG_LOG_INFO("Present Queue created successfully!");
//
//		SwapChainCreateDesc swapChainCreate;
//		swapChainCreate.width = mSettings.width;
//		swapChainCreate.height = mSettings.height;
//		swapChainCreate.colorClearValue = { 0.0f, 0.0f, 0.0f, 1.0f };
//		swapChainCreate.colorFormat = get_recommended_swapchain_format(false);
//		swapChainCreate.enableVsync = false;
//		swapChainCreate.imageCount = 2;
//		swapChainCreate.useFlipSwapEffect = false;
//		swapChainCreate.windowHandle = mWindow->handle;
//		swapChainCreate.ppPresentQueues = &mGraphicQueue;
//		swapChainCreate.presentQueueCount = 1;
//
//		add_swapchain(mRenderer, &swapChainCreate, &mSwapChain);
//		SG_LOG_INFO("Swapchain created successfully!");
//
//		for (int i = 0; i < 2; i++)
//		{
//			add_fence(mRenderer, &mInFlightFences[i]);
//			add_fence(mRenderer, &mImagesInFlight[i]);
//			add_semaphore(mRenderer, &mImageAvailableSemaphore[i]);
//			add_semaphore(mRenderer, &mRenderFinishedSemaphore[i]);
//		}
//		SG_LOG_INFO("Fences and semaphores created successfully!");
//
//		CmdPoolCreateDesc cmdPoolCreate;
//		cmdPoolCreate.pQueue = mGraphicQueue;
//		cmdPoolCreate.transient = false;
//		add_command_pool(mRenderer, &cmdPoolCreate, &mCommandPool);
//		SG_LOG_INFO("Command pool created successfully!");
//	
//		CmdCreateDesc cmdDesc;
//		cmdDesc.pPool = mCommandPool;
//		cmdDesc.secondary = false;
//		add_cmd(mRenderer, &cmdDesc, &mCommandBuffer);
//		SG_LOG_INFO("Command buffer created successfully!");
//		
//		char* entryName = "main";
//		mVertexShader->pEntryNames = &entryName;
//		mVertexShader->pReflection = nullptr;
//		mVertexShader->stages = SG_SHADER_STAGE_VERT;
//		//mVertexShader->pShaderModules = 
//
//		BinaryShaderCreateDesc binaryShaderCreateDesc;
//		BinaryShaderStageDesc vertexShaderStage;
//		BinaryShaderStageDesc fragShaderStage;
//		binaryShaderCreateDesc.vert = vertexShaderStage;
//		binaryShaderCreateDesc.frag = fragShaderStage;
//		binaryShaderCreateDesc.stages = SG_SHADER_STAGE_VERT | SG_SHADER_STAGE_FRAG;
//		add_shader_binary(mRenderer, &binaryShaderCreateDesc, mShaders);
//
//		RootSignatureCreateDesc rootSignatureCreateDesc;
//		rootSignatureCreateDesc.shaderCount = 2;
//		rootSignatureCreateDesc.ppShaders = mShaders;
//		rootSignatureCreateDesc.maxBindlessTextures = 64;
//		rootSignatureCreateDesc.staticSamplerCount = 0;
//		rootSignatureCreateDesc.ppStaticSamplers = nullptr;
//		add_root_signature(mRenderer, &rootSignatureCreateDesc, &mRootSignature);
//
//		PipelineCreateDesc pipelineCreate;
//		GraphicsPipelineDesc graphicPipelineDesc;
//		graphicPipelineDesc.renderTargetCount = 2;
//		graphicPipelineDesc.sampleCount = SG_SAMPLE_COUNT_1;
//		graphicPipelineDesc.supportIndirectCommandBuffer = false;
//		graphicPipelineDesc.primitiveTopo = SG_PRIMITIVE_TOPO_TRI_LIST;
//		graphicPipelineDesc.pColorFormats = &mSwapChain->pDesc->colorFormat;
//		graphicPipelineDesc.pBlendState = nullptr;
//		graphicPipelineDesc.pDepthState = nullptr;
//		graphicPipelineDesc.pRootSignature = mRootSignature;
//		graphicPipelineDesc.pShaderProgram = nullptr;
//
//		VertexLayout vertexLayout;
//		VertexAttrib vertexAttrib;
//		vertexAttrib.binding = 0;
//		vertexAttrib.location = 0;
//		vertexAttrib.format = TinyImageFormat_R32_UINT;
//		vertexAttrib.offset = 0;
//		vertexAttrib.rate = SG_VERTEX_ATTRIB_RATE_VERTEX;
//		vertexAttrib.semantic = SG_SEMANTIC_POSITION;
//		char* name = vertexAttrib.semanticName;
//		name = "vertex";
//		vertexAttrib.semanticNameLength = 7;
//
//		vertexLayout.attribs[0] = vertexAttrib;
//		vertexLayout.attribCount = 1;
//		graphicPipelineDesc.pVertexLayout = &vertexLayout;
//
//		pipelineCreate.graphicsDesc = graphicPipelineDesc;
//		pipelineCreate.type = SG_PIPELINE_TYPE_GRAPHICS;
//		pipelineCreate.name = "Test Pipeline";
//		pipelineCreate.pPipelineExtensions = nullptr;
//		pipelineCreate.extensionCount = 0;
//		pipelineCreate.pCache = nullptr;
//		add_pipeline(mRenderer, &pipelineCreate, &mPipeline);
//
//		RasterizerStateDesc rasterizerStateDesc;
//		rasterizerStateDesc.cullMode = SG_CULL_MODE_BACK;
//		rasterizerStateDesc.fillMode = SG_FILL_MODE_SOLID;
//		rasterizerStateDesc.frontFace = SG_FRONT_FACE_CCW;
//		rasterizerStateDesc.multiSample = false;
//		rasterizerStateDesc.depthClampEnable = true;
//		rasterizerStateDesc.scissor = true;
//		graphicPipelineDesc.pRasterizerState = &rasterizerStateDesc;
//
//		begin_cmd(mCommandBuffer);
//
//		LoadActionsDesc loadAction;
//		loadAction.loadActionDepth = SG_LOAD_ACTION_DONTCARE;
//		loadAction.loadActionsColor[0] = SG_LOAD_ACTION_CLEAR;
//		loadAction.loadActionsColor[1] = SG_LOAD_ACTION_CLEAR;
//		loadAction.loadActionStencil = SG_LOAD_ACTION_DONTCARE;
//		loadAction.clearDepth.depth = 1.0;
//		loadAction.clearColorValues[0] = { 0.0f, 0.0f, 0.0f, 1.0f };
//		loadAction.clearColorValues[1] = { 0.0f, 0.0f, 0.0f, 1.0f };
//
//		cmd_bind_render_targets(mCommandBuffer, 2, mSwapChain->ppRenderTargets, nullptr, &loadAction, nullptr, nullptr, 0, 0);
//
//		cmd_bind_pipeline(mCommandBuffer, mPipeline);
//		cmd_bind_vertex_buffer(mCommandBuffer, 1, );
//
//		end_cmd(mCommandBuffer);
//
//		return true;
//	}
//
//	virtual void OnExit() override
//	{
//		remove_cmd(mRenderer, mCommandBuffer);
//		SG_LOG_INFO("Command buffer destroyed successfully!");
//		remove_command_pool(mRenderer, mCommandPool);
//		SG_LOG_INFO("Command pool destroyed successfully!");
//
//		for (int i = 0; i < 2; i++)
//		{
//			remove_fence(mRenderer, mInFlightFences[i]);
//			remove_fence(mRenderer, mImagesInFlight[i]);
//			remove_semaphore(mRenderer, mImageAvailableSemaphore[i]);
//			remove_semaphore(mRenderer, mRenderFinishedSemaphore[i]);
//		}
//		SG_LOG_INFO("Fences destroyed successfully!");
//		remove_swapchain(mRenderer, mSwapChain);
//		SG_LOG_INFO("Swapchain destroyed successfully!");
//		remove_queue(mRenderer, mGraphicQueue);
//		SG_LOG_INFO("Present queue destroyed successfully!");
//		remove_renderer(mRenderer);
//		SG_LOG_INFO("Renderer destroyed successfully!");
//	}
//
//	virtual bool Load() override
//	{
//		return true;
//	}
//
//	virtual bool Unload() override
//	{
//		return true;
//	}
//
//	virtual bool OnUpdate(float deltaTime) override
//	{
//		//SG_LOG_INFO("Frame: %.1f", 1.0f / deltaTime);
//		return true;
//	}
//
//	virtual bool OnDraw() override
//	{
//		// wait until the submit queue has empty slot
//		wait_for_fences(mRenderer, 1, &mInFlightFences[mCurrentFrame]);
//
//		uint32_t nextImageIndex;
//		acquire_next_image(mRenderer, mSwapChain, mImageAvailableSemaphore[mCurrentFrame],
//			nullptr, &nextImageIndex);
//
//		// if previous frame is using this image
//		wait_for_fences(mRenderer, 1, &mImagesInFlight[nextImageIndex]);
//
//		// mark the image as now being in use by this frame
//		mImagesInFlight[nextImageIndex] = mInFlightFences[mCurrentFrame];
//		
//		QueueSubmitDesc submit;
//		submit.waitSemaphoreCount = 1;
//		submit.ppWaitSemaphores = &mImageAvailableSemaphore[mCurrentFrame];
//		submit.signalSemaphoreCount = 1;
//		submit.ppSignalSemaphores = &mRenderFinishedSemaphore[mCurrentFrame];
//		//queue_submit(mGraphicQueue, );
//
//		mCurrentFrame = (mCurrentFrame + 1) % 2;
//
//		return true;
//	}
//
//	virtual const char* GetName() override
//	{
//		return "CustomRenderer";
//	}
//
//private:
//	static void CustomLogFunc(LogType type, const char* t, const char* msg)
//	{
//		SG_LOG(type, "%s :(%s)", t, msg);
//	}
//private:
//	Renderer* mRenderer = nullptr;
//
//	Fence* mInFlightFences[2] = { nullptr, nullptr };
//	Fence* mImagesInFlight[2] = { nullptr, nullptr };
//	Semaphore* mImageAvailableSemaphore[2] = { nullptr, nullptr };
//	Semaphore* mRenderFinishedSemaphore[2] = { nullptr, nullptr };
//
//	SwapChain* mSwapChain = nullptr;
//
//	Queue* mGraphicQueue;
//
//	Shader* mVertexShader;
//	Shader* mFragShader;
//	Shader* mShaders[2] = { mVertexShader, mFragShader };
//	RootSignature* mRootSignature;
//	Pipeline* mPipeline;
//
//	CmdPool* mCommandPool;
//	Cmd* mCommandBuffer;
//
//	Buffer* mVertexBuffer;
//
//	uint32_t mCurrentFrame = 0;
//
//	const eastl::vector<const char*> mRequiredDeviceExtensions = {
//		"VK_KHR_swapchain"
//	};
//};
//
//SG_DEFINE_APPLICATION_MAIN(CustomRenderer)