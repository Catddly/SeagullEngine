#include "Seagull.h"
using namespace SG;

#include "CustomLighting/Light.h"
#include "CustomLighting/Material.h"

#include <commdlg.h>

#define IMAGE_COUNT 2

#define COUNT_OF(a) (sizeof(a) / sizeof(a[0]))
#define PI 3.14159265358979323846

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

struct IrradianceConstant
{
	Matrix4 mvp = Matrix4(1.0);
	// integral sampling
	float deltaPhi = (2.0f * float(PI)) / 180.0f;
	float deltaTheta = (0.5f * float(PI)) / 64.0f;
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

Geometry* gDynamicGeo = nullptr;
Geometry* gDeleteGeo  = nullptr;
HWND      gCurrWindowHandle = NULL;
uint32_t  gCurrentIndex = 0;
int32_t   gDeleteIndex = -1;
void LoadGeometry()
{
	if (gDynamicGeo)
	{
		gDeleteGeo = gDynamicGeo;
		gDynamicGeo = nullptr;
		gDeleteIndex = gCurrentIndex;
	}

	VertexLayout geoVertexLayout;
	geoVertexLayout.attribCount = 3;

	geoVertexLayout.attribs[0].semantic = SG_SEMANTIC_POSITION;
	geoVertexLayout.attribs[0].format = TinyImageFormat_R32G32B32_SFLOAT;
	geoVertexLayout.attribs[0].binding = 0;
	geoVertexLayout.attribs[0].location = 0;
	geoVertexLayout.attribs[0].offset = 0;

	geoVertexLayout.attribs[1].semantic = SG_SEMANTIC_TEXCOORD0;
	geoVertexLayout.attribs[1].format = TinyImageFormat_R32G32_SFLOAT;
	geoVertexLayout.attribs[1].binding = 0;
	geoVertexLayout.attribs[1].location = 1;
	geoVertexLayout.attribs[1].offset = 3 * sizeof(float);

	geoVertexLayout.attribs[2].semantic = SG_SEMANTIC_NORMAL;
	geoVertexLayout.attribs[2].format = TinyImageFormat_R32G32B32_SFLOAT;
	geoVertexLayout.attribs[2].binding = 0;
	geoVertexLayout.attribs[2].location = 2;
	geoVertexLayout.attribs[2].offset = 5 * sizeof(float);

	OPENFILENAMEA ofn;        // common dialog box structure
	CHAR szFile[260] = { 0 }; // if using TCHAR macros
	const char* filter = "Model (*.gltf)\0*.gltf\0";
	eastl::string filepath;

	ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = gCurrWindowHandle;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
	if (::GetOpenFileNameA(&ofn) == TRUE)
	{
		//SG_LOG_DEBUG(ofn.lpstrFile);
		filepath = ofn.lpstrFile;
	}
	char token = '/\\';
	size_t lastToken = filepath.find_last_of(token);
	eastl::string fileName = filepath.substr(lastToken + 1, filepath.size() - lastToken);

	GeometryLoadDesc geoCreate = {};
	geoCreate.fileName = fileName.c_str();
	geoCreate.ppGeometry = &gDynamicGeo;
	geoCreate.pVertexLayout = &geoVertexLayout;
	add_resource(&geoCreate, nullptr);

	wait_for_all_resource_loads();
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

		//set_window_size(mWindow, 1368, 912);

		gDefaultLight1.color = { 1.0f, 1.0f, 1.0f };
		gDefaultLight1.intensity = 0.75f;
		gDefaultLight1.position = { 0.0f, 0.0f, 1.0f };
		gDefaultLight1.range = 2.0f;

		gDefaultLight2.color = { 1.0f, 1.0f, 1.0f };
		gDefaultLight2.intensity = 0.45f;
		gDefaultLight2.position = { -0.8f, 0.0f, 0.1f };
		gDefaultLight2.range = 2.2f;

		mSkyboxData.model = glm::rotate(Matrix4(1.0f), glm::radians(270.0f), { 1.0f, 0.0f, 0.0f }) *
			glm::rotate(Matrix4(1.0f), glm::radians(0.0f), { 0.0f, 0.0f, 1.0f }) *
			glm::rotate(Matrix4(1.0f), glm::radians(90.0f), { 0.0f, 1.0f, 0.0f });
		mPbrSamplerData.model = glm::rotate(Matrix4(1.0f), glm::radians(90.0f), { 1.0f, 0.0f, 0.0f }) *
			glm::rotate(Matrix4(1.0f), glm::radians(0.0f), { 0.0f, 1.0f, 0.0f }) *
			glm::rotate(Matrix4(1.0f), glm::radians(270.0f), { 0.0f, 0.0f, -1.0f });

		gCurrWindowHandle = (HWND)mWindow->handle.window;

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
		ButtonWidget loadGeo("Load Model");
		loadGeo.pOnEdited = LoadGeometry;
		mSecondGui->AddWidget(loadGeo);
		mSecondGui->AddWidget(SeparatorWidget());
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
		
		//mSecondGui->AddWidget(SeparatorWidget());
		//mSecondGui->AddWidget(SliderFloatWidget("SkyboxX", &gSkyboxRotateX, 0.0f, 360.0f));
		//mSecondGui->AddWidget(SliderFloatWidget("SkyboxY", &gSkyboxRotateY, 0.0f, 360.0f));
		//mSecondGui->AddWidget(SliderFloatWidget("SkyboxZ", &gSkyboxRotateZ, 0.0f, 360.0f));

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

		GenerateIrradianceCubeMap();
		GeneratePrefilterCubeMap();

		return true;
	}

	virtual void OnExit() override
	{
		wait_queue_idle(mGraphicQueue);

		destroy_camera(gCamera);
		exit_input_system();

		mUiMiddleware.OnExit();

		remove_render_target(mRenderer, mIrradianceCubeMap);
		remove_render_target(mRenderer, mPrefilterCubeMap);
		RemoveRenderResource();
	}

