#include "Seagull.h"
using namespace SG;

#include "CustomLighting/Light.h"
#include "CustomLighting/Material.h"

#define IMAGE_COUNT 2

#define COUNT_OF(a) (sizeof(a) / sizeof(a[0]))

struct UniformBuffer
{
	alignas(16) Matrix4 model;
};

struct CameraUbo
{
	alignas(16) Matrix4 view;
	alignas(16) Matrix4 proj;
	Vec3 cameraPos;
};

struct LightProxyData 
{
	alignas(16) Matrix4 model[2];      // 2 instances of lights
	alignas(16) Vec4    lightColor[2]; // w is for the intensity
};

Vec2  gSliderData = {};
float gFps = 0.0;
bool  gIsGuiFocused = false;

ICameraController* gCamera = nullptr;
Vec2			   gLastMousePos;

PointLight gDefaultLight1;
uint32_t gLightColor1 = 0xffffffff;
float    gLightRange1 = 2.0f;

PointLight gDefaultLight2;
uint32_t gLightColor2 = 0xffffffff;
float    gLightRange2 = 2.2f;

bool  gIsRotating = false;
void ToggleRotate()
{
	gIsRotating = !gIsRotating;
}

bool  gDrawLightProxyGeom = false;
void ToggleDrawLightProxyGeom()
{
	gDrawLightProxyGeom = !gDrawLightProxyGeom;
}

Vec3 pCubePoints[36 * 2];
static void GenerateCube(Vec3* pPoints)
{
	uint32_t counter = 0;
	// bottom face
	pPoints[counter++] = { -0.5f, -0.5f, -0.5f }; // position
	pPoints[counter++] = {  0.0f,  0.0f, 1.0f }; // normal
	pPoints[counter++] = { -0.5f,  0.5f, -0.5f };
	pPoints[counter++] = { 0.0f,  0.0f,  1.0f };  // normal
	pPoints[counter++] = {  0.5f,  0.5f, -0.5f };
	pPoints[counter++] = { 0.0f,  0.0f, 1.0f }; // normal
	pPoints[counter++] = { -0.5f, -0.5f, -0.5f };
	pPoints[counter++] = { 0.0f,  0.0f, 1.0f }; // normal
	pPoints[counter++] = {  0.5f,  0.5f, -0.5f };
	pPoints[counter++] = { 0.0f,  0.0f, 1.0f }; // normal
	pPoints[counter++] = {  0.5f, -0.5f, -0.5f };
	pPoints[counter++] = { 0.0f,  0.0f, 1.0f }; // normal
	/// upper face
	pPoints[counter++] = { -0.5f, -0.5f,  0.5f };
	pPoints[counter++] = { 0.0f,  0.0f, -1.0f }; // normal
	pPoints[counter++] = {  0.5f,  0.5f,  0.5f };
	pPoints[counter++] = { 0.0f,  0.0f, -1.0f }; // normal
	pPoints[counter++] = { -0.5f,  0.5f,  0.5f };
	pPoints[counter++] = { 0.0f,  0.0f, -1.0f }; // normal
	pPoints[counter++] = { -0.5f, -0.5f,  0.5f };
	pPoints[counter++] = { 0.0f,  0.0f, -1.0f }; // normal
	pPoints[counter++] = { 0.5f, -0.5f,  0.5f };
	pPoints[counter++] = { 0.0f,  0.0f, -1.0f }; // normal
	pPoints[counter++] = { 0.5f,  0.5f,  0.5f };
	pPoints[counter++] = { 0.0f,  0.0f, -1.0f }; // normal
	/// right face
	pPoints[counter++] = {  0.5f, -0.5f, -0.5f };
	pPoints[counter++] = { -1.0f,  0.0f, 0.0f }; // normal
	pPoints[counter++] = { 0.5f,  0.5f,  0.5f };
	pPoints[counter++] = { -1.0f,  0.0f, 0.0f }; // normal
	pPoints[counter++] = { 0.5f, -0.5f,  0.5f };
	pPoints[counter++] = { -1.0f,  0.0f, 0.0f }; // normal
	pPoints[counter++] = { 0.5f, -0.5f, -0.5f };
	pPoints[counter++] = { -1.0f,  0.0f, 0.0f }; // normal
	pPoints[counter++] = { 0.5f,  0.5f, -0.5f };
	pPoints[counter++] = { -1.0f,  0.0f, 0.0f }; // normal
	pPoints[counter++] = { 0.5f,  0.5f,  0.5f };
	pPoints[counter++] = { -1.0f,  0.0f, 0.0f }; // normal
	//// left face
	pPoints[counter++] = { -0.5f, -0.5f, -0.5f };
	pPoints[counter++] = { 1.0f,  0.0f, 0.0f }; // normal
	pPoints[counter++] = { -0.5f, -0.5f,  0.5f };
	pPoints[counter++] = { 1.0f,  0.0f, 0.0f }; // normal
	pPoints[counter++] = { -0.5f,  0.5f,  0.5f };
	pPoints[counter++] = { 1.0f,  0.0f, 0.0f }; // normal
	pPoints[counter++] = { -0.5f, -0.5f, -0.5f };
	pPoints[counter++] = { 1.0f,  0.0f, 0.0f }; // normal
	pPoints[counter++] = { -0.5f,  0.5f,  0.5f };
	pPoints[counter++] = { 1.0f,  0.0f, 0.0f }; // normal
	pPoints[counter++] = { -0.5f,  0.5f, -0.5f };
	pPoints[counter++] = { 1.0f,  0.0f, 0.0f }; // normal
	///// front face
	pPoints[counter++] = { -0.5f, -0.5f, -0.5f };
	pPoints[counter++] = { 0.0f,  1.0f, 0.0f }; // normal
	pPoints[counter++] = { 0.5f, -0.5f,  0.5f };
	pPoints[counter++] = { 0.0f,  1.0f, 0.0f }; // normal
	pPoints[counter++] = { -0.5f, -0.5f,  0.5f };
	pPoints[counter++] = { 0.0f,  1.0f, 0.0f }; // normal
	pPoints[counter++] = { -0.5f, -0.5f, -0.5f };
	pPoints[counter++] = { 0.0f,  1.0f, 0.0f }; // normal
	pPoints[counter++] = { 0.5f, -0.5f, -0.5f };
	pPoints[counter++] = { 0.0f,  1.0f, 0.0f }; // normal
	pPoints[counter++] = { 0.5f, -0.5f,  0.5f };
	pPoints[counter++] = { 0.0f,  1.0f, 0.0f }; // normal
	//// back face
	pPoints[counter++] = { -0.5f,  0.5f, -0.5f };
	pPoints[counter++] = { 0.0f,  -1.0f, 0.0f }; // normal
	pPoints[counter++] = { -0.5f,  0.5f,  0.5f };
	pPoints[counter++] = { 0.0f,  -1.0f, 0.0f }; // normal
	pPoints[counter++] = {  0.5f,  0.5f,  0.5f };
	pPoints[counter++] = { 0.0f,  -1.0f, 0.0f }; // normal
	pPoints[counter++] = { -0.5f,  0.5f, -0.5f };
	pPoints[counter++] = { 0.0f,  -1.0f, 0.0f }; // normal
	pPoints[counter++] = {  0.5f,  0.5f,  0.5f };
	pPoints[counter++] = { 0.0f,  -1.0f, 0.0f }; // normal
	pPoints[counter++] = {  0.5f,  0.5f, -0.5f };
	pPoints[counter++] = { 0.0f,  -1.0f, 0.0f }; // normal
}

