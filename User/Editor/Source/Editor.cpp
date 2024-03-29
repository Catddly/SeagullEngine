#include "Seagull.h"

using namespace SG;

#define IMAGE_COUNT 2

#define COUNT_OF(a) (sizeof(a) / sizeof(a[0]))

struct Vertex
{
	Vec3 Position;
	Vec2 TexCoord;

	static float GetStructSize()
	{
		return sizeof(Vertex);
	}
};

struct UniformBuffer
{
	Matrix4 model;
	Matrix4 view;
	Matrix4 projection;
};

char* ParserPairsToStr(const eastl::vector<eastl::pair<uint32_t, uint32_t>>& vec)
{
	char strs[400];
	memset(strs, 0, sizeof(strs));
	for (uint32_t i = 0; i < vec.size(); i++)
	{
		char widthStr[20];
		char heightStr[10];
		sprintf(widthStr, "%d", vec[i].first);
		sprintf(heightStr, "%d", vec[i].second);
		strcat(widthStr, " x ");
		strcat(widthStr, heightStr);
		strcpy(strs, widthStr);
		SG_LOG_DEBUG("%s", strs[i]);
	}
	
	for (uint32_t i = 0; i < vec.size(); i++)
	{
		SG_LOG_DEBUG("%s", strs[i]);
	}
	
	return strs;
}

Vec2 gSliderData = {};

eastl::vector<eastl::pair<uint32_t, uint32_t>> windowSizesPair = {
	{ 2560, 1440 },
	{ 2176, 1224 },
	{ 1920, 1080 },
	{ 1440, 1080 },
	{ 1440, 720 },
	{ 1280, 720 },
	{ 640, 320 }
};

const char* windowSizesName[] = {
	{ "2560 x 1440" },
	{ "2176 x 1224" },
	{ "1920 x 1080" },
	{ "1440 x 1080" },
	{ "1440 x 720" },
	{ "1280 x 720" },
	{ "640 x 320" }
};

uint32_t windowSizeSelect = 2;
uint32_t windowSizeValue;

bool isEdited = false;
void EditorCallback()
{
	isEdited = true;
}

ICameraController* gCamera = nullptr;
Vec2			   gLastMousePos;

bool        gIsStopPressed = false;
bool        gStopRotating = false;
float       gFps = 0.0;