	virtual bool OnLoad() override
	{
		DescriptorSetCreateDesc descriptorSetCreate = { mSkyboxRootSignature, SG_DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		add_descriptor_set(mRenderer, &descriptorSetCreate, &mSkyboxTexDescSet);
		descriptorSetCreate = { mPbrRootSignature, SG_DESCRIPTOR_UPDATE_FREQ_NONE, 2 };
		add_descriptor_set(mRenderer, &descriptorSetCreate, &mModelTexDescSet);
		descriptorSetCreate = { mPbrRootSignature, SG_DESCRIPTOR_UPDATE_FREQ_PER_FRAME, IMAGE_COUNT * 2 };
		add_descriptor_set(mRenderer, &descriptorSetCreate, &mModelUboDescSet);
		descriptorSetCreate = { mLightProxyRootSignature, SG_DESCRIPTOR_UPDATE_FREQ_PER_FRAME, IMAGE_COUNT * 3 };
		add_descriptor_set(mRenderer, &descriptorSetCreate, &mLightProxyGeomUboDescSet);
		descriptorSetCreate = { mSkyboxRootSignature, SG_DESCRIPTOR_UPDATE_FREQ_PER_FRAME, IMAGE_COUNT };
		add_descriptor_set(mRenderer, &descriptorSetCreate, &mSkyboxUboDescSet);

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
		updateData[0].ppTextures = &mSkyboxCubeMap;
		update_descriptor_set(mRenderer, 0, mSkyboxTexDescSet, 1, updateData); // update the cubemap

		DescriptorData modelUpdateData[2] = {};
		modelUpdateData[0].name = "samplerIrradiance";
		modelUpdateData[0].ppTextures = &mIrradianceCubeMap->pTexture;
		modelUpdateData[1].name = "samplerPrefilter";
		modelUpdateData[1].ppTextures = &mPrefilterCubeMap->pTexture;
		update_descriptor_set(mRenderer, 0, mModelTexDescSet, 2, modelUpdateData); // update the cubemap

		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			DescriptorData bufferUpdate[1] = {};
			bufferUpdate[0].name = "camera";
			bufferUpdate[0].ppBuffers = &mCameraUniformBuffer[i];
			update_descriptor_set(mRenderer, i, mSkyboxUboDescSet, 1, bufferUpdate);
		}

		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			DescriptorData bufferUpdate[4] = {};
			bufferUpdate[0].name = "camera";
			bufferUpdate[0].ppBuffers = &mCameraUniformBuffer[i];
			bufferUpdate[1].name = "ubo";
			bufferUpdate[1].ppBuffers = &mRoomUniformBuffer[i];
			bufferUpdate[2].name = "light";
			bufferUpdate[2].ppBuffers = mLightUniformBuffer[i];
			bufferUpdate[2].count = 2;
			bufferUpdate[3].name = "mat";
			bufferUpdate[3].ppBuffers = &mMaterialUniformBuffer[i];
			update_descriptor_set(mRenderer, i, mModelUboDescSet, 4, bufferUpdate);
		}