ColorSliderWidget   gSliderW_1("Color", &gLightColor1);
ControlFloat3Widget gControlFloat3_1("Position", &gDefaultLight1.position);
SliderFloatWidget   gFloat1_1("Intensity", &gDefaultLight1.intensity, 0.0f, 1.0f);
SliderFloatWidget   gFloat2_1("Range", &gLightRange1, 0.0f, 6.0f);

ColorSliderWidget   gSliderW_2("Color", &gLightColor2);
ControlFloat3Widget gControlFloat3_2("Position", &gDefaultLight2.position);
SliderFloatWidget   gFloat1_2("Intensity", &gDefaultLight2.intensity, 0.0f, 1.0f);
SliderFloatWidget   gFloat2_2("Range", &gLightRange2, 0.0f, 6.0f);

MaterialData        gMaterialData;
uint32_t            gMaterialColor = 0xffffffff;
ColorSliderWidget   gColorSliderMat("Color", &gMaterialColor);
SliderFloatWidget   gSliderMat("Metallic", &gMaterialData.metallic, 0.0f, 1.0f);
SliderFloatWidget   gSliderSmoo("Smoothness", &gMaterialData.smoothness, 0.0f, 1.0f);

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

		gDefaultLight1.color = { 1.0f, 1.0f, 1.0f };
		gDefaultLight1.intensity = 0.75f;
		gDefaultLight1.position = { 0.0f, 0.0f, 1.0f };
		gDefaultLight1.range = 2.0f;

		gDefaultLight2.color = { 1.0f, 1.0f, 1.0f };
		gDefaultLight2.intensity = 0.45f;
		gDefaultLight2.position = { -0.8f, 0.0f, 0.1f };
		gDefaultLight2.range = 2.2f;

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
		guiDesc.startSize = { mSettings.width * 0.5 * dpiScale , mSettings.height * 0.5 * dpiScale };
		guiDesc.defaultTextDrawDesc = UITextDesc;

		mMainGui = mUiMiddleware.AddGuiComponent("TestWindow", &guiDesc);
		mMainGui->flags ^= SG_GUI_FLAGS_ALWAYS_AUTO_RESIZE;
		mMainGui->AddWidget(LabelWidget("TestWindow1"));
		mMainGui->AddWidget(ImageWidget("Logo", (void*)mLogoTex, { 256, 256 }));

		mSecondGui = mUiMiddleware.AddGuiComponent("Settings", &guiDesc);
		mSecondGui->flags ^= SG_GUI_FLAGS_ALWAYS_AUTO_RESIZE;
		ButtonWidget buttonRotate("Toggle Rotate");
		buttonRotate.pOnEdited = ToggleRotate;
		mSecondGui->AddWidget(buttonRotate);
		ButtonWidget buttonLight("Toggle Light Proxy Geom");
		buttonLight.pOnEdited = ToggleDrawLightProxyGeom;
		mSecondGui->AddWidget(buttonLight);

		mSecondGui->AddWidget(SeparatorWidget());
		PropertyWidget propWidget_1("Light1", true);
		propWidget_1.AddItem(&gSliderW_1);
		propWidget_1.AddItem(&gControlFloat3_1);
		propWidget_1.AddItem(&gFloat1_1);
		propWidget_1.AddItem(&gFloat2_1);
		mSecondGui->AddWidget(propWidget_1);

		PropertyWidget propWidget_2("Light2", true);
		propWidget_2.AddItem(&gSliderW_2);
		propWidget_2.AddItem(&gControlFloat3_2);
		propWidget_2.AddItem(&gFloat1_2);
		propWidget_2.AddItem(&gFloat2_2);
		mSecondGui->AddWidget(propWidget_2);

		mSecondGui->AddWidget(SeparatorWidget());
		PropertyWidget materialWidget("Material", true);
		materialWidget.AddItem(&gColorSliderMat);
		materialWidget.AddItem(&gSliderMat);
		materialWidget.AddItem(&gSliderSmoo);
		mSecondGui->AddWidget(materialWidget);

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
		DescriptorSetCreateDesc descriptorSetCreate = { mSkyboxRootSignature, SG_DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		add_descriptor_set(mRenderer, &descriptorSetCreate, &mSkyboxDescriptorTexSet);
		descriptorSetCreate = { mRoomRootSignature, SG_DESCRIPTOR_UPDATE_FREQ_PER_FRAME, IMAGE_COUNT * 2 };
		add_descriptor_set(mRenderer, &descriptorSetCreate, &mRoomUboDescriptorSet);
		descriptorSetCreate = { mCubeRootSignature, SG_DESCRIPTOR_UPDATE_FREQ_PER_FRAME, IMAGE_COUNT * 3 };
		add_descriptor_set(mRenderer, &descriptorSetCreate, &mCubeUboDescriptorSet);
		descriptorSetCreate = { mSkyboxRootSignature, SG_DESCRIPTOR_UPDATE_FREQ_PER_FRAME, IMAGE_COUNT * 2 };
		add_descriptor_set(mRenderer, &descriptorSetCreate, &mSkyboxDescriptorUboSet);

		if (!CreateSwapChain())
			return false;

		if (!mUiMiddleware.OnLoad(mSwapChain->ppRenderTargets))
			return false;

		if (!CreateDepthBuffer())
			return false;

		wait_for_all_resource_loads();

		if (!CreateSkyboxPipeline())
			return false;

		if (!CreateGraphicPipeline())
			return false;

		if (!CreateLightpassPipeline())
			return false;

		DescriptorData updateData[2] = {};
		updateData[0].name = "skyboxCubeMap";
		updateData[0].ppTextures = &mSkyboxCubeMap[0];
		//updateData[1].name = "skyboxCubeMapL";
		//updateData[1].ppTextures = &mSkyboxCubeMap[1];
		//updateData[2].name = "skyboxCubeMapU";
		//updateData[2].ppTextures = &mSkyboxCubeMap[2];
		//updateData[3].name = "skyboxCubeMapD";
		//updateData[3].ppTextures = &mSkyboxCubeMap[3];
		//updateData[4].name = "skyboxCubeMapF";
		//updateData[4].ppTextures = &mSkyboxCubeMap[4];
		//updateData[5].name = "skyboxCubeMapB";
		//updateData[5].ppTextures = &mSkyboxCubeMap[5];
		update_descriptor_set(mRenderer, 0, mSkyboxDescriptorTexSet, 1, updateData); // update the descriptor sets use

		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			DescriptorData bufferUpdate[4] = {};
			bufferUpdate[0].name = "ubo";
			bufferUpdate[0].ppBuffers = &mRoomUniformBuffer[i];
			bufferUpdate[1].name = "camera";
			bufferUpdate[1].ppBuffers = &mCameraUniformBuffer[i];
			update_descriptor_set(mRenderer, i, mSkyboxDescriptorUboSet, 2, bufferUpdate);
		}

		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			DescriptorData bufferUpdate[4] = {};
			bufferUpdate[0].name = "ubo";
			bufferUpdate[0].ppBuffers = &mRoomUniformBuffer[i];
			bufferUpdate[1].name = "camera";
			bufferUpdate[1].ppBuffers = &mCameraUniformBuffer[i];
			bufferUpdate[2].name = "light";
			bufferUpdate[2].ppBuffers = mLightUniformBuffer[i];
			bufferUpdate[2].count = 2;
			bufferUpdate[3].name = "mat";
			bufferUpdate[3].ppBuffers = &mMaterialUniformBuffer[i];
			update_descriptor_set(mRenderer, i, mRoomUboDescriptorSet, 4, bufferUpdate);
		}

		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			DescriptorData bufferUpdate[2] = {};
			bufferUpdate[0].name = "lightUbo";
			bufferUpdate[0].ppBuffers = &mCubeUniformBuffer[i];
			bufferUpdate[1].name = "camera";
			bufferUpdate[1].ppBuffers = &mCameraUniformBuffer[i];
			update_descriptor_set(mRenderer, i, mCubeUboDescriptorSet, 2, bufferUpdate);
		}

		return true;
	}

	virtual bool OnUnload() override
	{
		wait_queue_idle(mGraphicQueue);

		mUiMiddleware.OnUnload();

		remove_render_target(mRenderer, mDepthBuffer);

		remove_pipeline(mRenderer, mSkyboxPipeline);
		remove_pipeline(mRenderer, mLightProxyGeomPipeline);
		remove_pipeline(mRenderer, mDefaultPipeline);
		remove_swapchain(mRenderer, mSwapChain);

		remove_descriptor_set(mRenderer, mSkyboxDescriptorTexSet);
		remove_descriptor_set(mRenderer, mRoomUboDescriptorSet);
		remove_descriptor_set(mRenderer, mCubeUboDescriptorSet);

		return true;
	}

	virtual bool OnUpdate(float deltaTime) override
	{
		update_input_system(mSettings.width, mSettings.height);

		gCamera->SetCameraLens(glm::radians(45.0f), (float)mSettings.width / mSettings.height, 0.001f, 1000.0f);
		gCamera->OnUpdate(deltaTime);
		gLastMousePos = get_mouse_pos_relative(mWindow);

		static float time = 0.0f;
		static float rotateTime = 0.0f;
		time += deltaTime;

		if (gIsRotating)
			rotateTime += deltaTime;

		mModelData.model = glm::rotate(Matrix4(1.0f), glm::radians(rotateTime * 90.0f + 145.0f), { 0.0f, 0.0f, 1.0f }) *
			glm::scale(Matrix4(1.0f), { 0.3f, 0.3f, 0.3f });

		//mModelData.model = glm::translate(Matrix4(1.0f), gCamera->GetPosition());

		gDefaultLight1.color = UintToVec4Color(gLightColor1) / 255.0f;
		gDefaultLight1.range = 1.0f / eastl::max(glm::pow(gLightRange1, 2.0f), 0.000001f);
		gDefaultLight2.color = UintToVec4Color(gLightColor2) / 255.0f;
		gDefaultLight2.range = 1.0f / eastl::max(glm::pow(gLightRange2, 2.0f), 0.000001f);

		mLightData.model[0] = glm::translate(Matrix4(1.0f), gDefaultLight1.position) * glm::scale(Matrix4(1.0f), { 0.03f, 0.03f, 0.03f });
		mLightData.model[1] = glm::translate(Matrix4(1.0f), gDefaultLight2.position) * glm::scale(Matrix4(1.0f), { 0.03f, 0.03f, 0.03f });
		mLightData.lightColor[0] = { gDefaultLight1.color.r, gDefaultLight1.color.g, gDefaultLight1.color.b, gDefaultLight1.intensity };
		mLightData.lightColor[1] = { gDefaultLight2.color.r, gDefaultLight2.color.g, gDefaultLight2.color.b, gDefaultLight2.intensity };

		mCameraData.view = gCamera->GetViewMatrix();
		mCameraData.proj = gCamera->GetProjMatrix();
		mCameraData.cameraPos = gCamera->GetPosition();

		Vec4 matColor = UintToVec4Color(gMaterialColor) / 255.0f;
		gMaterialData.color = { matColor.x, matColor.y, matColor.z };

		UpdateResoureces();

		mUiMiddleware.OnUpdate(deltaTime);
		gIsGuiFocused = mUiMiddleware.IsFocused();

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

		const uint32_t vertexStride = sizeof(float) * 6;

		renderTargetBarriers = { renderTarget, SG_RESOURCE_STATE_PRESENT, SG_RESOURCE_STATE_RENDER_TARGET };
		cmd_resource_barrier(cmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarriers);

		// begin render pass
		cmd_bind_render_targets(cmd, 1, &renderTarget, mDepthBuffer, &loadAction, nullptr, nullptr, -1, -1);
			cmd_set_viewport(cmd, 0.0f, 0.0f, (float)renderTarget->width, (float)renderTarget->height, 0.0f, 1.0f);
			cmd_set_scissor(cmd, 0, 0, renderTarget->width, renderTarget->height);

			if (gDrawLightProxyGeom)
			{
				/// light pass start
				cmd_bind_pipeline(cmd, mLightProxyGeomPipeline);
				cmd_bind_descriptor_set(cmd, mCurrentIndex, mCubeUboDescriptorSet);

				cmd_bind_vertex_buffer(cmd, 1, &mCubeVertexBuffer, &vertexStride, nullptr);
				cmd_draw_instanced(cmd, 36, 0, 2, 0);
				/// light pass end
			}

			/// geom start
			//cmd_bind_pipeline(cmd, mDefaultPipeline);
			//cmd_bind_descriptor_set(cmd, mCurrentIndex, mRoomUboDescriptorSet);

			//cmd_bind_index_buffer(cmd, mRoomGeo->pIndexBuffer, mRoomGeo->indexType, 0);
			//Buffer* vertexBuffer[] = { mRoomGeo->pVertexBuffers[0] };
			//cmd_bind_vertex_buffer(cmd, 1, vertexBuffer, mRoomGeo->vertexStrides, nullptr);

			//for (uint32_t i = 0; i < mRoomGeo->drawArgCount; i++)
			//{
			//	IndirectDrawIndexArguments& cmdDraw = mRoomGeo->pDrawArgs[i];
			//	cmd_draw_indexed(cmd, cmdDraw.indexCount, cmdDraw.startIndex, cmdDraw.vertexOffset);
			//}
			/// geom end

			/// skybox start
			//cmd_set_viewport(cmd, 0.0f, 0.0f, (float)renderTarget->width, (float)renderTarget->height, 1.0f, 1.0f);

			cmd_bind_pipeline(cmd, mSkyboxPipeline);
			cmd_bind_descriptor_set(cmd, 0, mSkyboxDescriptorTexSet); // just the skybox texture
			cmd_bind_descriptor_set(cmd, mCurrentIndex, mSkyboxDescriptorUboSet); // just the skybox texture

			cmd_bind_vertex_buffer(cmd, 1, &mCubeVertexBuffer, &vertexStride, nullptr);
			cmd_draw(cmd, 36, 0);

			cmd_set_viewport(cmd, 0.0f, 0.0f, (float)renderTarget->width, (float)renderTarget->height, 0.0f, 1.0f);
			/// skybox end
		cmd_bind_render_targets(cmd, 0, nullptr, 0, nullptr, nullptr, nullptr, -1, -1);

		cmd_bind_render_targets(cmd, 1, &renderTarget, nullptr, nullptr, nullptr, nullptr, -1, -1);
			cmd_set_viewport(cmd, 0.0f, 0.0f, (float)renderTarget->width, (float)renderTarget->height, 0.0f, 1.0f);
			cmd_set_scissor(cmd, 0, 0, renderTarget->width, renderTarget->height);

			mUiMiddleware.AddUpdateGui(mMainGui);
			mUiMiddleware.AddUpdateGui(mSecondGui);
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
		vertexLayout.attribCount = 3;
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

		vertexLayout.attribs[2].semantic = SG_SEMANTIC_NORMAL;
		vertexLayout.attribs[2].format = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayout.attribs[2].binding = 0;
		vertexLayout.attribs[2].location = 2;
		vertexLayout.attribs[2].offset = 5 * sizeof(float);

		RasterizerStateDesc rasterizeState = {};
		rasterizeState.cullMode = SG_CULL_MODE_BACK;
		rasterizeState.frontFace = SG_FRONT_FACE_CCW;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.depthTest = true;
		depthStateDesc.depthWrite = true;
		depthStateDesc.depthFunc = SG_COMPARE_MODE_LEQUAL;

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

		graphicPipe.pRootSignature = mRoomRootSignature;
		graphicPipe.pShaderProgram = mDefaultShader;

		graphicPipe.pVertexLayout = &vertexLayout;
		graphicPipe.pRasterizerState = &rasterizeState;

		graphicPipe.depthStencilFormat = mDepthBuffer->format;

		graphicPipe.pBlendState = &blendStateDesc;
		add_pipeline(mRenderer, &pipelineCreate, &mDefaultPipeline);

		return mDefaultPipeline != nullptr;
	}

	bool CreateLightpassPipeline()
	{
		VertexLayout vertexLayout = {};
		vertexLayout.attribCount = 2;
		vertexLayout.attribs[0].semantic = SG_SEMANTIC_POSITION;
		vertexLayout.attribs[0].format = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayout.attribs[0].binding = 0;
		vertexLayout.attribs[0].location = 0;
		vertexLayout.attribs[0].offset = 0;

		vertexLayout.attribs[1].semantic = SG_SEMANTIC_NORMAL;
		vertexLayout.attribs[1].format = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayout.attribs[1].binding = 0;
		vertexLayout.attribs[1].location = 1;
		vertexLayout.attribs[1].offset = 3 * sizeof(float);;

		RasterizerStateDesc rasterizeState = {};
		rasterizeState.cullMode = SG_CULL_MODE_BACK;
		rasterizeState.frontFace = SG_FRONT_FACE_CCW;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.depthTest = true;
		depthStateDesc.depthWrite = true;
		depthStateDesc.depthFunc = SG_COMPARE_MODE_LEQUAL;

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.srcFactors[0] = SG_BLEND_CONST_SRC_ALPHA;
		blendStateDesc.srcFactors[1] = SG_BLEND_CONST_SRC_ALPHA;
		blendStateDesc.dstFactors[0] = SG_BLEND_CONST_ONE_MINUS_SRC_ALPHA;
		blendStateDesc.dstFactors[1] = SG_BLEND_CONST_ONE_MINUS_SRC_ALPHA;
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

		graphicPipe.pRootSignature = mCubeRootSignature;
		graphicPipe.pShaderProgram = mLightShader;

		graphicPipe.pVertexLayout = &vertexLayout;
		graphicPipe.pRasterizerState = &rasterizeState;

		graphicPipe.depthStencilFormat = mDepthBuffer->format;

		graphicPipe.pBlendState = &blendStateDesc;
		add_pipeline(mRenderer, &pipelineCreate, &mLightProxyGeomPipeline);

		return mLightProxyGeomPipeline != nullptr;
	}

	bool CreateSkyboxPipeline()
	{
		VertexLayout vertexLayout = {};
		vertexLayout.attribCount = 2;

		vertexLayout.attribs[0].semantic = SG_SEMANTIC_POSITION;
		vertexLayout.attribs[0].format = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayout.attribs[0].binding = 0;
		vertexLayout.attribs[0].location = 0;
		vertexLayout.attribs[0].offset = 0;

		vertexLayout.attribs[1].semantic = SG_SEMANTIC_NORMAL;
		vertexLayout.attribs[1].format = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayout.attribs[1].binding = 0;
		vertexLayout.attribs[1].location = 1;
		vertexLayout.attribs[1].offset = 3 * sizeof(float);

		RasterizerStateDesc rasterizeState = {};
		rasterizeState.cullMode = SG_CULL_MODE_NONE;
		rasterizeState.frontFace = SG_FRONT_FACE_CCW;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.depthTest = false;
		depthStateDesc.depthWrite = false;
		depthStateDesc.depthFunc = SG_COMPARE_MODE_LEQUAL;

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

		graphicPipe.pRootSignature = mSkyboxRootSignature;
		graphicPipe.pShaderProgram = mSkyboxShader;

		graphicPipe.pVertexLayout = &vertexLayout;
		graphicPipe.pRasterizerState = &rasterizeState;

		graphicPipe.depthStencilFormat = mDepthBuffer->format;

		graphicPipe.pBlendState = &blendStateDesc;
		add_pipeline(mRenderer, &pipelineCreate, &mSkyboxPipeline);

		return mSkyboxPipeline != nullptr;
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
		textureCreate.fileName = "logo";
		textureCreate.ppTexture = &mLogoTex;
		add_resource(&textureCreate, nullptr);

		// cube map sequence -- r, l, u, d, f, b
		for (int i = 0; i < 6; i++)
		{
			eastl::string name = "sahara_" + eastl::to_string(i + 1);
			textureCreate.fileName = name.c_str();
			textureCreate.ppTexture = &mSkyboxCubeMap[i];
			add_resource(&textureCreate, nullptr);
		}

		VertexLayout roomGeoVertexLayout;
		roomGeoVertexLayout.attribCount = 3;

		roomGeoVertexLayout.attribs[0].semantic = SG_SEMANTIC_POSITION;
		roomGeoVertexLayout.attribs[0].format = TinyImageFormat_R32G32B32_SFLOAT;
		roomGeoVertexLayout.attribs[0].binding = 0;
		roomGeoVertexLayout.attribs[0].location = 0;
		roomGeoVertexLayout.attribs[0].offset = 0;

		roomGeoVertexLayout.attribs[1].semantic = SG_SEMANTIC_TEXCOORD0;
		roomGeoVertexLayout.attribs[1].format = TinyImageFormat_R32G32_SFLOAT;
		roomGeoVertexLayout.attribs[1].binding = 0;
		roomGeoVertexLayout.attribs[1].location = 1;
		roomGeoVertexLayout.attribs[1].offset = 3 * sizeof(float);

		roomGeoVertexLayout.attribs[2].semantic = SG_SEMANTIC_NORMAL;
		roomGeoVertexLayout.attribs[2].format = TinyImageFormat_R32G32B32_SFLOAT;
		roomGeoVertexLayout.attribs[2].binding = 0;
		roomGeoVertexLayout.attribs[2].location = 2;
		roomGeoVertexLayout.attribs[2].offset = 5 * sizeof(float);

		GeometryLoadDesc geoCreate = {};
		geoCreate.fileName = "sphere.gltf";
		geoCreate.ppGeometry = &mRoomGeo;
		geoCreate.pVertexLayout = &roomGeoVertexLayout;
		add_resource(&geoCreate, nullptr);

		GenerateCube(pCubePoints);
		BufferLoadDesc sphereVbDesc = {};
		sphereVbDesc.desc.descriptors = SG_DESCRIPTOR_TYPE_VERTEX_BUFFER;
		sphereVbDesc.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_GPU_ONLY;
		sphereVbDesc.desc.size = 36 * 2 * sizeof(Vec3);
		sphereVbDesc.pData = (float*)pCubePoints;
		sphereVbDesc.ppBuffer = &mCubeVertexBuffer;
		add_resource(&sphereVbDesc, NULL);

		ShaderLoadDesc loadBasicShader = {};
		loadBasicShader.stages[0] = { "default.vert", nullptr, 0, "main" };
		loadBasicShader.stages[1] = { "brdf.frag", nullptr, 0, "main" };
		add_shader(mRenderer, &loadBasicShader, &mDefaultShader);

		loadBasicShader.stages[0] = { "LightProxy/lightGeo.vert", nullptr, 0, "main" };
		loadBasicShader.stages[1] = { "LightProxy/lightGeo.frag", nullptr, 0, "main" };
		add_shader(mRenderer, &loadBasicShader, &mLightShader);

		loadBasicShader.stages[0] = { "sky.vert", nullptr, 0, "main" };
		loadBasicShader.stages[1] = { "sky.frag", nullptr, 0, "main" };
		add_shader(mRenderer, &loadBasicShader, &mSkyboxShader);

		SamplerCreateDesc samplerCreate = {};
		samplerCreate.addressU = SG_ADDRESS_MODE_REPEAT;
		samplerCreate.addressV = SG_ADDRESS_MODE_REPEAT;
		samplerCreate.addressW = SG_ADDRESS_MODE_REPEAT;
		samplerCreate.minFilter = SG_FILTER_LINEAR;
		samplerCreate.magFilter = SG_FILTER_LINEAR;
		samplerCreate.mipMapMode = SG_MIPMAP_MODE_LINEAR;
		add_sampler(mRenderer, &samplerCreate, &mSampler);

		Shader* submitSkyboxShader[] = { mSkyboxShader };
		const char* staticSamplers[] = { "Sampler" };
		RootSignatureCreateDesc rootSignatureCreate = {};
		rootSignatureCreate.staticSamplerCount = 1;
		rootSignatureCreate.ppStaticSamplers = &mSampler;
		rootSignatureCreate.ppStaticSamplerNames = staticSamplers;
		rootSignatureCreate.ppShaders = submitSkyboxShader;
		rootSignatureCreate.shaderCount = COUNT_OF(submitSkyboxShader);
		add_root_signature(mRenderer, &rootSignatureCreate, &mSkyboxRootSignature);

		Shader* submitShaders[] = { mDefaultShader };
		rootSignatureCreate = {};
		rootSignatureCreate.ppShaders = submitShaders;
		rootSignatureCreate.shaderCount = COUNT_OF(submitShaders);
		add_root_signature(mRenderer, &rootSignatureCreate, &mRoomRootSignature);

		submitShaders[0] = { mLightShader };
		rootSignatureCreate.ppShaders = submitShaders;
		rootSignatureCreate.shaderCount = COUNT_OF(submitShaders);
		add_root_signature(mRenderer, &rootSignatureCreate, &mCubeRootSignature);

		BufferLoadDesc uboCreate;
		uboCreate.desc.descriptors = SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboCreate.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		uboCreate.desc.name = "RoomUniformBuffer";
		uboCreate.desc.size = sizeof(UniformBuffer);
		uboCreate.desc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; // we need to update it every frame
		uboCreate.pData = nullptr;
		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			uboCreate.ppBuffer = &mRoomUniformBuffer[i];
			add_resource(&uboCreate, nullptr);
		}

		uboCreate.desc.descriptors = SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboCreate.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		uboCreate.desc.name = "LightUbo";
		uboCreate.desc.size = sizeof(PointLight);
		uboCreate.desc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; // we need to update it every frame
		uboCreate.pData = nullptr;
		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			uboCreate.ppBuffer = &mLightUniformBuffer[i][0];
			add_resource(&uboCreate, nullptr);
			uboCreate.ppBuffer = &mLightUniformBuffer[i][1];
			add_resource(&uboCreate, nullptr);
		}

		uboCreate.desc.descriptors = SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboCreate.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		uboCreate.desc.name = "CubeUniformBuffer";
		uboCreate.desc.size = sizeof(LightProxyData);
		uboCreate.desc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; // we need to update it every frame
		uboCreate.pData = nullptr;
		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			uboCreate.ppBuffer = &mCubeUniformBuffer[i];
			add_resource(&uboCreate, nullptr);
		}

		uboCreate.desc.descriptors = SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboCreate.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		uboCreate.desc.name = "CameraUniformBuffer";
		uboCreate.desc.size = sizeof(CameraUbo);
		uboCreate.desc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; // we need to update it every frame
		uboCreate.pData = nullptr;
		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			uboCreate.ppBuffer = &mCameraUniformBuffer[i];
			add_resource(&uboCreate, nullptr);
		}

		uboCreate.desc.descriptors = SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboCreate.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		uboCreate.desc.name = "MaterialUniformBuffer";
		uboCreate.desc.size = sizeof(MaterialData);
		uboCreate.desc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; // we need to update it every frame
		uboCreate.pData = nullptr;
		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			uboCreate.ppBuffer = &mMaterialUniformBuffer[i];
			add_resource(&uboCreate, nullptr);
		}

		return true;
	}

	void RemoveRenderResource()
	{
		for (int i = 0; i < 6; i++)
		{
			remove_resource(mSkyboxCubeMap[i]);
		}
		remove_resource(mLogoTex);

		remove_resource(mRoomGeo);
		remove_resource(mCubeVertexBuffer);
		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			remove_resource(mRoomUniformBuffer[i]);
			remove_resource(mCubeUniformBuffer[i]);
			remove_resource(mCameraUniformBuffer[i]);
			remove_resource(mMaterialUniformBuffer[i]);
			remove_resource(mLightUniformBuffer[i][0]);
			remove_resource(mLightUniformBuffer[i][1]);
		}

		remove_sampler(mRenderer, mSampler);
		remove_shader(mRenderer, mDefaultShader);
		remove_shader(mRenderer, mLightShader);
		remove_shader(mRenderer, mSkyboxShader);

		exit_resource_loader_interface(mRenderer);

		remove_root_signature(mRenderer, mSkyboxRootSignature);
		remove_root_signature(mRenderer, mRoomRootSignature);
		remove_root_signature(mRenderer, mCubeRootSignature);

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
					if (!gIsGuiFocused)
						gCamera->OnMove({ 0.6f, 0.0f, 0.0f });
					return true;
				},
			nullptr,
			gestureDesc
		};
		add_input_action(&inputAction);

		gestureDesc.triggerBinding = SG_KEY_S;
		inputAction = { SG_GESTURE_LONG_PRESS,
			[](InputActionContext* ctx)
				{
					if (!gIsGuiFocused)
						gCamera->OnMove({ -0.6f, 0.0f, 0.0f });
					return true;
				},
			nullptr,
			gestureDesc
		};
		add_input_action(&inputAction);

		gestureDesc.triggerBinding = SG_KEY_A;
		inputAction = { SG_GESTURE_LONG_PRESS,
			[](InputActionContext* ctx)
				{
					if (!gIsGuiFocused)
						gCamera->OnMove({ 0.0f, -0.6f, 0.0f });
					return true;
				},
			nullptr,
			gestureDesc
		};
		add_input_action(&inputAction);

		gestureDesc.triggerBinding = SG_KEY_D;
		inputAction = { SG_GESTURE_LONG_PRESS,
			[](InputActionContext* ctx)
				{
					if (!gIsGuiFocused)
						gCamera->OnMove({ 0.0f, 0.6f, 0.0f });
					return true;
				},
			nullptr,
			gestureDesc
		};
		add_input_action(&inputAction);

		gestureDesc.triggerBinding = SG_KEY_SPACE;
		inputAction = { SG_GESTURE_LONG_PRESS,
			[](InputActionContext* ctx)
				{
					if (!gIsGuiFocused)
						gCamera->OnMove({ 0.0f, 0.0f, 0.3f });
					return true;
				},
			nullptr,
			gestureDesc
		};
		add_input_action(&inputAction);

		gestureDesc.triggerBinding = SG_KEY_CTRLL;
		inputAction = { SG_GESTURE_LONG_PRESS,
			[](InputActionContext* ctx)
				{
					if (!gIsGuiFocused)
						gCamera->OnMove({ 0.0f, 0.0f, -0.3f });
					return true;
				},
			nullptr,
			gestureDesc
		};
		add_input_action(&inputAction);

		gestureDesc.triggerBinding = SG_MOUSE_LEFT;
		inputAction = { SG_GESTURE_LONG_PRESS,
			[](InputActionContext* ctx)
				{
					if (!gIsGuiFocused)
					{
						Vec2 offset = { ctx->pPosition->x - gLastMousePos.x, ctx->pPosition->y - gLastMousePos.y };
						offset *= get_dpi_scale();
						gCamera->OnRotate(offset);
						auto* window = (WindowDesc*)ctx->pUserData;
						set_enable_capture_input_custom(true, window->clientRect);
					}
					return true;
				},
			mWindow,
			gestureDesc
		};
		add_input_action(&inputAction);

		inputAction = { SG_MOUSE_LEFT,
			[](InputActionContext* ctx)
				{
					if (!gIsGuiFocused)
					{
						if (ctx->phase == INPUT_ACTION_PHASE_CANCELED)
						{
							auto* window = (WindowDesc*)ctx->pUserData;
							set_enable_capture_input_custom(false, window->clientRect);
						}
					}
					return true;
				},
			mWindow
		};
		add_input_action(&inputAction);
	}

	void UpdateResoureces()
	{
		BufferUpdateDesc uboUpdate = {};
		uboUpdate.pBuffer = mRoomUniformBuffer[mCurrentIndex];
		begin_update_resource(&uboUpdate);
		*(UniformBuffer*)uboUpdate.pMappedData = mModelData;
		end_update_resource(&uboUpdate, nullptr);

		uboUpdate.pBuffer = mCubeUniformBuffer[mCurrentIndex];
		begin_update_resource(&uboUpdate);
		*(LightProxyData*)uboUpdate.pMappedData = mLightData;
		end_update_resource(&uboUpdate, nullptr);

		uboUpdate.pBuffer = mLightUniformBuffer[mCurrentIndex][0];
		begin_update_resource(&uboUpdate);
		*(PointLight*)uboUpdate.pMappedData = gDefaultLight1;
		end_update_resource(&uboUpdate, nullptr);

		uboUpdate.pBuffer = mLightUniformBuffer[mCurrentIndex][1];
		begin_update_resource(&uboUpdate);
		*(PointLight*)uboUpdate.pMappedData = gDefaultLight2;
		end_update_resource(&uboUpdate, nullptr);

		uboUpdate.pBuffer = mCameraUniformBuffer[mCurrentIndex];
		begin_update_resource(&uboUpdate);
		*(CameraUbo*)uboUpdate.pMappedData = mCameraData;
		end_update_resource(&uboUpdate, nullptr);

		uboUpdate.pBuffer = mMaterialUniformBuffer[mCurrentIndex];
		begin_update_resource(&uboUpdate);
		*(MaterialData*)uboUpdate.pMappedData = gMaterialData;
		end_update_resource(&uboUpdate, nullptr);
	}