class CustomRenderer : public IApp
{
public:
	virtual bool OnInit() override
	{
		// set the file path
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_SHADER_SOURCES, "../../../Resources/Shaders");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_DEBUG, SG_RD_SHADER_BINARIES, "../../../Resources/CompiledShaders");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_GPU_CONFIG, "GPUfg");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_TEXTURES, "../../../Resources/Textures");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_MESHES, "../../../Resources/Meshes");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_FONTS, "../../../Resources/Fonts");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_ANIMATIONS, "Animation");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_SCRIPTS, "Scripts");

		//sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_SHADER_SOURCES, "Resources/Shaders");
		//sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_DEBUG, SG_RD_SHADER_BINARIES, "Resources/CompiledShaders");
		//sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_GPU_CONFIG, "GPUfg");
		//sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_TEXTURES, "Resources/Textures");
		//sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_MESHES, "Resources/Meshes");
		//sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_FONTS, "Resources/Fonts");
		//sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_ANIMATIONS, "Animation");
		//sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_SCRIPTS, "Scripts");

		//SG_LOG_DEBUG("Main thread ID: %ul", Thread::get_curr_thread_id());

		if (!CreateRenderResource())
			return false;

		if (!mUiMiddleware.OnInit(mRenderer, true))
			return false;

		mUiMiddleware.LoadFont("Source_Sans_Pro/SourceSansPro-Regular.ttf");
		mUiMiddleware.LoadFont("Source_Sans_Pro/SourceSansPro-Bold.ttf");
		TextDrawDesc UITextDesc = { 0, 0xffff00ff, 18.0f };

		float dpiScale = get_dpi_scale().x;
		GuiCreateDesc guiDesc{};
		guiDesc.startPosition = { 0 , 0 };
		guiDesc.startSize = { mSettings.width * 0.5 , mSettings.height * 0.5 };
		guiDesc.defaultTextDrawDesc = UITextDesc;

		mMainGui = mUiMiddleware.AddGuiComponent("TestWindow", &guiDesc);
		mMainGui->flags ^= SG_GUI_FLAGS_ALWAYS_AUTO_RESIZE;
		mMainGui->AddWidget(LabelWidget("TestWindow1"));
		mMainGui->AddWidget(FloatLabelWidget("Fps: %.2f", &gFps));
		mMainGui->AddWidget(ImageWidget("Logo", (void*)mLogoTex, { 256, 256 }));

		mSecondGui = mUiMiddleware.AddGuiComponent("Settings", &guiDesc);
		mSecondGui->flags ^= SG_GUI_FLAGS_ALWAYS_AUTO_RESIZE;
		mSecondGui->AddWidget(SliderFloat2Widget("Slider", &gSliderData, { 0 , 0 }, { 5 , 5 }))->pOnEdited = []
		{
			SG_LOG_DEBUG("Value: (%f, %f))", gSliderData.x, gSliderData.y);
		};
		mSecondGui->AddWidget(ButtonWidget("Stop Rotate", &gIsStopPressed));

		//windowSizesName = ParserPairsToStr(windowSizesPair);
		mSecondGui->AddWidget(DropdownWidget("WindowSize", &windowSizeSelect, windowSizesName, &windowSizeValue, windowSizesPair.size()))->pOnEdited = EditorCallback;

		mViewportGui = mUiMiddleware.AddGuiComponent("Viewport", &guiDesc);
		mViewportGui->flags ^= SG_GUI_FLAGS_ALWAYS_AUTO_RESIZE;
		mViewportGui->flags |= SG_GUI_FLAGS_NO_TITLE_BAR;
		mViewportWidget = mViewportGui->AddWidget(ViewportWidget("Viewport", { mSettings.width, mSettings.height }));

		//mUiMiddleware.mShowDemoUiWindow = true;

		gCamera = create_perspective_camera({ -3.0f, 0.0f, 0.4f }, { 1.0f, 0.0f, 0.0f });

		init_input_system(mWindow);

		InputActionDesc inputAction = { SG_KEY_ESCAPE,
			[](InputActionContext* ctx)
				{
					request_shutdown();
					return true;
				},
			nullptr
		};
		add_input_action(&inputAction);

		inputAction =
		{
			SG_BUTTON_ANY, [](InputActionContext* ctx)
			{
				auto* uiMiddleware = (UIMiddleware*)ctx->pUserData;
				bool capture = uiMiddleware->OnButton(ctx->bindings, ctx->isPressed, ctx->pPosition);
				//set_enable_capture_input(capture && INPUT_ACTION_PHASE_CANCELED != ctx->phase);
				//SG_LOG_DEBUG("button %d (%d)", ctx->bindings, (int)ctx->isPressed);
				//if (ctx->pPosition)
				//	SG_LOG_DEBUG("mouse pos: (%f, %f)", ctx->pPosition->x, ctx->pPosition->y);
				//SG_LOG_DEBUG("scroll value (%f)", ctx->scrollValue);
				//if (ctx->pCaptured)
				//	SG_LOG_DEBUG("is captured (%d)", (int)ctx->pCaptured);
				//SG_LOG_DEBUG("UI window is captured (%d)", (int)uiMiddleware->IsFocused());
				return true;
			},
			&mUiMiddleware
		};
		add_input_action(&inputAction);

		inputAction = { SG_TEXT, 
			[](InputActionContext* ctx) 
			{ 
				auto* uiMiddleware = (UIMiddleware*)ctx->pUserData;
				uiMiddleware->OnText(ctx->text);
				return true;
			}, 
			&mUiMiddleware
		};
		add_input_action(&inputAction);

		RegisterCameraControls();

		return true;
	}

	virtual void OnExit() override
	{
		wait_queue_idle(mGraphicQueue);

		destroy_camera(gCamera);
		exit_input_system();

		mUiMiddleware.OnExit();

		RemoveRenderResource();
	}

	virtual bool OnLoad() override
	{
		DescriptorSetCreateDesc descriptorSetCreate = { mPbrRootSignature, SG_DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		add_descriptor_set(mRenderer, &descriptorSetCreate, &mDescriptorSet);
		descriptorSetCreate = { mPbrRootSignature, SG_DESCRIPTOR_UPDATE_FREQ_PER_FRAME, IMAGE_COUNT };
		add_descriptor_set(mRenderer, &descriptorSetCreate, &mModelUboDescSet);

		if (!CreateSwapChain())
			return false;

		if (!mUiMiddleware.OnLoad(mSwapChain->ppRenderTargets))
			return false;

		if (!CreateRts())
			return false;

		if (!CreateDepthBuffer())
			return false;

		wait_for_all_resource_loads();

		if (!CreateGraphicPipeline())
			return false;

		DescriptorData updateData[2] = {};
		updateData[0].name = "Texture";
		updateData[0].ppTextures = &mTexture;
		update_descriptor_set(mRenderer, 0, mDescriptorSet, 1, updateData); // update the descriptor sets use 

		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			DescriptorData bufferUpdate[1] = {};
			bufferUpdate[0].name = "ubo";
			bufferUpdate[0].ppBuffers = &mRoomUniformBuffer[i];
			update_descriptor_set(mRenderer, i, mModelUboDescSet, 1, bufferUpdate);
		}

		return true;
	}

	virtual bool OnUnload() override
	{
		wait_queue_idle(mGraphicQueue);

		mUiMiddleware.OnUnload();

		remove_render_target(mRenderer, mRt);
		remove_render_target(mRenderer, mDepthBuffer);

		remove_pipeline(mRenderer, mPbrPipeline);
		remove_swapchain(mRenderer, mSwapChain);

		remove_descriptor_set(mRenderer, mDescriptorSet);
		remove_descriptor_set(mRenderer, mModelUboDescSet);

		return true;
	}

	virtual bool OnUpdate(float deltaTime) override
	{
		static float time = 0.0f;
		time += deltaTime;

		if (gIsStopPressed)
			gStopRotating = !gStopRotating;

		update_input_system(mSettings.width, mSettings.height);

		gCamera->SetCameraLens(glm::radians(45.0f), reinterpret_cast<ViewportWidget*>(mViewportWidget)->GetViewportFOV(), 0.001f, 1000.0f);
		gCamera->OnUpdate(deltaTime);
		gLastMousePos = get_mouse_pos_relative(mWindow);

		static float rotateTime = 0.0f;
		if (!gStopRotating)
			rotateTime += deltaTime;

		if (time >= 0.2f)
		{
			gFps = 1.0f / deltaTime;
			time = 0.0f;
		}
		
		mModelData.model = glm::rotate(Matrix4(1.0f), rotateTime * 0.03f * 60.0f, { 0, 0, 1 });
		mModelData.view = gCamera->GetViewMatrix();
		mModelData.projection = gCamera->GetProjMatrix();

		mUiMiddleware.OnUpdate(deltaTime);

		if (isEdited)
		{
			if (mDelayDrawCount == 1)
				set_window_size(mWindow, windowSizesPair[windowSizeSelect].first, windowSizesPair[windowSizeSelect].second);
			mStopUpdate = true;
		}

		// update the viewport rt
		if (!mStopUpdate)
			reinterpret_cast<ViewportWidget*>(mViewportWidget)->BindRenderTexture(mRt->pTexture);
		else
		{
			reinterpret_cast<ViewportWidget*>(mViewportWidget)->BindRenderTexture(mLogoTex);
			// delay for viewport to update
			if (mDelayDrawCount == 0)
				mDelayDrawCount += 2;
		}

		return true;
	}

	virtual bool OnDraw() override
	{
		if (mDelayDrawCount > 0)
			--mDelayDrawCount;

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
		
		BufferUpdateDesc uboUpdate = {};
		uboUpdate.pBuffer = mRoomUniformBuffer[mCurrentIndex];
		begin_update_resource(&uboUpdate);
		*(UniformBuffer*)uboUpdate.pMappedData = mModelData;
		end_update_resource(&uboUpdate, nullptr);

		const uint32_t stride = (uint32_t)Vertex::GetStructSize();
		Cmd* cmd = mCmds[mCurrentIndex];
		// begin command buffer
		begin_cmd(cmd);

		RenderTargetBarrier renderTargetBarriers;

		LoadActionsDesc loadAction = {};
		loadAction.loadActionsColor[0] = SG_LOAD_ACTION_CLEAR;
		loadAction.clearColorValues[0].r = 0.0f;
		loadAction.clearColorValues[0].g = 0.0f;
		loadAction.clearColorValues[0].b = 0.0f;
		loadAction.clearColorValues[0].a = 1.0f;

		loadAction.loadActionDepth = SG_LOAD_ACTION_CLEAR;
		loadAction.clearDepth.depth = 1.0f;
		loadAction.clearDepth.stencil = 0;

		if (!mStopUpdate)
		{
			renderTargetBarriers = { mRt, SG_RESOURCE_STATE_SHADER_RESOURCE, SG_RESOURCE_STATE_RENDER_TARGET };
			cmd_resource_barrier(cmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarriers);

			// begin render pass
			cmd_bind_render_targets(cmd, 1, &mRt, mDepthBuffer, &loadAction, nullptr, nullptr, -1, -1);
				cmd_set_viewport(cmd, 0.0f, 0.0f, (float)renderTarget->width, (float)renderTarget->height, 0.0f, 1.0f);
				cmd_set_scissor(cmd, 0, 0, renderTarget->width, renderTarget->height);

				cmd_bind_pipeline(cmd, mPbrPipeline);
				cmd_bind_descriptor_set(cmd, 0, mDescriptorSet);
				cmd_bind_descriptor_set(cmd, mCurrentIndex, mModelUboDescSet);

				cmd_bind_index_buffer(cmd, mModelGeo->pIndexBuffer, mModelGeo->indexType, 0);
				Buffer* vertexBuffer[] = { mModelGeo->pVertexBuffers[0] };
				cmd_bind_vertex_buffer(cmd, 1, vertexBuffer, mModelGeo->vertexStrides, nullptr);

				for (uint32_t i = 0; i < mModelGeo->drawArgCount; i++)
				{
					IndirectDrawIndexArguments& cmdDraw = mModelGeo->pDrawArgs[i];
					cmd_draw_indexed(cmd, cmdDraw.indexCount, cmdDraw.startIndex, cmdDraw.vertexOffset);
				}
			cmd_bind_render_targets(cmd, 0, nullptr, 0, nullptr, nullptr, nullptr, -1, -1);

			renderTargetBarriers = { mRt, SG_RESOURCE_STATE_RENDER_TARGET, SG_RESOURCE_STATE_SHADER_RESOURCE };
			cmd_resource_barrier(cmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarriers);
		}

		renderTargetBarriers = { renderTarget, SG_RESOURCE_STATE_PRESENT, SG_RESOURCE_STATE_RENDER_TARGET };
		cmd_resource_barrier(cmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarriers);

		cmd_bind_render_targets(cmd, 1, &renderTarget, nullptr, &loadAction, nullptr, nullptr, -1, -1);
			cmd_set_viewport(cmd, 0.0f, 0.0f, (float)renderTarget->width, (float)renderTarget->height, 0.0f, 1.0f);
			cmd_set_scissor(cmd, 0, 0, renderTarget->width, renderTarget->height);

			mUiMiddleware.AddUpdateGui(mMainGui);
			mUiMiddleware.AddUpdateGui(mSecondGui);
			mUiMiddleware.AddUpdateGui(mViewportGui);
			mUiMiddleware.OnDraw(cmd);
		cmd_bind_render_targets(cmd, 0, nullptr, nullptr, nullptr, nullptr, nullptr, -1, -1);

		renderTargetBarriers = { renderTarget, SG_RESOURCE_STATE_RENDER_TARGET, SG_RESOURCE_STATE_PRESENT };
		cmd_resource_barrier(cmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarriers);

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

		if (mDelayDrawCount == 0)
		{
			mStopUpdate = false;
			isEdited = false;
		}

		return true;
	}

	virtual const char* GetName() override
	{
		return "Seagull Engine";
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
		add_swapchain(mRenderer, &swapChainCreate, &mSwapChain);

		return mSwapChain != nullptr;
	}

	bool CreateGraphicPipeline()
	{
		VertexLayout vertexLayout = {};
		vertexLayout.attribCount = 2;
		vertexLayout.attribs[0].semantic = SG_SEMANTIC_POSITION;
		vertexLayout.attribs[0].format = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayout.attribs[0].binding = 0;
		vertexLayout.attribs[0].location = 0;
		vertexLayout.attribs[0].offset = 0;

		vertexLayout.attribs[1].semantic = SG_SEMANTIC_TEXCOORD0;
		vertexLayout.attribs[1].format = TinyImageFormat_R32G32_SFLOAT;
		vertexLayout.attribs[1].binding = 0;
		vertexLayout.attribs[1].location = 1;
		vertexLayout.attribs[1].offset = 3 * sizeof(float);

		RasterizerStateDesc rasterizeState = {};
		rasterizeState.cullMode = SG_CULL_MODE_BACK;
		rasterizeState.frontFace = SG_FRONT_FACE_CCW;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.depthTest = true;
		depthStateDesc.depthWrite = true;
		depthStateDesc.depthFunc = SG_COMPARE_MODE_LESS;

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.srcFactors[0] = SG_BLEND_CONST_ONE;
		blendStateDesc.srcFactors[1] = SG_BLEND_CONST_ONE;
		blendStateDesc.srcAlphaFactors[0] = SG_BLEND_CONST_ONE;
		blendStateDesc.srcAlphaFactors[1] = SG_BLEND_CONST_ONE;
		blendStateDesc.renderTargetMask = SG_BLEND_STATE_TARGET_0;
		blendStateDesc.masks[0] = SG_BLEND_COLOR_MASK_ALL;
		blendStateDesc.masks[1] = SG_BLEND_COLOR_MASK_ALL;

		PipelineCreateDesc pipelineCreate = {};
		pipelineCreate.type = SG_PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& graphicPipe = pipelineCreate.graphicsDesc;
		graphicPipe.primitiveTopo = SG_PRIMITIVE_TOPO_TRI_LIST;
		graphicPipe.renderTargetCount = 1;

		graphicPipe.pColorFormats = &mSwapChain->ppRenderTargets[0]->format;
		graphicPipe.pDepthState = &depthStateDesc;

		graphicPipe.sampleCount = mSwapChain->ppRenderTargets[0]->sampleCount;
		graphicPipe.sampleQuality = mSwapChain->ppRenderTargets[0]->sampleQuality;

		graphicPipe.pRootSignature = mPbrRootSignature;
		graphicPipe.pShaderProgram = mPbrShader;

		graphicPipe.pVertexLayout = &vertexLayout;
		graphicPipe.pRasterizerState = &rasterizeState;

		graphicPipe.depthStencilFormat = mDepthBuffer->format;

		graphicPipe.pBlendState = &blendStateDesc;
		add_pipeline(mRenderer, &pipelineCreate, &mPbrPipeline);

		return mPbrPipeline != nullptr;
	}

	bool CreateDepthBuffer()
	{
		RenderTargetCreateDesc depthRT = {};
		depthRT.arraySize = 1;
		depthRT.clearValue = { { 1.0f, 0 } };
		depthRT.depth = 1;
		depthRT.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE;
		depthRT.format = TinyImageFormat_D24_UNORM_S8_UINT;
		depthRT.startState = SG_RESOURCE_STATE_SHADER_RESOURCE;
		depthRT.height = mSettings.height;
		depthRT.width = mSettings.width;
		depthRT.sampleCount = SG_SAMPLE_COUNT_1;
		depthRT.sampleQuality = 0;
		depthRT.name = "DepthBuffer";

		add_render_target(mRenderer, &depthRT, &mDepthBuffer);
		return mDepthBuffer != nullptr;
	}

	bool CreateRts()
	{
		RenderTargetCreateDesc rtDesc = {};
		rtDesc.format = mSwapChain->ppRenderTargets[0]->format;
		rtDesc.width = mSettings.width;
		rtDesc.height = mSettings.height;
		rtDesc.clearValue.r = 0.0f;
		rtDesc.clearValue.g = 0.0f;
		rtDesc.clearValue.b = 0.0f;
		rtDesc.clearValue.a = 1.0f;
		rtDesc.sampleCount = SG_SAMPLE_COUNT_1;
		rtDesc.arraySize = 1;
		rtDesc.depth = 1;
		rtDesc.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE;
		rtDesc.sampleQuality = 0;
		rtDesc.startState = SG_RESOURCE_STATE_SHADER_RESOURCE;
		rtDesc.mipLevels = 0;
			
		add_render_target(mRenderer, &rtDesc, &mRt);
		return mRt != nullptr;
	}

	bool CreateRenderResource()
	{
		RendererCreateDesc rendererCreate = {};
		rendererCreate.shaderTarget = SG_SHADER_TARGET_6_3;
		init_renderer("Seagull Renderer", &rendererCreate, &mRenderer);

		if (!mRenderer)
		{
			SG_LOG_ERROR("Failed to initialize renderer!");
			return false;
		}

		QueueCreateDesc queueCreate = {};
		queueCreate.type = SG_QUEUE_TYPE_GRAPHICS;
		queueCreate.flag = SG_QUEUE_FLAG_INIT_MICROPROFILE;
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

		TextureLoadDesc textureCreate = {};
		textureCreate.fileName = "viking_room";
		textureCreate.ppTexture = &mTexture;
		add_resource(&textureCreate, nullptr);

		textureCreate.fileName = "logo";
		textureCreate.ppTexture = &mLogoTex;
		add_resource(&textureCreate, nullptr);

		mRoomGeoVertexLayout.attribCount = 2;

		mRoomGeoVertexLayout.attribs[0].semantic = SG_SEMANTIC_POSITION;
		mRoomGeoVertexLayout.attribs[0].format = TinyImageFormat_R32G32B32_SFLOAT;
		mRoomGeoVertexLayout.attribs[0].binding = 0;
		mRoomGeoVertexLayout.attribs[0].location = 0;
		mRoomGeoVertexLayout.attribs[0].offset = 0;

		mRoomGeoVertexLayout.attribs[1].semantic = SG_SEMANTIC_TEXCOORD0;
		mRoomGeoVertexLayout.attribs[1].format = TinyImageFormat_R32G32_SFLOAT;
		mRoomGeoVertexLayout.attribs[1].binding = 0;
		mRoomGeoVertexLayout.attribs[1].location = 1;
		mRoomGeoVertexLayout.attribs[1].offset = 3 * sizeof(float);

		GeometryLoadDesc geoCreate = {};
		geoCreate.fileName = "model.gltf";
		geoCreate.ppGeometry = &mModelGeo;
		geoCreate.pVertexLayout = &mRoomGeoVertexLayout;
		add_resource(&geoCreate, nullptr);

		ShaderLoadDesc loadBasicShader = {};
		loadBasicShader.stages[0] = { "triangle.vert", nullptr, 0, "main" };
		loadBasicShader.stages[1] = { "triangle.frag", nullptr, 0, "main" };
		add_shader(mRenderer, &loadBasicShader, &mPbrShader);

		SamplerCreateDesc samplerCreate = {};
		samplerCreate.addressU = SG_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCreate.addressV = SG_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCreate.addressW = SG_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCreate.minFilter = SG_FILTER_LINEAR;
		samplerCreate.magFilter = SG_FILTER_LINEAR;
		samplerCreate.mipMapMode = SG_MIPMAP_MODE_LINEAR;
		add_sampler(mRenderer, &samplerCreate, &mSampler);

		Shader* submitShaders[] = { mPbrShader };
		const char* staticSamplers[] = { "Sampler" };
		RootSignatureCreateDesc rootSignatureCreate = {};
		rootSignatureCreate.staticSamplerCount = COUNT_OF(staticSamplers);
		rootSignatureCreate.ppStaticSamplers = &mSampler;
		rootSignatureCreate.ppStaticSamplerNames = staticSamplers;
		rootSignatureCreate.ppShaders = submitShaders;
		rootSignatureCreate.shaderCount = COUNT_OF(submitShaders);
		add_root_signature(mRenderer, &rootSignatureCreate, &mPbrRootSignature);

		BufferLoadDesc uboCreate;
		uboCreate.desc.descriptors = SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboCreate.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		uboCreate.desc.name = "UniformBuffer";
		uboCreate.desc.size = sizeof(UniformBuffer);
		uboCreate.desc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; // we need to update it every frame
		uboCreate.pData = nullptr;
		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			uboCreate.ppBuffer = &mRoomUniformBuffer[i];
			add_resource(&uboCreate, nullptr);
		}

		return true;
	}

	void RemoveRenderResource()
	{
		remove_resource(mTexture);
		remove_resource(mLogoTex);

		remove_resource(mModelGeo);
		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			remove_resource(mRoomUniformBuffer[i]);
		}

		remove_sampler(mRenderer, mSampler);
		remove_shader(mRenderer, mPbrShader);

		exit_resource_loader_interface(mRenderer);

		remove_root_signature(mRenderer, mPbrRootSignature);

		for (uint32_t i = 0; i < IMAGE_COUNT; ++i)
		{
			remove_fence(mRenderer, mRenderCompleteFences[i]);
			remove_semaphore(mRenderer, mRenderCompleteSemaphores[i]);

			remove_cmd(mRenderer, mCmds[i]);
			remove_command_pool(mRenderer, mCmdPools[i]);
		}
		remove_semaphore(mRenderer, mImageAcquiredSemaphore);

		remove_queue(mRenderer, mGraphicQueue);
		remove_renderer(mRenderer);
	}

	void RegisterCameraControls()
	{
		GestureDesc gestureDesc;
		gestureDesc.minimumPressDurationMs = 1;
		gestureDesc.triggerBinding = SG_KEY_W;
		InputActionDesc inputAction = { SG_GESTURE_LONG_PRESS,
			[](InputActionContext* ctx)
				{
					auto* viewportWidget = (ViewportWidget*)ctx->pUserData;
					if (viewportWidget->IsWindowFocused())
						gCamera->OnMove({ 0.6f, 0.0f, 0.0f });
					return true;
				},
			mViewportWidget,
			gestureDesc
		};
		add_input_action(&inputAction);

		gestureDesc.triggerBinding = SG_KEY_S;
		inputAction = { SG_GESTURE_LONG_PRESS,
			[](InputActionContext* ctx)
				{
					auto* viewportWidget = (ViewportWidget*)ctx->pUserData;
					if (viewportWidget->IsWindowFocused())
						gCamera->OnMove({ -0.6f, 0.0f, 0.0f });
					return true;
				},
			mViewportWidget,
			gestureDesc
		};
		add_input_action(&inputAction);

		gestureDesc.triggerBinding = SG_KEY_A;
		inputAction = { SG_GESTURE_LONG_PRESS,
			[](InputActionContext* ctx)
				{
					auto* viewportWidget = (ViewportWidget*)ctx->pUserData;
					if (viewportWidget->IsWindowFocused())
						gCamera->OnMove({ 0.0f, -0.6f, 0.0f });
					return true;
				},
			mViewportWidget,
			gestureDesc
		};
		add_input_action(&inputAction);

		gestureDesc.triggerBinding = SG_KEY_D;
		inputAction = { SG_GESTURE_LONG_PRESS,
			[](InputActionContext* ctx)
				{
					auto* viewportWidget = (ViewportWidget*)ctx->pUserData;
					if (viewportWidget->IsWindowFocused())
						gCamera->OnMove({ 0.0f, 0.6f, 0.0f });
					return true;
				},
			mViewportWidget,
			gestureDesc
		};
		add_input_action(&inputAction);

		gestureDesc.triggerBinding = SG_KEY_SPACE;
		inputAction = { SG_GESTURE_LONG_PRESS,
			[](InputActionContext* ctx)
				{
					auto* viewportWidget = (ViewportWidget*)ctx->pUserData;
					if (viewportWidget->IsWindowFocused())
						gCamera->OnMove({ 0.0f, 0.0f, 0.3f });
					return true;
				},
			mViewportWidget,
			gestureDesc
		};
		add_input_action(&inputAction);

		gestureDesc.triggerBinding = SG_KEY_CTRLL;
		inputAction = { SG_GESTURE_LONG_PRESS,
			[](InputActionContext* ctx)
				{
					auto* viewportWidget = (ViewportWidget*)ctx->pUserData;
					if (viewportWidget->IsWindowFocused())
						gCamera->OnMove({ 0.0f, 0.0f, -0.3f });
					return true;
				},
			mViewportWidget,
			gestureDesc
		};
		add_input_action(&inputAction);

		gestureDesc.triggerBinding = SG_MOUSE_LEFT;
		inputAction = { SG_GESTURE_LONG_PRESS,
			[](InputActionContext* ctx)
				{
					auto* viewportWidget = (ViewportWidget*)ctx->pUserData;
					if (viewportWidget->IsWindowFocused())
					{
						Vec2 offset = { ctx->pPosition->x - gLastMousePos.x, ctx->pPosition->y - gLastMousePos.y };
						offset *= get_dpi_scale();
						gCamera->OnRotate(offset);
						if (viewportWidget->isHovered)
							set_enable_capture_input_custom(true, viewportWidget->GetViewportClipRect());
					}
					gLastMousePos = *ctx->pPosition;
					return true;
				},
			mViewportWidget,
			gestureDesc
		};
		add_input_action(&inputAction);

		inputAction = { SG_MOUSE_LEFT,
			[](InputActionContext* ctx)
				{
					if (ctx->phase == INPUT_ACTION_PHASE_CANCELED)
					{
						auto* viewportWidget = (ViewportWidget*)ctx->pUserData;
						set_enable_capture_input_custom(false, viewportWidget->GetViewportClipRect());
					}
					return true;
				},
			mViewportWidget
		};
		add_input_action(&inputAction);
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

	Shader* mPbrShader = nullptr;
	Sampler* mSampler = nullptr;

	Texture* mTexture = nullptr;
	Texture* mLogoTex = nullptr;
	RootSignature* mPbrRootSignature = nullptr;
	DescriptorSet* mDescriptorSet = nullptr;
	Pipeline* mPbrPipeline = nullptr;

	/// this texture is use for getting the current present rt in the swapchain
	RenderTarget* mDepthBuffer = nullptr;

	VertexLayout mRoomGeoVertexLayout = {};
	Geometry* mModelGeo = nullptr;

	Buffer* mRoomUniformBuffer[IMAGE_COUNT] = { nullptr, nullptr };
	UniformBuffer mModelData;
	DescriptorSet* mModelUboDescSet = nullptr;

	uint32_t mCurrentIndex = 0;

	// Gui
	UIMiddleware  mUiMiddleware;
	GuiComponent* mMainGui = nullptr;
	GuiComponent* mSecondGui = nullptr;

	GuiComponent* mViewportGui = nullptr;
	IWidget* mViewportWidget = nullptr;
	RenderTarget* mRt = nullptr;

	bool mStopUpdate = false;
	uint32_t mDelayDrawCount = 0;
};

SG_DEFINE_APPLICATION_MAIN(CustomRenderer)