		for (uint32_t i = 0; i < IMAGE_COUNT; i++)
		{
			DescriptorData bufferUpdate[2] = {};
			bufferUpdate[0].name = "camera";
			bufferUpdate[0].ppBuffers = &mCameraUniformBuffer[i];
			bufferUpdate[1].name = "lightUbo";
			bufferUpdate[1].ppBuffers = &mCubeUniformBuffer[i];
			update_descriptor_set(mRenderer, i, mLightProxyGeomUboDescSet, 2, bufferUpdate);
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
		remove_pipeline(mRenderer, mPbrPipeline);
		remove_swapchain(mRenderer, mSwapChain);

		remove_descriptor_set(mRenderer, mSkyboxTexDescSet);
		remove_descriptor_set(mRenderer, mModelUboDescSet);
		remove_descriptor_set(mRenderer, mLightProxyGeomUboDescSet);

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
		static int direction = 1;
		time += deltaTime;

		if (gIsRotating)
			rotateTime += deltaTime * direction;

		float rotatedDegreed = rotateTime * 90.0f;
		if (rotatedDegreed >= 360.0f || rotatedDegreed < 0.0f)
		{
			direction *= -1;
		}

		mModelData.model = glm::rotate(Matrix4(1.0f), glm::radians(rotatedDegreed), { 0.0f, 0.0f, 1.0f }) *
			glm::scale(Matrix4(1.0f), { 0.3f, 0.3f, 0.3f });

		gDefaultLight1.color = UintToVec4Color(gLightColor1) / 255.0f;
		gDefaultLight1.range = 1.0f / eastl::max(glm::pow(gLightRange1, 2.0f), 0.000001f);
		gDefaultLight2.color = UintToVec4Color(gLightColor2) / 255.0f;
		gDefaultLight2.range = 1.0f / eastl::max(glm::pow(gLightRange2, 2.0f), 0.000001f);

		mLightData.model[0] = glm::translate(Matrix4(1.0f), gDefaultLight1.position) * glm::scale(Matrix4(1.0f), { 0.005f, 0.005f, 0.005f });
		mLightData.model[1] = glm::translate(Matrix4(1.0f), gDefaultLight2.position) * glm::scale(Matrix4(1.0f), { 0.005f, 0.005f, 0.005f });
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
		Semaphore* renderCompleteSemaphore = mRenderCompleteSemaphores[gCurrentIndex];
		Fence* renderCompleteFence = mRenderCompleteFences[gCurrentIndex];

		FenceStatus fenceStatus;
		get_fence_status(mRenderer, renderCompleteFence, &fenceStatus);
		if (fenceStatus == SG_FENCE_STATUS_INCOMPLETE)
			wait_for_fences(mRenderer, 1, &renderCompleteFence);

		// reset command pool for this frame 
		reset_command_pool(mRenderer, mCmdPools[gCurrentIndex]);

		Cmd* cmd = mCmds[gCurrentIndex];

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

			/// geom start
			cmd_bind_push_constants(cmd, mPbrRootSignature, "pushConsts", &mPbrSamplerData);
			cmd_bind_pipeline(cmd, mPbrPipeline);
			cmd_bind_descriptor_set(cmd, 0, mModelTexDescSet);
			cmd_bind_descriptor_set(cmd, gCurrentIndex, mModelUboDescSet);

			cmd_bind_index_buffer(cmd, gDynamicGeo->pIndexBuffer, gDynamicGeo->indexType, 0);
			Buffer* vertexBuffer[] = { gDynamicGeo->pVertexBuffers[0] };
			cmd_bind_vertex_buffer(cmd, 1, vertexBuffer, gDynamicGeo->vertexStrides, nullptr);

			for (uint32_t i = 0; i < gDynamicGeo->drawArgCount; i++)
			{
				IndirectDrawIndexArguments& cmdDraw = gDynamicGeo->pDrawArgs[i];
				cmd_draw_indexed(cmd, cmdDraw.indexCount, cmdDraw.startIndex, cmdDraw.vertexOffset);
			}
			/// geom end
			
			/// skybox start
			cmd_set_viewport(cmd, 0.0f, 0.0f, (float)renderTarget->width, (float)renderTarget->height, 1.0f, 1.0f);
			cmd_bind_push_constants(cmd, mSkyboxRootSignature, "pushConsts", &mSkyboxData);
			cmd_bind_pipeline(cmd, mSkyboxPipeline);
			cmd_bind_descriptor_set(cmd, 0, mSkyboxTexDescSet); // just the skybox texture
			cmd_bind_descriptor_set(cmd, gCurrentIndex, mSkyboxUboDescSet);

			cmd_bind_index_buffer(cmd, mSkyboxGeo->pIndexBuffer, mSkyboxGeo->indexType, 0);
			Buffer* skyboxVertexBuffer[] = { mSkyboxGeo->pVertexBuffers[0] };
			cmd_bind_vertex_buffer(cmd, 1, skyboxVertexBuffer, mSkyboxGeo->vertexStrides, nullptr);

			for (uint32_t i = 0; i < mSkyboxGeo->drawArgCount; i++)
			{
				IndirectDrawIndexArguments& cmdDraw = mSkyboxGeo->pDrawArgs[i];
				cmd_draw_indexed(cmd, cmdDraw.indexCount, cmdDraw.startIndex, cmdDraw.vertexOffset);
			}
			cmd_set_viewport(cmd, 0.0f, 0.0f, (float)renderTarget->width, (float)renderTarget->height, 0.0f, 1.0f);
			/// skybox end

			if (gDrawLightProxyGeom)
			{
				/// light proxy start
				cmd_bind_pipeline(cmd, mLightProxyGeomPipeline);
				cmd_bind_descriptor_set(cmd, gCurrentIndex, mLightProxyGeomUboDescSet);

				cmd_bind_index_buffer(cmd, mSkyboxGeo->pIndexBuffer, mSkyboxGeo->indexType, 0);
				Buffer* skyboxVertexBuffer[] = { mSkyboxGeo->pVertexBuffers[0] };
				cmd_bind_vertex_buffer(cmd, 1, skyboxVertexBuffer, mSkyboxGeo->vertexStrides, nullptr);

				for (uint32_t i = 0; i < mSkyboxGeo->drawArgCount; i++)
				{
					IndirectDrawIndexArguments& cmdDraw = mSkyboxGeo->pDrawArgs[i];
					cmd_draw_indexed_instanced(cmd, mSkyboxGeo->indexCount, 0, 2, 0, 0);
				}
				/// light proxy end
			}
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

		if (gDeleteGeo && gDeleteIndex != -1 && ((gDeleteIndex + 1) % IMAGE_COUNT) == gCurrentIndex)
		{
			remove_resource(gDeleteGeo);
			gDeleteGeo = nullptr;
			gDeleteIndex = -1;
		}

		gCurrentIndex = (gCurrentIndex + 1) % IMAGE_COUNT;

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

		graphicPipe.pRootSignature = mPbrRootSignature;
		graphicPipe.pShaderProgram = mPbrShader;

		graphicPipe.pVertexLayout = &vertexLayout;
		graphicPipe.pRasterizerState = &rasterizeState;

		graphicPipe.depthStencilFormat = mDepthBuffer->format;

		graphicPipe.pBlendState = &blendStateDesc;
		add_pipeline(mRenderer, &pipelineCreate, &mPbrPipeline);

		return mPbrPipeline != nullptr;
	}

	bool CreateLightpassPipeline()
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

		graphicPipe.pRootSignature = mLightProxyRootSignature;
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
		rasterizeState.cullMode = SG_CULL_MODE_NONE;
		//rasterizeState.frontFace = SG_FRONT_FACE_CCW;

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

		graphicPipe.pRootSignature = mSkyboxRootSignature;
		graphicPipe.pShaderProgram = mSkyboxShader;

		graphicPipe.pVertexLayout = &vertexLayout;
		graphicPipe.pRasterizerState = &rasterizeState;

		graphicPipe.depthStencilFormat = mDepthBuffer->format;

		graphicPipe.pBlendState = &blendStateDesc;
		add_pipeline(mRenderer, &pipelineCreate, &mSkyboxPipeline);