private:
	Renderer* mRenderer = nullptr;
	Queue* mGraphicQueue = nullptr;

	CmdPool* mCmdPools[IMAGE_COUNT] = { 0 };
	Cmd* mCmds[IMAGE_COUNT] = { 0 };

	SwapChain* mSwapChain = nullptr;
	/// this texture is use for getting the current present rt in the swapchain
	RenderTarget* mDepthBuffer = nullptr;

	Fence* mRenderCompleteFences[IMAGE_COUNT] = { 0 };
	Semaphore* mRenderCompleteSemaphores[IMAGE_COUNT] = { 0 };
	Semaphore* mImageAcquiredSemaphore = { 0 };

	Shader* mSkyboxShader = nullptr;
	Shader* mDefaultShader = nullptr;
	Shader* mLightShader = nullptr;
	Texture* mLogoTex = nullptr;
	Texture* mSkyboxCubeMap[6] = { nullptr };
	Sampler* mSampler = nullptr;

	RootSignature* mSkyboxRootSignature = nullptr;
	DescriptorSet* mSkyboxDescriptorTexSet = nullptr;
	DescriptorSet* mSkyboxDescriptorUboSet = nullptr;
	Pipeline* mSkyboxPipeline = nullptr;

	RootSignature* mRoomRootSignature = nullptr;
	DescriptorSet* mRoomUboDescriptorSet = nullptr;
	Pipeline* mDefaultPipeline = nullptr;

	RootSignature* mCubeRootSignature = nullptr;
	DescriptorSet* mCubeUboDescriptorSet = nullptr;
	Pipeline* mLightProxyGeomPipeline = nullptr;

	Geometry* mRoomGeo = nullptr;

	Buffer* mCameraUniformBuffer[IMAGE_COUNT] = { nullptr, nullptr };
	Buffer* mMaterialUniformBuffer[IMAGE_COUNT] = { nullptr, nullptr };
	Buffer* mRoomUniformBuffer[IMAGE_COUNT] = { nullptr, nullptr };
	Buffer* mCubeUniformBuffer[IMAGE_COUNT] = { nullptr, nullptr };
	Buffer* mLightUniformBuffer[IMAGE_COUNT][2] = { nullptr, nullptr, nullptr, nullptr };

	Buffer* mCubeVertexBuffer = nullptr;
	CameraUbo      mCameraData;
	UniformBuffer  mModelData;
	LightProxyData mLightData;

	uint32_t mCurrentIndex = 0;

	// Gui
	UIMiddleware  mUiMiddleware;
	GuiComponent* mMainGui = nullptr;
	GuiComponent* mSecondGui = nullptr;
};

SG_DEFINE_APPLICATION_MAIN(CustomRenderer)