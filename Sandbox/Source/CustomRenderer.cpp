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
	alignas(16) Matrix4 model;
	alignas(16) Matrix4 view;
	alignas(16) Matrix4 projection;
};

Vec2 mSliderData = {};

const char* windowSizesName[] = {
	"1920 x 1080",
	"1280 x 720"
};

uint32_t windowSizeSelect;
uint32_t windowSizeValue;
bool isEdited = false;

void EditorCallback()
{
	isEdited = true;
}

class CustomRenderer : public IApp
{
public:
	virtual bool OnInit() override
	{
		// set the file path
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_SHADER_SOURCES, "../../../Resources/Shaders");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_DEBUG, SG_RD_SHADER_BINARIES, "CompiledShaders");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_GPU_CONFIG, "GPUfg");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_TEXTURES, "../../../Resources/Textures");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_MESHES, "../../../Resources/Meshes");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_FONTS, "../../../Resources/Fonts");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_ANIMATIONS, "Animation");
		sgfs_set_path_for_resource_dir(pSystemFileIO, SG_RM_CONTENT, SG_RD_SCRIPTS, "Scripts");

		//SG_LOG_DEBUG("Main thread ID: %ul", Thread::get_curr_thread_id());

		if (!CreateRenderResource())
			return false;

		if (!mUiMiddleware.OnInit(mRenderer))
			return false;

		mUiMiddleware.LoadFont("Source_Sans_Pro/SourceSansPro-Regular.ttf");
		mUiMiddleware.LoadFont("Source_Sans_Pro/SourceSansPro-Bold.ttf");
		TextDrawDesc UITextDesc = { 0, 0xffff00ff, 18.0f };

		float dpiScale = get_dpi_scale().x;
		GuiCreateDesc guiDesc{};
		guiDesc.startPosition = { 0 , 0 };
		guiDesc.startSize = { mSettings.width * 0.5 , mSettings.height * 0.5 };
		guiDesc.defaultTextDrawDesc = UITextDesc;

		mMainGui = mUiMiddleware.AddGuiComponent("TestWindow1", &guiDesc);
		mMainGui->flags ^= SG_GUI_FLAGS_ALWAYS_AUTO_RESIZE;
		mMainGui->AddWidget(LabelWidget("TestWindow1"));
		mMainGui->AddWidget(ImageWidget("Logo", (void*)mLogoTex, { 256, 256 }));

		mSecondGui = mUiMiddleware.AddGuiComponent("TestWindow2", &guiDesc);
		mSecondGui->flags ^= SG_GUI_FLAGS_ALWAYS_AUTO_RESIZE;
		mSecondGui->AddWidget(SliderFloat2Widget("Slider", &mSliderData, { 0 , 0 }, { 5 , 5 }))->pOnEdited = []
		{
			SG_LOG_DEBUG("Value: (%f, %f))", mSliderData.x, mSliderData.y);
		};
		mSecondGui->AddWidget(ButtonWidget("StopUpdate", &mIsPressed));
		mSecondGui->AddWidget(DropdownWidget("WindowSize", &windowSizeSelect, windowSizesName, &windowSizeValue, 2))->pOnEdited = EditorCallback;

		mViewportGui = mUiMiddleware.AddGuiComponent("ViewportMini", &guiDesc);
		mViewportGui->flags ^= SG_GUI_FLAGS_ALWAYS_AUTO_RESIZE;
		mViewportWidget = mViewportGui->AddWidget(ViewportWidget("Viewport", { mSettings.width, mSettings.height }));

		//mUiMiddleware.mShowDemoUiWindow = true;

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
				set_enable_capture_input(capture && INPUT_ACTION_PHASE_CANCELED != ctx->phase);
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

		return true;
	}

	virtual void OnExit() override
	{
		wait_queue_idle(mGraphicQueue);

		exit_input_system();

		mUiMiddleware.OnExit();

		RemoveRenderResource();
	}

	virtual bool OnLoad() override
	{
		DescriptorSetCreateDesc descriptorSetCreate = { mRootSignature, SG_DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		add_descriptor_set(mRenderer, &descriptorSetCreate, &mDescriptorSet);
		descriptorSetCreate = { mRootSignature, SG_DESCRIPTOR_UPDATE_FREQ_PER_FRAME, IMAGE_COUNT };
		add_descriptor_set(mRenderer, &descriptorSetCreate, &mUboDescriptorSet);

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
			bufferUpdate[0].ppBuffers = &mUniformBuffer[i];
			update_descriptor_set(mRenderer, i, mUboDescriptorSet, 1, bufferUpdate);
		}

		return true;
	}

	virtual bool OnUnload() override
	{
		wait_queue_idle(mGraphicQueue);

		mUiMiddleware.OnUnload();

		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
			remove_render_target(mRenderer, mRts[i]);
		remove_render_target(mRenderer, mDepthBuffer);

		remove_pipeline(mRenderer, mPipeline);
		remove_swapchain(mRenderer, mSwapChain);

		remove_descriptor_set(mRenderer, mDescriptorSet);
		remove_descriptor_set(mRenderer, mUboDescriptorSet);

		return true;
	}

	virtual bool OnUpdate(float deltaTime) override
	{
		update_input_system(mSettings.width, mSettings.height);

		//static float degreed = 45.0f;
		static float time = 0.0f;
		time += deltaTime;
		
		/*if (InputListener::IsMousePressed(SG_MOUSE_LEFT) &&
			InputListener::GetMousePosClient().first >= 0 &&
			InputListener::GetMousePosClient().first <= mSettings.width &&
			InputListener::GetMousePosClient().second >= 0 &&
			InputListener::GetMousePosClient().second <= mSettings.height)
		{
			auto xOffset = InputListener::GetMousePosClient().first - mCurrentMousePos.x;
			auto yOffset = InputListener::GetMousePosClient().second - mCurrentMousePos.y;
			mCurrentMousePos.x = InputListener::GetMousePosClient().first;
			mCurrentMousePos.y = InputListener::GetMousePosClient().second;

			xOffset *= deltaTime * mXSensitity;
			yOffset *= deltaTime * mYSensitity;

			yaw -= xOffset;
			pitch -= yOffset;

			if (pitch <= -89.0f)
				pitch = -89.0f;
			if (pitch >= 89.0f)
				pitch = 89.0f;

			mViewVec.x = glm::cos(glm::radians(yaw));
			mViewVec.y = glm::cos(glm::radians(pitch)) * glm::sin(glm::radians(yaw));
			mViewVec.z = glm::sin(glm::radians(pitch));
		}
		else
		{
			mCurrentMousePos.x = InputListener::GetMousePosClient().first;
			mCurrentMousePos.y = InputListener::GetMousePosClient().second;
		}

		if (InputListener::IsKeyPressed(SG_KEY_W))
			mCameraPos += mViewVec * deltaTime * mCameraMoveSpeed;
		if (InputListener::IsKeyPressed(SG_KEY_S))
			mCameraPos -= mViewVec * deltaTime * mCameraMoveSpeed;
		if (InputListener::IsKeyPressed(SG_KEY_A))
			mCameraPos -= glm::cross(mViewVec, mUpVec) * deltaTime * mCameraMoveSpeed;
		if (InputListener::IsKeyPressed(SG_KEY_D))
			mCameraPos += glm::cross(mViewVec, mUpVec) * deltaTime * mCameraMoveSpeed;
		if (InputListener::IsKeyPressed(SG_KEY_SPACE))
			mCameraPos += mUpVec * deltaTime * mCameraMoveSpeed;
		if (InputListener::IsKeyPressed(SG_KEY_CTRLL))
			mCameraPos -= mUpVec * deltaTime * mCameraMoveSpeed;
		if (InputListener::IsKeyPressed(SG_KEY_O))
			degreed += deltaTime * 20.0f;
		if (InputListener::IsKeyPressed(SG_KEY_P))
			degreed -= deltaTime * 20.0f;*/

		mUbo.model = glm::rotate(Matrix4(1.0f), time * 0.03f * 60.0f, mUpVec);
		mUbo.view = glm::lookAt(mCameraPos, mCameraPos + glm::normalize(mViewVec), mUpVec);
		mUbo.projection = glm::perspective(glm::radians(45.0f), reinterpret_cast<ViewportWidget*>(mViewportWidget)->GetViewportFOV(),
			0.001f, 100000.0f);

		mUiMiddleware.OnUpdate(deltaTime);

		if (mIsPressed)
			mStopUpdate = !mStopUpdate;

		if (isEdited)
		{
			switch (windowSizeSelect)
			{
			case 0:
				set_window_size(mWindow, 1920, 1080);
				break;
			case 1:
				set_window_size(mWindow, 1280, 720);
				break;
			default:
				break;
			}
		}

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
		
		BufferUpdateDesc uboUpdate = {};
		uboUpdate.pBuffer = mUniformBuffer[mCurrentIndex];
		begin_update_resource(&uboUpdate);
		*(UniformBuffer*)uboUpdate.pMappedData = mUbo;
		end_update_resource(&uboUpdate, nullptr);

		const uint32_t stride = Vertex::GetStructSize();
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
		loadAction.clearDepth.stencil = 0.0f;

		renderTargetBarriers = { mRts[imageIndex], SG_RESOURCE_STATE_SHADER_RESOURCE, SG_RESOURCE_STATE_RENDER_TARGET };
		cmd_resource_barrier(cmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarriers);

		// begin render pass
		cmd_bind_render_targets(cmd, 1, &mRts[imageIndex], mDepthBuffer, &loadAction, nullptr, nullptr, -1, -1);
			cmd_set_viewport(cmd, 0.0f, 0.0f, (float)renderTarget->width, (float)renderTarget->height, 0.0f, 1.0f);
			cmd_set_scissor(cmd, 0, 0, renderTarget->width, renderTarget->height);

			cmd_bind_pipeline(cmd, mPipeline);
			cmd_bind_descriptor_set(cmd, 0, mDescriptorSet);
			cmd_bind_descriptor_set(cmd, mCurrentIndex, mUboDescriptorSet);

			cmd_bind_index_buffer(cmd, mRoomGeo->pIndexBuffer, mRoomGeo->indexType, 0);
			Buffer* vertexBuffer[] = { mRoomGeo->pVertexBuffers[0] };
			cmd_bind_vertex_buffer(cmd, 1, vertexBuffer, mRoomGeo->vertexStrides, nullptr);

			for (uint32_t i = 0; i < mRoomGeo->drawArgCount; i++)
			{
				IndirectDrawIndexArguments& cmdDraw = mRoomGeo->pDrawArgs[i];
				cmd_draw_indexed(cmd, cmdDraw.indexCount, cmdDraw.startIndex, cmdDraw.vertexOffset);
			}
		cmd_bind_render_targets(cmd, 0, nullptr, 0, nullptr, nullptr, nullptr, -1, -1);

		renderTargetBarriers = { mRts[imageIndex], SG_RESOURCE_STATE_RENDER_TARGET, SG_RESOURCE_STATE_SHADER_RESOURCE };
		cmd_resource_barrier(cmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarriers);

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

		// update the viewport rt
		if (!mStopUpdate)
			reinterpret_cast<ViewportWidget*>(mViewportWidget)->BindRenderTexture(mRts[imageIndex]->pTexture);
		else
			reinterpret_cast<ViewportWidget*>(mViewportWidget)->BindRenderTexture(mLogoTex);

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

		graphicPipe.pRootSignature = mRootSignature;
		graphicPipe.pShaderProgram = mTriangleShader;

		graphicPipe.pVertexLayout = &vertexLayout;
		graphicPipe.pRasterizerState = &rasterizeState;

		graphicPipe.depthStencilFormat = mDepthBuffer->format;

		graphicPipe.pBlendState = &blendStateDesc;
		add_pipeline(mRenderer, &pipelineCreate, &mPipeline);

		return mPipeline != nullptr;
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
		bool success = true;
		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
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
			
			add_render_target(mRenderer, &rtDesc, &mRts[i]);
			success &= mRts[i] != nullptr;
		}
		return success;
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
		geoCreate.ppGeometry = &mRoomGeo;
		geoCreate.pVertexLayout = &mRoomGeoVertexLayout;
		add_resource(&geoCreate, nullptr);

		ShaderLoadDesc loadBasicShader = {};
		loadBasicShader.stages[0] = { "triangle.vert", nullptr, 0, "main" };
		loadBasicShader.stages[1] = { "triangle.frag", nullptr, 0, "main" };
		add_shader(mRenderer, &loadBasicShader, &mTriangleShader);

		SamplerCreateDesc samplerCreate = {};
		samplerCreate.addressU = SG_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCreate.addressV = SG_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCreate.addressW = SG_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCreate.minFilter = SG_FILTER_LINEAR;
		samplerCreate.magFilter = SG_FILTER_LINEAR;
		samplerCreate.mipMapMode = SG_MIPMAP_MODE_LINEAR;
		add_sampler(mRenderer, &samplerCreate, &mSampler);

		Shader* submitShaders[] = { mTriangleShader };
		const char* staticSamplers[] = { "Sampler" };
		RootSignatureCreateDesc rootSignatureCreate = {};
		rootSignatureCreate.staticSamplerCount = COUNT_OF(staticSamplers);
		rootSignatureCreate.ppStaticSamplers = &mSampler;
		rootSignatureCreate.ppStaticSamplerNames = staticSamplers;
		rootSignatureCreate.ppShaders = submitShaders;
		rootSignatureCreate.shaderCount = COUNT_OF(submitShaders);
		add_root_signature(mRenderer, &rootSignatureCreate, &mRootSignature);

		BufferLoadDesc uboCreate;
		uboCreate.desc.descriptors = SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboCreate.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		uboCreate.desc.name = "UniformBuffer";
		uboCreate.desc.size = sizeof(UniformBuffer);
		uboCreate.desc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; // we need to update it every frame
		uboCreate.pData = nullptr;
		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			uboCreate.ppBuffer = &mUniformBuffer[i];
			add_resource(&uboCreate, nullptr);
		}

		return true;
	}

	void RemoveRenderResource()
	{
		remove_resource(mTexture);
		remove_resource(mLogoTex);

		remove_resource(mRoomGeo);
		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			remove_resource(mUniformBuffer[i]);
		}

		remove_sampler(mRenderer, mSampler);
		remove_shader(mRenderer, mTriangleShader);

		exit_resource_loader_interface(mRenderer);

		remove_root_signature(mRenderer, mRootSignature);

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
	Sampler* mSampler = nullptr;

	Texture* mTexture = nullptr;
	Texture* mLogoTex = nullptr;
	RootSignature* mRootSignature = nullptr;
	DescriptorSet* mDescriptorSet = nullptr;
	Pipeline* mPipeline = nullptr;

	/// this texture is use for getting the current present rt in the swapchain
	RenderTarget* mDepthBuffer = nullptr;

	VertexLayout mRoomGeoVertexLayout = {};
	Geometry* mRoomGeo = nullptr;

	Buffer* mUniformBuffer[IMAGE_COUNT] = { nullptr, nullptr };
	UniformBuffer mUbo;
	DescriptorSet* mUboDescriptorSet = nullptr;

	uint32_t mCurrentIndex = 0;

	Vec3  mCameraPos = { -3.0f, 0.0f, 0.4f };
	Vec3  mViewVec = { 1.0f, 0.0f, 0.0f };
	Vec3  mUpVec = { 0.0f, 0.0f, 1.0f };

	// Gui
	UIMiddleware  mUiMiddleware;
	GuiComponent* mMainGui = nullptr;
	GuiComponent* mSecondGui = nullptr;

	GuiComponent* mViewportGui = nullptr;
	IWidget* mViewportWidget = nullptr;
	RenderTarget* mRts[IMAGE_COUNT] = { nullptr, nullptr };

	bool mIsPressed = false;
	bool mStopUpdate = false;
};

SG_DEFINE_APPLICATION_MAIN(CustomRenderer)