		return mSkyboxPipeline != nullptr;
	}

	bool CreateIrradiancePipeline()
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
		rasterizeState.cullMode = SG_CULL_MODE_NONE;
		//rasterizeState.frontFace = SG_FRONT_FACE_CCW;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.depthTest = false;
		depthStateDesc.depthWrite = false;
		depthStateDesc.depthFunc = SG_COMPARE_MODE_LEQUAL;

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.srcFactors[0] = SG_BLEND_CONST_ONE;
		blendStateDesc.srcAlphaFactors[0] = SG_BLEND_CONST_ONE;
		blendStateDesc.renderTargetMask = SG_BLEND_STATE_TARGET_0;
		blendStateDesc.masks[0] = SG_BLEND_COLOR_MASK_ALL;

		PipelineCreateDesc pipelineCreate = {};
		pipelineCreate.type = SG_PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& graphicPipe = pipelineCreate.graphicsDesc;
		graphicPipe.primitiveTopo = SG_PRIMITIVE_TOPO_TRI_LIST;
		graphicPipe.renderTargetCount = 1;

		graphicPipe.pColorFormats = &mIrradianceCubeBuffer->format;
		graphicPipe.pDepthState = &depthStateDesc;

		graphicPipe.sampleCount = mIrradianceCubeBuffer->sampleCount;
		graphicPipe.sampleQuality = mIrradianceCubeBuffer->sampleQuality;

		graphicPipe.pRootSignature = mIrradianceRootSignature;
		graphicPipe.pShaderProgram = mIrradianceShader;

		graphicPipe.pVertexLayout = &vertexLayout;
		graphicPipe.pRasterizerState = &rasterizeState;

		graphicPipe.pBlendState = &blendStateDesc;
		add_pipeline(mRenderer, &pipelineCreate, &mIrradianceCubePipeline);

		return mIrradianceCubePipeline != nullptr;
	}

	bool CreatePrefilterPipeline()
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
		rasterizeState.cullMode = SG_CULL_MODE_NONE;
		//rasterizeState.frontFace = SG_FRONT_FACE_CCW;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.depthTest = false;
		depthStateDesc.depthWrite = false;
		depthStateDesc.depthFunc = SG_COMPARE_MODE_LEQUAL;

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.srcFactors[0] = SG_BLEND_CONST_ONE;
		blendStateDesc.srcAlphaFactors[0] = SG_BLEND_CONST_ONE;
		blendStateDesc.renderTargetMask = SG_BLEND_STATE_TARGET_0;
		blendStateDesc.masks[0] = SG_BLEND_COLOR_MASK_ALL;

		PipelineCreateDesc pipelineCreate = {};
		pipelineCreate.type = SG_PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& graphicPipe = pipelineCreate.graphicsDesc;
		graphicPipe.primitiveTopo = SG_PRIMITIVE_TOPO_TRI_LIST;
		graphicPipe.renderTargetCount = 1;

		graphicPipe.pColorFormats = &mPrefilterCubeBuffer->format;
		graphicPipe.pDepthState = &depthStateDesc;

		graphicPipe.sampleCount = mPrefilterCubeBuffer->sampleCount;
		graphicPipe.sampleQuality = mPrefilterCubeBuffer->sampleQuality;

		graphicPipe.pRootSignature = mPrefilterRootSignature;
		graphicPipe.pShaderProgram = mPrefilterShader;

		graphicPipe.pVertexLayout = &vertexLayout;
		graphicPipe.pRasterizerState = &rasterizeState;

		graphicPipe.pBlendState = &blendStateDesc;
		add_pipeline(mRenderer, &pipelineCreate, &mPrefilterCubePipeline);

		return mPrefilterCubePipeline != nullptr;
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

	bool CreateIrradianceBuffer()
	{
		RenderTargetCreateDesc irradianceCubeRT = {};
		irradianceCubeRT.arraySize = 1;
		irradianceCubeRT.clearValue = { { 1.0f, 0 } };
		irradianceCubeRT.depth = 1;
		irradianceCubeRT.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE;
		irradianceCubeRT.format = TinyImageFormat_R32G32B32A32_SFLOAT;
		irradianceCubeRT.startState = SG_RESOURCE_STATE_COPY_SOURCE;
		irradianceCubeRT.width = 64;
		irradianceCubeRT.height = 64;
		irradianceCubeRT.mipLevels = 1;
		irradianceCubeRT.sampleCount = SG_SAMPLE_COUNT_1;
		irradianceCubeRT.sampleQuality = 0;
		irradianceCubeRT.name = "IrradianceCubeBuffer";

		add_render_target(mRenderer, &irradianceCubeRT, &mIrradianceCubeBuffer);
		return mIrradianceCubeBuffer != nullptr;
	}

	bool CreatePrefilterBuffer()
	{
		RenderTargetCreateDesc prefilterCubeRT = {};
		prefilterCubeRT.arraySize = 1;
		prefilterCubeRT.clearValue = { { 1.0f, 0 } };
		prefilterCubeRT.depth = 1;
		prefilterCubeRT.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE;
		prefilterCubeRT.format = TinyImageFormat_R16G16B16A16_SFLOAT;
		prefilterCubeRT.startState = SG_RESOURCE_STATE_COPY_SOURCE;
		prefilterCubeRT.width = 512;
		prefilterCubeRT.height = 512;
		prefilterCubeRT.mipLevels = 1;
		prefilterCubeRT.sampleCount = SG_SAMPLE_COUNT_1;
		prefilterCubeRT.sampleQuality = 0;
		prefilterCubeRT.name = "PrefilterCubeBuffer";

		add_render_target(mRenderer, &prefilterCubeRT, &mPrefilterCubeBuffer);
		return mPrefilterCubeBuffer != nullptr;
	}

	bool CreateIrradianceCubeMap()
	{
		const int32_t dim = 64;
		const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

		RenderTargetCreateDesc irradianceCubeRT = {};
		irradianceCubeRT.arraySize = 6;
		irradianceCubeRT.clearValue = { { 1.0f, 0 } };
		irradianceCubeRT.depth = 1;
		irradianceCubeRT.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE_CUBE;
		irradianceCubeRT.format = TinyImageFormat_R32G32B32A32_SFLOAT;
		irradianceCubeRT.startState = SG_RESOURCE_STATE_SHADER_RESOURCE;
		irradianceCubeRT.width = dim;
		irradianceCubeRT.height = dim;
		irradianceCubeRT.mipLevels = numMips;
		irradianceCubeRT.sampleCount = SG_SAMPLE_COUNT_1;
		irradianceCubeRT.sampleQuality = 0;
		irradianceCubeRT.name = "IrradianceCubeMap";

		add_render_target(mRenderer, &irradianceCubeRT, &mIrradianceCubeMap);
		return mIrradianceCubeMap != nullptr;
	}

	bool CreatePrefilterCubeMap()
	{
		const int32_t dim = 512;
		const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

		RenderTargetCreateDesc prefilterCubeRT = {};
		prefilterCubeRT.arraySize = 6;
		prefilterCubeRT.clearValue = { { 1.0f, 0 } };
		prefilterCubeRT.depth = 1;
		prefilterCubeRT.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE_CUBE;
		prefilterCubeRT.format = TinyImageFormat_R16G16B16A16_SFLOAT;
		prefilterCubeRT.startState = SG_RESOURCE_STATE_SHADER_RESOURCE;
		prefilterCubeRT.width = dim;
		prefilterCubeRT.height = dim;
		prefilterCubeRT.mipLevels = numMips;
		prefilterCubeRT.sampleCount = SG_SAMPLE_COUNT_1;
		prefilterCubeRT.sampleQuality = 0;
		prefilterCubeRT.name = "PrefilterCubeMap";

		add_render_target(mRenderer, &prefilterCubeRT, &mPrefilterCubeMap);
		return mPrefilterCubeMap != nullptr;
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

		TextureLoadDesc texCubeMapLoad = {};
		texCubeMapLoad.fileName = "pisa_cube";
		texCubeMapLoad.container = SG_TEXTURE_CONTAINER_KTX;
		texCubeMapLoad.ppTexture = &mSkyboxCubeMap;
		add_resource(&texCubeMapLoad, nullptr);

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
		geoCreate.fileName = "model.gltf";
		geoCreate.ppGeometry = &gDynamicGeo;
		geoCreate.pVertexLayout = &roomGeoVertexLayout;
		add_resource(&geoCreate, nullptr);

		geoCreate = {};
		geoCreate.fileName = "cube.gltf";
		geoCreate.ppGeometry = &mSkyboxGeo;
		geoCreate.pVertexLayout = &roomGeoVertexLayout;
		add_resource(&geoCreate, nullptr);

		ShaderLoadDesc loadBasicShader = {};
		loadBasicShader.stages[0] = { "pbr.vert", nullptr, 0, "main" };
		loadBasicShader.stages[1] = { "pbr.frag", nullptr, 0, "main" };
		add_shader(mRenderer, &loadBasicShader, &mPbrShader);

		loadBasicShader.stages[0] = { "LightProxy/lightGeo.vert", nullptr, 0, "main" };
		loadBasicShader.stages[1] = { "LightProxy/lightGeo.frag", nullptr, 0, "main" };
		add_shader(mRenderer, &loadBasicShader, &mLightShader);

		loadBasicShader.stages[0] = { "skybox.vert", nullptr, 0, "main" };
		loadBasicShader.stages[1] = { "skybox.frag", nullptr, 0, "main" };
		add_shader(mRenderer, &loadBasicShader, &mSkyboxShader);

		SamplerCreateDesc samplerCreate = {};
		samplerCreate.addressU = SG_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreate.addressV = SG_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreate.addressW = SG_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreate.minFilter = SG_FILTER_LINEAR;
		samplerCreate.magFilter = SG_FILTER_LINEAR;
		samplerCreate.mipMapMode = SG_MIPMAP_MODE_LINEAR;
		add_sampler(mRenderer, &samplerCreate, &mSampler);

		Shader* submitSkyboxShader[] = { mSkyboxShader };
		const char* staticSamplers[] = { "skyboxCubeMap" };
		RootSignatureCreateDesc rootSignatureCreate = {};
		rootSignatureCreate.staticSamplerCount = 1;
		rootSignatureCreate.ppStaticSamplers = &mSampler;
		rootSignatureCreate.ppStaticSamplerNames = staticSamplers;
		rootSignatureCreate.ppShaders = submitSkyboxShader;
		rootSignatureCreate.shaderCount = COUNT_OF(submitSkyboxShader);
		add_root_signature(mRenderer, &rootSignatureCreate, &mSkyboxRootSignature);

		Shader* submitShaders[] = { mPbrShader };
		const char* modelStaticSamplers[] = { "samplerIrradiance", "samplerPrefilter" };
		Sampler* submitSamplers[] = { mSampler, mSampler };
		rootSignatureCreate = {};
		rootSignatureCreate.staticSamplerCount = 2;
		rootSignatureCreate.ppStaticSamplers = submitSamplers;
		rootSignatureCreate.ppStaticSamplerNames = modelStaticSamplers;
		rootSignatureCreate.ppShaders = submitShaders;
		rootSignatureCreate.shaderCount = COUNT_OF(submitShaders);
		add_root_signature(mRenderer, &rootSignatureCreate, &mPbrRootSignature);

		submitShaders[0] = { mLightShader };
		rootSignatureCreate.ppShaders = submitShaders;
		rootSignatureCreate.shaderCount = COUNT_OF(submitShaders);
		add_root_signature(mRenderer, &rootSignatureCreate, &mLightProxyRootSignature);

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
		remove_resource(mSkyboxCubeMap);

		remove_resource(mLogoTex);

		remove_resource(mSkyboxGeo);
		remove_resource(gDynamicGeo);
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
		remove_shader(mRenderer, mPbrShader);
		remove_shader(mRenderer, mLightShader);
		remove_shader(mRenderer, mSkyboxShader);

		exit_resource_loader_interface(mRenderer);

		remove_root_signature(mRenderer, mSkyboxRootSignature);
		remove_root_signature(mRenderer, mPbrRootSignature);
		remove_root_signature(mRenderer, mLightProxyRootSignature);

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
		uboUpdate.pBuffer = mRoomUniformBuffer[gCurrentIndex];
		begin_update_resource(&uboUpdate);
		*(UniformBuffer*)uboUpdate.pMappedData = mModelData;
		end_update_resource(&uboUpdate, nullptr);

		uboUpdate.pBuffer = mCubeUniformBuffer[gCurrentIndex];
		begin_update_resource(&uboUpdate);
		*(LightProxyData*)uboUpdate.pMappedData = mLightData;
		end_update_resource(&uboUpdate, nullptr);

		uboUpdate.pBuffer = mLightUniformBuffer[gCurrentIndex][0];
		begin_update_resource(&uboUpdate);
		*(PointLight*)uboUpdate.pMappedData = gDefaultLight1;
		end_update_resource(&uboUpdate, nullptr);

		uboUpdate.pBuffer = mLightUniformBuffer[gCurrentIndex][1];
		begin_update_resource(&uboUpdate);
		*(PointLight*)uboUpdate.pMappedData = gDefaultLight2;
		end_update_resource(&uboUpdate, nullptr);

		uboUpdate.pBuffer = mCameraUniformBuffer[gCurrentIndex];
		begin_update_resource(&uboUpdate);
		*(CameraUbo*)uboUpdate.pMappedData = mCameraData;
		end_update_resource(&uboUpdate, nullptr);

		uboUpdate.pBuffer = mMaterialUniformBuffer[gCurrentIndex];
		begin_update_resource(&uboUpdate);
		*(MaterialData*)uboUpdate.pMappedData = gMaterialData;
		end_update_resource(&uboUpdate, nullptr);
	}

	void GenerateIrradianceCubeMap()
	{
		const int32_t dim = 64;
		const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

		eastl::vector<Matrix4> matrices = {
			// POSITIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f + 180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f + 180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		};

		ShaderLoadDesc loadBasicShader = {};
		loadBasicShader.stages[0] = { "irradianceCube.vert", nullptr, 0, "main" };
		loadBasicShader.stages[1] = { "irradianceCube.frag", nullptr, 0, "main" };
		add_shader(mRenderer, &loadBasicShader, &mIrradianceShader);

		Shader* submitShader[] = { mIrradianceShader };
		const char* staticSamplers[] = { "skyboxCubeMap" };
		RootSignatureCreateDesc rootSignatureCreate = {};
		rootSignatureCreate.staticSamplerCount = 1;
		rootSignatureCreate.ppStaticSamplers = &mSampler;
		rootSignatureCreate.ppStaticSamplerNames = staticSamplers;
		rootSignatureCreate.ppShaders = submitShader;
		rootSignatureCreate.shaderCount = COUNT_OF(submitShader);
		add_root_signature(mRenderer, &rootSignatureCreate, &mIrradianceRootSignature);

		DescriptorSetCreateDesc descriptorSetCreate = { mIrradianceRootSignature, SG_DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		add_descriptor_set(mRenderer, &descriptorSetCreate, &mIrradianceTexDescSet);

		wait_for_all_resource_loads();

		DescriptorData updateData[2] = {};
		updateData[0].name = "skyboxCubeMap";
		updateData[0].ppTextures = &mSkyboxCubeMap;
		update_descriptor_set(mRenderer, 0, mIrradianceTexDescSet, 1, updateData); // update the cubemap

		CreateIrradianceBuffer();
		CreateIrradianceCubeMap();
		CreateIrradiancePipeline();

		/// begin draw
		FenceStatus fenceStatus;

		reset_command_pool(mRenderer, mCmdPools[0]);

		Cmd* tempCmd = nullptr;
		CmdCreateDesc cmdCreate = {};
		cmdCreate.pPool = mCmdPools[0];
		add_cmd(mRenderer, &cmdCreate, &tempCmd);

		IrradianceConstant pushConsts;

		// begin command buffer
		begin_cmd(tempCmd);

		RenderTargetBarrier renderTargetBarriers;

		LoadActionsDesc loadAction = {};
		loadAction.loadActionsColor[0] = SG_LOAD_ACTION_CLEAR;
		loadAction.clearColorValues[0].r = 0.0f;
		loadAction.clearColorValues[0].g = 0.0f;
		loadAction.clearColorValues[0].b = 0.0f;
		loadAction.clearColorValues[0].a = 1.0f;

		//loadAction.loadActionDepth = SG_LOAD_ACTION_LOAD;
		//loadAction.clearDepth.depth = 1.0f;
		//loadAction.clearDepth.stencil = 0;

		renderTargetBarriers = { mIrradianceCubeMap, SG_RESOURCE_STATE_SHADER_RESOURCE, SG_RESOURCE_STATE_COPY_DEST };
		cmd_resource_barrier(tempCmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarriers);

		for (int i = 0; i < numMips; i++) // for each mip-level
		{
			for (int j = 0; j < 6; j++) // for each side of the cube map
			{
				renderTargetBarriers = { mIrradianceCubeBuffer, SG_RESOURCE_STATE_COPY_SOURCE, SG_RESOURCE_STATE_RENDER_TARGET };
				cmd_resource_barrier(tempCmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarriers);

				auto width = static_cast<float>(dim * std::pow(0.5f, i));
				auto height = static_cast<float>(dim * std::pow(0.5f, i));
				cmd_set_viewport(tempCmd, 0, 0, width, height, 0.0f, 1.0f);
				cmd_set_scissor(tempCmd, 0, 0, width, height);

				cmd_bind_render_targets(tempCmd, 1, &mIrradianceCubeBuffer, nullptr, &loadAction, nullptr, nullptr, -1, -1);
					pushConsts.mvp = glm::perspective((float)(PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[j];
					cmd_bind_push_constants(tempCmd, mIrradianceRootSignature, "pushConsts", &pushConsts);

					cmd_bind_pipeline(tempCmd, mIrradianceCubePipeline);
					cmd_bind_descriptor_set(tempCmd, 0, mIrradianceTexDescSet);

					cmd_bind_index_buffer(tempCmd, mSkyboxGeo->pIndexBuffer, mSkyboxGeo->indexType, 0);
					Buffer* skyboxVertexBuffer[] = { mSkyboxGeo->pVertexBuffers[0] };
					cmd_bind_vertex_buffer(tempCmd, 1, skyboxVertexBuffer, mSkyboxGeo->vertexStrides, nullptr);

					for (uint32_t i = 0; i < mSkyboxGeo->drawArgCount; i++)
					{
						IndirectDrawIndexArguments& cmdDraw = mSkyboxGeo->pDrawArgs[i];
						cmd_draw_indexed(tempCmd, cmdDraw.indexCount, cmdDraw.startIndex, cmdDraw.vertexOffset);
					}
				cmd_bind_render_targets(tempCmd, 0, nullptr, nullptr, nullptr, nullptr, nullptr, -1, -1);

				renderTargetBarriers = { mIrradianceCubeBuffer, SG_RESOURCE_STATE_RENDER_TARGET, SG_RESOURCE_STATE_COPY_SOURCE };
				cmd_resource_barrier(tempCmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarriers);
				
				CopyImageDesc imageBlit = {};
				imageBlit.arrayLayer = j;
				imageBlit.layerCount = 1;
				imageBlit.mipLevel = i;
				imageBlit.offset = 0;
				imageBlit.width = static_cast<uint32_t>(width);
				imageBlit.height = static_cast<uint32_t>(height);
				imageBlit.depth = 1;
				cmd_image_blit(tempCmd, mIrradianceCubeBuffer->pTexture, mIrradianceCubeMap->pTexture, &imageBlit);
			}
		}

		renderTargetBarriers = { mIrradianceCubeMap, SG_RESOURCE_STATE_COPY_DEST, SG_RESOURCE_STATE_SHADER_RESOURCE };
		cmd_resource_barrier(tempCmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarriers);

		end_cmd(tempCmd);

		Fence* tempFence = nullptr;
		add_fence(mRenderer, &tempFence);

		QueueSubmitDesc submitDesc = {};
		submitDesc.cmdCount = 1;
		submitDesc.signalSemaphoreCount = 0;
		submitDesc.waitSemaphoreCount = 0;
		submitDesc.ppCmds = &tempCmd;
		//submitDesc.ppSignalSemaphores = &renderCompleteSemaphore;
		//submitDesc.ppWaitSemaphores = &mImageAcquiredSemaphore;
		submitDesc.pSignalFence = tempFence;
		queue_submit(mGraphicQueue, &submitDesc);

		wait_for_fences(mRenderer, 1, &tempFence);
		remove_fence(mRenderer, tempFence);
		/// end draw

		remove_cmd(mRenderer, tempCmd);
		remove_descriptor_set(mRenderer, mIrradianceTexDescSet);
		remove_root_signature(mRenderer, mIrradianceRootSignature);
		remove_shader(mRenderer, mIrradianceShader);
		remove_render_target(mRenderer, mIrradianceCubeBuffer);
		remove_pipeline(mRenderer, mIrradianceCubePipeline);
	}

	void GeneratePrefilterCubeMap()
	{
		const int32_t dim = 512;
		const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

		struct PushLauout
		{
			Matrix4 mvp = Matrix4(1.0);
			float roughness = 0.0f;
			float numSamples = 32.0f;
		};

		eastl::vector<Matrix4> matrices = {
			// POSITIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f + 180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f + 180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		};

		ShaderLoadDesc loadBasicShader = {};
		loadBasicShader.stages[0] = { "prefilterCube.vert", nullptr, 0, "main" };
		loadBasicShader.stages[1] = { "prefilterCube.frag", nullptr, 0, "main" };
		add_shader(mRenderer, &loadBasicShader, &mPrefilterShader);

		Shader* submitShader[] = { mPrefilterShader };
		const char* staticSamplers[] = { "skyboxCubeMap" };
		RootSignatureCreateDesc rootSignatureCreate = {};
		rootSignatureCreate.staticSamplerCount = 1;
		rootSignatureCreate.ppStaticSamplers = &mSampler;
		rootSignatureCreate.ppStaticSamplerNames = staticSamplers;
		rootSignatureCreate.ppShaders = submitShader;
		rootSignatureCreate.shaderCount = COUNT_OF(submitShader);
		add_root_signature(mRenderer, &rootSignatureCreate, &mPrefilterRootSignature);

		DescriptorSetCreateDesc descriptorSetCreate = { mPrefilterRootSignature, SG_DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		add_descriptor_set(mRenderer, &descriptorSetCreate, &mPrefilterTexDescSet);

		wait_for_all_resource_loads();

		DescriptorData updateData[2] = {};
		updateData[0].name = "skyboxCubeMap";
		updateData[0].ppTextures = &mSkyboxCubeMap;
		update_descriptor_set(mRenderer, 0, mPrefilterTexDescSet, 1, updateData); // update the cubemap

		CreatePrefilterBuffer();
		CreatePrefilterCubeMap();
		CreatePrefilterPipeline();

		/// begin draw
		FenceStatus fenceStatus;

		reset_command_pool(mRenderer, mCmdPools[0]);

		Cmd* tempCmd = nullptr;
		CmdCreateDesc cmdCreate = {};
		cmdCreate.pPool = mCmdPools[0];
		add_cmd(mRenderer, &cmdCreate, &tempCmd);

		PushLauout pushConsts;

		// begin command buffer
		begin_cmd(tempCmd);

		RenderTargetBarrier renderTargetBarriers;

		LoadActionsDesc loadAction = {};
		loadAction.loadActionsColor[0] = SG_LOAD_ACTION_CLEAR;
		loadAction.clearColorValues[0].r = 0.0f;
		loadAction.clearColorValues[0].g = 0.0f;
		loadAction.clearColorValues[0].b = 0.0f;
		loadAction.clearColorValues[0].a = 1.0f;

		renderTargetBarriers = { mPrefilterCubeMap, SG_RESOURCE_STATE_SHADER_RESOURCE, SG_RESOURCE_STATE_COPY_DEST };
		cmd_resource_barrier(tempCmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarriers);

		for (int i = 0; i < numMips; i++) // for each mip-level
		{
			pushConsts.roughness = (float)i / (float)(numMips - 1);
			for (int j = 0; j < 6; j++) // for each side of the cube map
			{
				renderTargetBarriers = { mPrefilterCubeBuffer, SG_RESOURCE_STATE_COPY_SOURCE, SG_RESOURCE_STATE_RENDER_TARGET };
				cmd_resource_barrier(tempCmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarriers);

				auto width = static_cast<float>(dim * std::pow(0.5f, i));
				auto height = static_cast<float>(dim * std::pow(0.5f, i));
				cmd_set_viewport(tempCmd, 0, 0, width, height, 0.0f, 1.0f);
				cmd_set_scissor(tempCmd, 0, 0, width, height);

				cmd_bind_render_targets(tempCmd, 1, &mPrefilterCubeBuffer, nullptr, &loadAction, nullptr, nullptr, -1, -1);
					pushConsts.mvp = glm::perspective((float)(PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[j];
					cmd_bind_push_constants(tempCmd, mPrefilterRootSignature, "pushConsts", &pushConsts);

					cmd_bind_pipeline(tempCmd, mPrefilterCubePipeline);
					cmd_bind_descriptor_set(tempCmd, 0, mPrefilterTexDescSet);

					cmd_bind_index_buffer(tempCmd, mSkyboxGeo->pIndexBuffer, mSkyboxGeo->indexType, 0);
					Buffer* skyboxVertexBuffer[] = { mSkyboxGeo->pVertexBuffers[0] };
					cmd_bind_vertex_buffer(tempCmd, 1, skyboxVertexBuffer, mSkyboxGeo->vertexStrides, nullptr);

					for (uint32_t i = 0; i < mSkyboxGeo->drawArgCount; i++)
					{
						IndirectDrawIndexArguments& cmdDraw = mSkyboxGeo->pDrawArgs[i];
						cmd_draw_indexed(tempCmd, cmdDraw.indexCount, cmdDraw.startIndex, cmdDraw.vertexOffset);
					}
				cmd_bind_render_targets(tempCmd, 0, nullptr, nullptr, nullptr, nullptr, nullptr, -1, -1);

				renderTargetBarriers = { mPrefilterCubeBuffer, SG_RESOURCE_STATE_RENDER_TARGET, SG_RESOURCE_STATE_COPY_SOURCE };
				cmd_resource_barrier(tempCmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarriers);

				CopyImageDesc imageBlit = {};
				imageBlit.arrayLayer = j;
				imageBlit.layerCount = 1;
				imageBlit.mipLevel = i;
				imageBlit.offset = 0;
				imageBlit.width = static_cast<uint32_t>(width);
				imageBlit.height = static_cast<uint32_t>(height);
				imageBlit.depth = 1;
				cmd_image_blit(tempCmd, mPrefilterCubeBuffer->pTexture, mPrefilterCubeMap->pTexture, &imageBlit);
			}
		}

		renderTargetBarriers = { mPrefilterCubeMap, SG_RESOURCE_STATE_COPY_DEST, SG_RESOURCE_STATE_SHADER_RESOURCE };
		cmd_resource_barrier(tempCmd, 0, nullptr, 0, nullptr, 1, &renderTargetBarriers);

		end_cmd(tempCmd);

		Fence* tempFence = nullptr;
		add_fence(mRenderer, &tempFence);

		QueueSubmitDesc submitDesc = {};
		submitDesc.cmdCount = 1;
		submitDesc.signalSemaphoreCount = 0;
		submitDesc.waitSemaphoreCount = 0;
		submitDesc.ppCmds = &tempCmd;
		submitDesc.pSignalFence = tempFence;
		queue_submit(mGraphicQueue, &submitDesc);

		wait_for_fences(mRenderer, 1, &tempFence);
		remove_fence(mRenderer, tempFence);
		/// end draw

		remove_cmd(mRenderer, tempCmd);
		remove_descriptor_set(mRenderer, mPrefilterTexDescSet);
		remove_root_signature(mRenderer, mPrefilterRootSignature);
		remove_shader(mRenderer, mPrefilterShader);
		remove_render_target(mRenderer, mPrefilterCubeBuffer);
		remove_pipeline(mRenderer, mPrefilterCubePipeline);
	}
private:
	Renderer* mRenderer = nullptr;
	Queue* mGraphicQueue = nullptr;

	CmdPool* mCmdPools[IMAGE_COUNT] = { 0 };
	Cmd* mCmds[IMAGE_COUNT] = { 0 };

	SwapChain* mSwapChain = nullptr;
	/// this texture is use for getting the current present rt in the swapchain
	RenderTarget* mDepthBuffer = nullptr;

	RenderTarget* mIrradianceCubeBuffer = nullptr;
	RootSignature* mIrradianceRootSignature = nullptr;
	DescriptorSet* mIrradianceTexDescSet = nullptr;
	Pipeline* mIrradianceCubePipeline = nullptr;
	RenderTarget* mPrefilterCubeBuffer = nullptr;
	RootSignature* mPrefilterRootSignature = nullptr;
	DescriptorSet* mPrefilterTexDescSet = nullptr;
	Pipeline* mPrefilterCubePipeline = nullptr;

	Fence* mRenderCompleteFences[IMAGE_COUNT] = { 0 };
	Semaphore* mRenderCompleteSemaphores[IMAGE_COUNT] = { 0 };
	Semaphore* mImageAcquiredSemaphore = { 0 };

	Shader* mSkyboxShader = nullptr;
	Shader* mPbrShader = nullptr;
	Shader* mLightShader = nullptr;
	Shader* mIrradianceShader = nullptr;
	Shader* mPrefilterShader = nullptr;
	Texture* mLogoTex = nullptr;

	// combined image sampler
	Sampler* mSampler = nullptr;
	Texture* mSkyboxCubeMap = nullptr;
	RenderTarget* mIrradianceCubeMap = nullptr; // for ambient
	RenderTarget* mPrefilterCubeMap = nullptr;  // for specular

	RootSignature* mSkyboxRootSignature = nullptr;
	DescriptorSet* mSkyboxUboDescSet = nullptr;
	DescriptorSet* mSkyboxTexDescSet = nullptr;
	Pipeline* mSkyboxPipeline = nullptr;

	RootSignature* mPbrRootSignature = nullptr;
	DescriptorSet* mModelUboDescSet = nullptr;
	DescriptorSet* mModelTexDescSet = nullptr;
	Pipeline* mPbrPipeline = nullptr;

	RootSignature* mLightProxyRootSignature = nullptr;
	DescriptorSet* mLightProxyGeomUboDescSet = nullptr;
	Pipeline* mLightProxyGeomPipeline = nullptr;

	//Geometry* mModelGeo = nullptr;
	Geometry* mSkyboxGeo = nullptr;

	Buffer* mCameraUniformBuffer[IMAGE_COUNT] = { nullptr, nullptr };
	Buffer* mMaterialUniformBuffer[IMAGE_COUNT] = { nullptr, nullptr };
	Buffer* mRoomUniformBuffer[IMAGE_COUNT] = { nullptr, nullptr };
	Buffer* mCubeUniformBuffer[IMAGE_COUNT] = { nullptr, nullptr };
	Buffer* mLightUniformBuffer[IMAGE_COUNT][2] = { nullptr, nullptr, nullptr, nullptr };

	CameraUbo      mCameraData;
	UniformBuffer  mModelData;
	UniformBuffer  mSkyboxData;
	UniformBuffer  mPbrSamplerData;
	LightProxyData mLightData;

	// Gui
	UIMiddleware  mUiMiddleware;
	GuiComponent* mMainGui = nullptr;
	GuiComponent* mSecondGui = nullptr;
};

SG_DEFINE_APPLICATION_MAIN(CustomRenderer)