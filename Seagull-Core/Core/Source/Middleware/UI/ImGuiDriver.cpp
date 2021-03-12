#include <include/tinyimageformat_query.h>

#include <include/imgui.h>
#include <include/imgui_internal.h>

#include "Include/IRenderer.h"
#include "Include/IResourceLoader.h"

#include "Middleware/UI/UIMiddleware.h"
#include "Interface/IInput.h"

#define LABELID1(prop) eastl::string().sprintf("##%llu", (uint64_t)(prop)).c_str()

static const uint64_t VERTEX_BUFFER_SIZE = 1024 * 64 * sizeof(ImDrawVert);
static const uint64_t INDEX_BUFFER_SIZE = 128 * 1024 * sizeof(ImDrawIdx);

// rewrite slider functions
namespace ImGui 
{

	bool SliderFloatWithSteps(const char* label, float* v, float v_min, float v_max, float v_step, const char* display_format)
	{
		eastl::string text_buf;
		bool          value_changed = false;

		if (!display_format)
			display_format = "%.1f";
		text_buf.sprintf(display_format, *v);

		if (ImGui::GetIO().WantTextInput)
		{
			value_changed = ImGui::SliderFloat(label, v, v_min, v_max, text_buf.c_str());

			int v_i = int(((*v - v_min) / v_step) + 0.5f);
			*v = v_min + float(v_i) * v_step;
		}
		else
		{
			// Map from [v_min,v_max] to [0,N]
			const int countValues = int((v_max - v_min) / v_step);
			int       v_i = int(((*v - v_min) / v_step) + 0.5f);
			value_changed = ImGui::SliderInt(label, &v_i, 0, countValues, text_buf.c_str());

			// Remap from [0,N] to [v_min,v_max]
			*v = v_min + float(v_i) * v_step;
		}

		if (*v < v_min)
			*v = v_min;
		if (*v > v_max)
			*v = v_max;

		return value_changed;
	}

	bool SliderIntWithSteps(const char* label, int32_t* v, int32_t v_min, int32_t v_max, int32_t v_step, const char* display_format)
	{
		eastl::string text_buf;
		bool          value_changed = false;

		if (!display_format)
			display_format = "%d";
		text_buf.sprintf(display_format, *v);

		if (ImGui::GetIO().WantTextInput)
		{
			value_changed = ImGui::SliderInt(label, v, v_min, v_max, text_buf.c_str());

			int32_t v_i = int((*v - v_min) / v_step);
			*v = v_min + int32_t(v_i) * v_step;
		}
		else
		{
			// Map from [v_min,v_max] to [0,N]
			const int countValues = int((v_max - v_min) / v_step);
			int32_t   v_i = int((*v - v_min) / v_step);
			value_changed = ImGui::SliderInt(label, &v_i, 0, countValues, text_buf.c_str());

			// Remap from [0,N] to [v_min,v_max]
			*v = v_min + int32_t(v_i) * v_step;
		}

		if (*v < v_min)
			*v = v_min;
		if (*v > v_max)
			*v = v_max;

		return value_changed;
	}

} // namespace ImGui


namespace SG
{

	// we use our own render backend, not use ImGui's render backend
	class ImGuiGuiDriver : public GUIDriver
	{
	public:
		ImGuiGuiDriver() = default;
		virtual ~ImGuiGuiDriver() = default;

		virtual bool OnInit(Renderer* pRenderer, uint32_t const maxDynamicUIUpdatesPerBatch) override;
		virtual void OnExit() override;

		virtual bool OnLoad(RenderTarget** pRts, uint32_t count) override;
		virtual void OnUnload() override;

		// for GUI with custom shaders not necessary in a normal application
		virtual void SetCustomShader(Shader* pShader) override;

		virtual bool AddFont(void* pFontBuffer, uint32_t fontBufferSize, void* pFontGlyphRanges, float fontSize, uintptr_t* pFont) override;

		virtual void* GetContext() override;

		virtual bool OnUpdate(GUIUpdate* update) override;
		virtual void OnDraw(Cmd* q) override;

		virtual bool     IsFocused() override;
		virtual bool     OnText(const wchar_t* pText) override;
		virtual bool     OnButton(uint32_t button, bool press, const Vec2* vec) override;
		virtual uint8_t  WantTextInput() const override;

		// override ImGui's allocate function
		static void* alloc_func(size_t size, void* pUserData) { return sg_malloc(size); }
		static void  free_func(void* ptr, void* pUserData) { sg_free(ptr); }
	protected:
		static const uint32_t MAX_FRAMES = 2;

		ImGuiContext* pImGuiContext;
		eastl::vector<Texture*> mFontTextures;
		Vec2      mDpiScale;
		uint32_t  mFrameIndex;

		Renderer* pRenderer = nullptr;
		Shader* pShaderTextured = nullptr;
		RootSignature* pRootSignatureTextured = nullptr;
		DescriptorSet* pDescriptorSetUniforms = nullptr;
		DescriptorSet* pDescriptorSetTexture = nullptr;
		Pipeline* pPipelineTextured = nullptr;
		Buffer* pVertexBuffer = nullptr;
		Buffer* pIndexBuffer = nullptr;
		Buffer* pUniformBuffer[MAX_FRAMES];

		Sampler* pDefaultSampler = nullptr;
		VertexLayout mVertexLayoutTextured = {};
		uint32_t     mMaxDynamicUIUpdatesPerBatch;
		uint32_t     mDynamicUIUpdates;
		float        mNavInputs[ImGuiNavInput_COUNT];
		const Vec2*  pMovePosition;
		uint32_t     mLastUpdateCount;
		Vec2         mLastUpdateMin[64] = {};
		Vec2         mLastUpdateMax[64] = {};
		bool         mIsActive;
		bool         mUseCustomShader;
		bool         mPostUpdateKeyDownStates[512]; // for delay updating key pressed purpose
	};

	// user defined window changes callbacks
	void IWidget::ProcessCallback(bool deferred)
	{
		if (!deferred)
		{
			isHovered = ImGui::IsItemHovered();
			isActive = ImGui::IsItemActive();
			isFocused = ImGui::IsItemFocused();
			isEdited = ImGui::IsItemEdited();
			isDeactivated = ImGui::IsItemDeactivated();
			isDeactivatedAfterEdit = ImGui::IsItemDeactivatedAfterEdit();
		}

		if (this->useDeferred != deferred)
			return;

		if (pOnHover && isHovered)
			pOnHover();

		if (pOnActive && isActive)
			pOnActive();

		if (pOnFocus && isFocused)
			pOnFocus();

		if (pOnEdited && isEdited)
			pOnEdited();

		if (pOnDeactivated && isDeactivated)
			pOnDeactivated();

		if (pOnDeactivatedAfterEdit && isDeactivatedAfterEdit)
			pOnDeactivatedAfterEdit();
	}

	bool ImGuiGuiDriver::OnInit(Renderer* pRenderer, uint32_t const maxDynamicUIUpdatesPerBatch)
	{
		// reset
		mHandledGestures = false;
		this->pRenderer = pRenderer;
		mMaxDynamicUIUpdatesPerBatch = maxDynamicUIUpdatesPerBatch;
		mIsActive = true;
		memset(mPostUpdateKeyDownStates, false, sizeof(mPostUpdateKeyDownStates));

		SamplerCreateDesc samplerDesc = {};
		samplerDesc.addressU = SG_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerDesc.addressV = SG_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerDesc.addressW = SG_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerDesc.minFilter = SG_FILTER_LINEAR;
		samplerDesc.magFilter = SG_FILTER_LINEAR;
		samplerDesc.mipMapMode = SG_MIPMAP_MODE_NEAREST;
		samplerDesc.compareFunc = SG_COMPARE_MODE_NEVER;
		add_sampler(this->pRenderer, &samplerDesc, &pDefaultSampler);

		if (!mUseCustomShader)
		{
			ShaderLoadDesc shaderDesc = {};
			shaderDesc.stages[0] = { "ImGuiShader/imgui.vert", nullptr, 0, nullptr };
			shaderDesc.stages[1] = { "ImGuiShader/imgui.frag", nullptr, 0, nullptr };
			shaderDesc.target = SG_SHADER_TARGET_6_3;
			add_shader(pRenderer, &shaderDesc, &pShaderTextured);
		}

		const char* pStaticSamplerNames[] = { "uSampler" };
		RootSignatureCreateDesc textureRootDesc = { &pShaderTextured, 1 };
		textureRootDesc.staticSamplerCount = 1;
		textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		textureRootDesc.ppStaticSamplers = &pDefaultSampler;
		add_root_signature(pRenderer, &textureRootDesc, &pRootSignatureTextured);

		DescriptorSetCreateDesc setDesc = { pRootSignatureTextured, SG_DESCRIPTOR_UPDATE_FREQ_PER_BATCH, 1 + (maxDynamicUIUpdatesPerBatch * MAX_FRAMES) };
		add_descriptor_set(pRenderer, &setDesc, &pDescriptorSetTexture);
		setDesc = { pRootSignatureTextured, SG_DESCRIPTOR_UPDATE_FREQ_NONE, MAX_FRAMES };
		add_descriptor_set(pRenderer, &setDesc, &pDescriptorSetUniforms);

		BufferLoadDesc vbDesc = {};
		vbDesc.desc.descriptors = SG_DESCRIPTOR_TYPE_VERTEX_BUFFER;
		vbDesc.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU; // not use staging buffer here
		vbDesc.desc.size = VERTEX_BUFFER_SIZE * MAX_FRAMES;
		vbDesc.desc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		vbDesc.ppBuffer = &pVertexBuffer;
		add_resource(&vbDesc, nullptr);

		BufferLoadDesc ibDesc = vbDesc;
		ibDesc.desc.descriptors = SG_DESCRIPTOR_TYPE_INDEX_BUFFER;
		ibDesc.desc.size = INDEX_BUFFER_SIZE * MAX_FRAMES;
		ibDesc.ppBuffer = &pIndexBuffer;
		add_resource(&ibDesc, nullptr);

		BufferLoadDesc ubDesc = {};
		ubDesc.desc.descriptors = SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.desc.memoryUsage = SG_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.desc.flags = SG_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.desc.size = sizeof(Matrix4);
		for (uint32_t i = 0; i < MAX_FRAMES; ++i)
		{
			ubDesc.ppBuffer = &pUniformBuffer[i];
			add_resource(&ubDesc, nullptr);
		}

		mVertexLayoutTextured.attribCount = 3;
		mVertexLayoutTextured.attribs[0].semantic = SG_SEMANTIC_POSITION;
		mVertexLayoutTextured.attribs[0].format = TinyImageFormat_R32G32_SFLOAT;
		mVertexLayoutTextured.attribs[0].binding = 0;
		mVertexLayoutTextured.attribs[0].location = 0;
		mVertexLayoutTextured.attribs[0].offset = 0;

		mVertexLayoutTextured.attribs[1].semantic = SG_SEMANTIC_TEXCOORD0;
		mVertexLayoutTextured.attribs[1].format = TinyImageFormat_R32G32_SFLOAT;
		mVertexLayoutTextured.attribs[1].binding = 0;
		mVertexLayoutTextured.attribs[1].location = 1;
		mVertexLayoutTextured.attribs[1].offset = TinyImageFormat_BitSizeOfBlock(mVertexLayoutTextured.attribs[0].format) / 8;

		mVertexLayoutTextured.attribs[2].semantic = SG_SEMANTIC_COLOR;
		mVertexLayoutTextured.attribs[2].format = TinyImageFormat_R8G8B8A8_UNORM;
		mVertexLayoutTextured.attribs[2].binding = 0;
		mVertexLayoutTextured.attribs[2].location = 2;
		mVertexLayoutTextured.attribs[2].offset = mVertexLayoutTextured.attribs[1].offset + TinyImageFormat_BitSizeOfBlock(mVertexLayoutTextured.attribs[1].format) / 8;

		mDpiScale = get_dpi_scale();

		// ImGui init
		ImGui::SetAllocatorFunctions(alloc_func, free_func);
		pImGuiContext = ImGui::CreateContext();
		ImGui::SetCurrentContext(pImGuiContext);

		ImGui::StyleColorsLight();

		ImGuiIO& io = ImGui::GetIO();
		io.NavActive = true;
		io.WantCaptureMouse = false;
		io.KeyMap[ImGuiKey_Backspace] = SG_KEY_BACK;
		io.KeyMap[ImGuiKey_LeftArrow] = SG_KEY_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = SG_KEY_RIGHT;
		io.KeyMap[ImGuiKey_Home] = SG_KEY_HOME;
		io.KeyMap[ImGuiKey_End] = SG_KEY_END;
		io.KeyMap[ImGuiKey_Delete] = SG_KEY_DELETE;

		for (uint32_t i = 0; i < MAX_FRAMES; ++i)
		{
			DescriptorData params[1] = {};
			params[0].name = "uniformBlockVS";
			params[0].ppBuffers = &pUniformBuffer[i];
			update_descriptor_set(pRenderer, i, pDescriptorSetUniforms, 1, params); // update ubo
		}

		return true;
	}

	void ImGuiGuiDriver::OnExit()
	{
		remove_sampler(pRenderer, pDefaultSampler);
		if (!mUseCustomShader)
			remove_shader(pRenderer, pShaderTextured);
		remove_descriptor_set(pRenderer, pDescriptorSetTexture);
		remove_descriptor_set(pRenderer, pDescriptorSetUniforms);
		remove_root_signature(pRenderer, pRootSignatureTextured);

		remove_resource(pVertexBuffer);
		remove_resource(pIndexBuffer);
		for (uint32_t i = 0; i < MAX_FRAMES; ++i)
			remove_resource(pUniformBuffer[i]);
		for (Texture*& pFontTexture : mFontTextures)
			remove_resource(pFontTexture);

		mFontTextures.set_capacity(0);
		ImGui::DestroyContext(pImGuiContext);
	}

	bool ImGuiGuiDriver::OnLoad(RenderTarget** pRts, uint32_t count)
	{
		UNREF_PARAM(count);

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.srcFactors[0] = SG_BLEND_CONST_SRC_ALPHA;
		blendStateDesc.dstFactors[0] = SG_BLEND_CONST_ONE_MINUS_SRC_ALPHA;
		blendStateDesc.srcAlphaFactors[0] = SG_BLEND_CONST_SRC_ALPHA;
		blendStateDesc.dstAlphaFactors[0] = SG_BLEND_CONST_ONE_MINUS_SRC_ALPHA;
		blendStateDesc.masks[0] = SG_BLEND_COLOR_MASK_ALL;
		blendStateDesc.renderTargetMask = SG_BLEND_STATE_TARGET_ALL;
		blendStateDesc.independentBlend = false;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.depthTest = false;
		depthStateDesc.depthWrite = false;

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.cullMode = SG_CULL_MODE_NONE;
		rasterizerStateDesc.scissor = true;

		PipelineCreateDesc desc = {};
		//desc.pCache = pCache;
		desc.type = SG_PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineDesc = desc.graphicsDesc;
		pipelineDesc.depthStencilFormat = TinyImageFormat_UNDEFINED;
		pipelineDesc.renderTargetCount = 1;
		pipelineDesc.sampleCount = pRts[0]->sampleCount;
		pipelineDesc.pBlendState = &blendStateDesc;
		pipelineDesc.sampleQuality = pRts[0]->sampleQuality;
		pipelineDesc.pColorFormats = &pRts[0]->format;
		pipelineDesc.pDepthState = &depthStateDesc;
		pipelineDesc.pRasterizerState = &rasterizerStateDesc;
		pipelineDesc.pRootSignature = pRootSignatureTextured;
		pipelineDesc.pShaderProgram = pShaderTextured;
		pipelineDesc.pVertexLayout = &mVertexLayoutTextured;
		pipelineDesc.primitiveTopo = SG_PRIMITIVE_TOPO_TRI_LIST;
		add_pipeline(pRenderer, &desc, &pPipelineTextured);

		return true;
	}

	void ImGuiGuiDriver::OnUnload()
	{
		remove_pipeline(pRenderer, pPipelineTextured);
	}

	void ImGuiGuiDriver::SetCustomShader(Shader* pShader)
	{
		pShaderTextured = pShader;
		mUseCustomShader = true;
	}

	bool ImGuiGuiDriver::AddFont(void* pFontBuffer, uint32_t fontBufferSize, void* pFontGlyphRanges, float fontSize, uintptr_t* pFont)
	{
		int            width, height;
		int            bytesPerPixel;
		unsigned char* pixels = nullptr;
		ImGuiIO& io = ImGui::GetIO();

		ImFontConfig config = {};
		config.FontDataOwnedByAtlas = false;
		ImFont* font = io.Fonts->AddFontFromMemoryTTF(pFontBuffer, fontBufferSize,
			fontSize * eastl::min(mDpiScale.x, mDpiScale.y), &config,
			(const ImWchar*)pFontGlyphRanges);
		if (font != nullptr)
		{
			io.FontDefault = font;
			*pFont = (uintptr_t)font;
		}
		else
		{
			*pFont = (uintptr_t)io.Fonts->AddFontDefault();
		}

		io.Fonts->Build();
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytesPerPixel);

		// At this point you've got the texture data and you need to upload that your your graphic system:
		// After we have created the texture, store its pointer/identifier (_in whichever format your engine uses_) in 'io.Fonts->TexID'.
		// This will be passed back to your via the renderer. Basically ImTextureID == void*. Read FAQ below for details about ImTextureID.
		Texture* pTexture = nullptr;
		SyncToken token = {};
		TextureLoadDesc loadDesc = {};
		TextureCreateDesc textureDesc = {};
		textureDesc.arraySize = 1;
		textureDesc.depth = 1;
		textureDesc.descriptors = SG_DESCRIPTOR_TYPE_TEXTURE;
		textureDesc.format = TinyImageFormat_R8G8B8A8_UNORM;
		textureDesc.height = height;
		textureDesc.mipLevels = 1;
		textureDesc.sampleCount = SG_SAMPLE_COUNT_1;
		textureDesc.startState = SG_RESOURCE_STATE_COMMON;
		textureDesc.width = width;
		textureDesc.name = "ImGui Font Texture";
		loadDesc.desc = &textureDesc;
		loadDesc.ppTexture = &pTexture;
		add_resource(&loadDesc, &token);
		wait_for_token(&token);

		TextureUpdateDesc updateDesc = { pTexture };
		begin_update_resource(&updateDesc);
		for (uint32_t r = 0; r < updateDesc.rowCount; ++r)
		{
			memcpy(updateDesc.pMappedData + r * updateDesc.dstRowStride,
				pixels + r * updateDesc.srcRowStride, updateDesc.srcRowStride);
		}
		end_update_resource(&updateDesc, &token);

		mFontTextures.emplace_back(pTexture);
		io.Fonts->TexID = (void*)(mFontTextures.size() - 1);

		DescriptorData params[1] = {};
		params[0].name = "uTex";
		params[0].ppTextures = &pTexture;
		update_descriptor_set(pRenderer, (uint32_t)mFontTextures.size() - 1, pDescriptorSetTexture, 1, params);

		return true;
	}

	void* ImGuiGuiDriver::GetContext()
	{
		return pImGuiContext;
	}

	bool ImGuiGuiDriver::OnUpdate(GUIUpdate* update)
	{
		ImGui::SetCurrentContext(pImGuiContext);

		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize.x = update->width;
		io.DisplaySize.y = update->height;
		io.DeltaTime = update->deltaTime;
		if (pMovePosition)
			io.MousePos = { pMovePosition->x, pMovePosition->y };
		memcpy(io.NavInputs, mNavInputs, sizeof(mNavInputs));

		ImGui::NewFrame();

		bool ret = false;
		if (mIsActive)
		{
			if (update->isShowDemoWindow)
				ImGui::ShowDemoWindow();

			mLastUpdateCount = update->componentCount;

			for (uint32_t compIndex = 0; compIndex < update->componentCount; ++compIndex)
			{
				GuiComponent* pComponent = update->pGuiComponents[compIndex];
				eastl::string title = pComponent->title;
				int32_t guiComponentFlags = pComponent->flags;

				bool* pCloseButtonActiveValue = pComponent->hasCloseButton ? &pComponent->hasCloseButton : nullptr;
				const eastl::vector<eastl::string>& contextualMenuLabels = pComponent->contextualMenuLabels;
				const eastl::vector<WidgetCallbackFunc>& contextualMenuCallbacks = pComponent->contextualMenuCallbacks;
				const Vec4& windowRect = pComponent->initialWindowRect;
				Vec4& currentWindowRect = pComponent->currentWindowRect;
				IWidget** pProps = pComponent->widgets.data();
				uint32_t  propCount = (uint32_t)pComponent->widgets.size();

				if (title == "")
					title.sprintf("##%llu", (uint64_t)pComponent);

				// Setup the ImGuiWindowFlags
				ImGuiWindowFlags guiWinFlags = SG_GUI_FLAGS_NONE;
				if (guiComponentFlags & SG_GUI_FLAGS_NO_TITLE_BAR)
					guiWinFlags |= ImGuiWindowFlags_NoTitleBar;
				if (guiComponentFlags & SG_GUI_FLAGS_NO_RESIZE)
					guiWinFlags |= ImGuiWindowFlags_NoResize;
				if (guiComponentFlags & SG_GUI_FLAGS_NO_MOVE)
					guiWinFlags |= ImGuiWindowFlags_NoMove;
				if (guiComponentFlags & SG_GUI_FLAGS_NO_SCROLLBAR)
					guiWinFlags |= ImGuiWindowFlags_NoScrollbar;
				if (guiComponentFlags & SG_GUI_FLAGS_NO_COLLAPSE)
					guiWinFlags |= ImGuiWindowFlags_NoCollapse;
				if (guiComponentFlags & SG_GUI_FLAGS_ALWAYS_AUTO_RESIZE)
					guiWinFlags |= ImGuiWindowFlags_AlwaysAutoResize;
				if (guiComponentFlags & SG_GUI_FLAGS_NO_INPUTS)
					guiWinFlags |= ImGuiWindowFlags_NoInputs;
				if (guiComponentFlags & SG_GUI_FLAGS_MEMU_BAR)
					guiWinFlags |= ImGuiWindowFlags_MenuBar;
				if (guiComponentFlags & SG_GUI_FLAGS_HORIZONTAL_SCROLLBAR)
					guiWinFlags |= ImGuiWindowFlags_HorizontalScrollbar;
				if (guiComponentFlags & SG_GUI_FLAGS_NO_FOCUS_ON_APPEARING)
					guiWinFlags |= ImGuiWindowFlags_NoFocusOnAppearing;
				if (guiComponentFlags & SG_GUI_FLAGS_NO_BRING_TO_FRONT_ON_FOCUS)
					guiWinFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
				if (guiComponentFlags & SG_GUI_FLAGS_ALWAYS_VERTICAL_SCROLLBAR)
					guiWinFlags |= ImGuiWindowFlags_AlwaysVerticalScrollbar;
				if (guiComponentFlags & SG_GUI_FLAGS_ALWAYS_HORIZONTAL_SCROLLBAR)
					guiWinFlags |= ImGuiWindowFlags_AlwaysHorizontalScrollbar;
				if (guiComponentFlags & SG_GUI_FLAGS_ALWAYS_USE_WINDOW_PADDING)
					guiWinFlags |= ImGuiWindowFlags_AlwaysUseWindowPadding;
				if (guiComponentFlags & SG_GUI_FLAGS_NO_NAV_INPUT)
					guiWinFlags |= ImGuiWindowFlags_NoNavInputs;
				if (guiComponentFlags & SG_GUI_FLAGS_NO_NAV_FOCUS)
					guiWinFlags |= ImGuiWindowFlags_NoNavFocus;

				ImGui::PushFont((ImFont*)pComponent->font);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, pComponent->alpha);

				// begin window
				bool result = ImGui::Begin(title.c_str(), pCloseButtonActiveValue, guiWinFlags);
				if (result)
				{
					// Setup the contextual menus
					if (!contextualMenuLabels.empty() && ImGui::BeginPopupContextItem())    // <-- This is using IsItemHovered()
					{
						for (size_t i = 0; i < contextualMenuLabels.size(); i++)
						{
							if (ImGui::MenuItem(contextualMenuLabels[i].c_str()))
							{
								if (i < contextualMenuCallbacks.size())
									contextualMenuCallbacks[i]();
							}
						}
						ImGui::EndPopup();
					}

					bool overrideSize = false;
					bool overridePos = false;

					if ((guiComponentFlags & SG_GUI_FLAGS_NO_RESIZE) && !(guiComponentFlags & SG_GUI_FLAGS_ALWAYS_AUTO_RESIZE))
						overrideSize = true;

					if (guiComponentFlags & SG_GUI_FLAGS_NO_MOVE)
						overridePos = true;

					ImGui::SetWindowSize(
						{ windowRect.z, windowRect.w }, overrideSize ? ImGuiCond_Always : ImGuiCond_Once);
					ImGui::SetWindowPos(
						{ windowRect.x, windowRect.y }, overridePos ? ImGuiCond_Always : ImGuiCond_Once);

					if (guiComponentFlags & SG_GUI_FLAGS_START_COLLAPSED)
						ImGui::SetWindowCollapsed(true, ImGuiCond_Once);

					for (uint32_t i = 0; i < propCount; ++i)
					{
						if (pProps[i] != nullptr)
						{
							pProps[i]->OnDraw();
						}
					}

					ret = ret || ImGui::GetIO().WantCaptureMouse;
				}

				Vec2 pos = { ImGui::GetWindowPos().x, ImGui::GetWindowPos().y };
				Vec2 size = { ImGui::GetWindowSize().x, ImGui::GetWindowSize().y };
				currentWindowRect.x = pos.x;
				currentWindowRect.y = pos.y;
				currentWindowRect.z = size.x;
				currentWindowRect.w = size.y;
				mLastUpdateMin[compIndex] = pos;
				mLastUpdateMax[compIndex] = pos + size;

				// Need to call ImGui::End event if result is false since we called ImGui::Begin
				ImGui::End();
				ImGui::PopStyleVar();
				ImGui::PopFont();
			}
		}
		ImGui::EndFrame();

		if (mIsActive)
		{
			for (uint32_t compIndex = 0; compIndex < update->componentCount; ++compIndex)
			{
				GuiComponent* pComponent = update->pGuiComponents[compIndex];
				IWidget** pProps = pComponent->widgets.data();
				uint32_t  propCount = (uint32_t)pComponent->widgets.size();

				for (uint32_t i = 0; i < propCount; ++i)
				{
					if (pProps[i] != nullptr)
					{
						pProps[i]->ProcessCallback(true);
					}
				}
			}
		}

		Vec2 pos = Vec2(-FLT_MAX);
		if (!io.MouseDown[0])
			io.MousePos = { pos.x, pos.y };
		mHandledGestures = false;

		// Apply post update keydown states
		memcpy(io.KeysDown, mPostUpdateKeyDownStates, sizeof(io.KeysDown));

		return ret;
	}

	void ImGuiGuiDriver::OnDraw(Cmd* pCmd)
	{
		ImGui::SetCurrentContext(pImGuiContext);
		ImGui::Render();
		mDynamicUIUpdates = 0;

		ImDrawData* drawData = ImGui::GetDrawData();
		Pipeline* pPipeline = pPipelineTextured;

		uint32_t vboSize = 0;
		uint32_t iboSize = 0;
		for (int n = 0; n < drawData->CmdListsCount; n++)
		{
			const ImDrawList* cmdList = drawData->CmdLists[n];
			vboSize += (int)(cmdList->VtxBuffer.size() * sizeof(ImDrawVert));
			iboSize += (int)(cmdList->IdxBuffer.size() * sizeof(ImDrawIdx));
		}

		vboSize = eastl::min<uint32_t>(vboSize, VERTEX_BUFFER_SIZE);
		iboSize = eastl::min<uint32_t>(iboSize, INDEX_BUFFER_SIZE);

		// Copy and convert all vertices into a single contiguous buffer
		uint64_t vOffset = mFrameIndex * VERTEX_BUFFER_SIZE;
		uint64_t iOffset = mFrameIndex * INDEX_BUFFER_SIZE;
		uint64_t vtxDst = vOffset;
		uint64_t idxDst = iOffset;
		for (int n = 0; n < drawData->CmdListsCount; n++)
		{
			const ImDrawList* cmdList = drawData->CmdLists[n];
			BufferUpdateDesc  update = { pVertexBuffer, vtxDst };
			begin_update_resource(&update);
			memcpy(update.pMappedData, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.size() * sizeof(ImDrawVert));
			end_update_resource(&update, NULL);

			update = { pIndexBuffer, idxDst };
			begin_update_resource(&update);
			memcpy(update.pMappedData, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.size() * sizeof(ImDrawIdx));
			end_update_resource(&update, NULL);

			vtxDst += (cmdList->VtxBuffer.size() * sizeof(ImDrawVert));
			idxDst += (cmdList->IdxBuffer.size() * sizeof(ImDrawIdx));
		}

		float L = drawData->DisplayPos.x;
		float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
		float T = drawData->DisplayPos.y;
		float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
		float mvp[4][4] = {
			{ 2.0f / (R - L), 0.0f, 0.0f, 0.0f },
			{ 0.0f, 2.0f / (T - B), 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.5f, 0.0f },
			{ (R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f },
		};

		BufferUpdateDesc update = { pUniformBuffer[mFrameIndex] };
		begin_update_resource(&update);
		*((Matrix4*)update.pMappedData) = *(Matrix4*)mvp;
		end_update_resource(&update, nullptr);

		const uint32_t vertexStride = sizeof(ImDrawVert);
		cmd_set_viewport(pCmd, 0.0f, 0.0f, drawData->DisplaySize.x, drawData->DisplaySize.y, 0.0f, 1.0f);
		cmd_set_scissor(pCmd, (uint32_t)drawData->DisplayPos.x, (uint32_t)drawData->DisplayPos.y, (uint32_t)drawData->DisplaySize.x,
			(uint32_t)drawData->DisplaySize.y);
		cmd_bind_pipeline(pCmd, pPipeline);
		cmd_bind_index_buffer(pCmd, pIndexBuffer, SG_INDEX_TYPE_UINT16, iOffset);
		cmd_bind_vertex_buffer(pCmd, 1, &pVertexBuffer, &vertexStride, &vOffset);

		cmd_bind_descriptor_set(pCmd, mFrameIndex, pDescriptorSetUniforms);

		// Render command lists
		int    vtxOffset = 0;
		int    idxOffset = 0;
		Vec2   pos = { drawData->DisplayPos.x, drawData->DisplayPos.y };
		for (int n = 0; n < drawData->CmdListsCount; n++)
		{
			const ImDrawList* cmdList = drawData->CmdLists[n];
			for (int cmdIndex = 0; cmdIndex < cmdList->CmdBuffer.size(); cmdIndex++)
			{
				const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmdIndex];
				if (pcmd->UserCallback)
				{
					// User callback (registered via ImDrawList::AddCallback)
					pcmd->UserCallback(cmdList, pcmd);
				}
				else
				{
					// Apply scissor/clipping rectangle
					//const D3D12_RECT r = { (LONG)(pcmd->ClipRect.x - pos.x), (LONG)(pcmd->ClipRect.y - pos.y), (LONG)(pcmd->ClipRect.z - pos.x), (LONG)(pcmd->ClipRect.w - pos.y) };
					//pCmd->pDxCmdList->RSSetScissorRects(1, &r);
					cmd_set_scissor(
						pCmd, (uint32_t)(pcmd->ClipRect.x - pos.x), (uint32_t)(pcmd->ClipRect.y - pos.y),
						(uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x), (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y));

					size_t id = (size_t)pcmd->TextureId;
					if (id >= mFontTextures.size())
					{
						uint32_t setIndex = (uint32_t)mFontTextures.size() + (mFrameIndex * mMaxDynamicUIUpdatesPerBatch + mDynamicUIUpdates);
						DescriptorData params[1] = {};
						params[0].name = "uTex";
						params[0].ppTextures = (Texture**)&pcmd->TextureId;
						update_descriptor_set(pRenderer, setIndex, pDescriptorSetTexture, 1, params);
						cmd_bind_descriptor_set(pCmd, setIndex, pDescriptorSetTexture);
						++mDynamicUIUpdates;
					}
					else
					{
						cmd_bind_descriptor_set(pCmd, (uint32_t)id, pDescriptorSetTexture);
					}

					cmd_draw_indexed(pCmd, pcmd->ElemCount, idxOffset, vtxOffset);
				}
				idxOffset += pcmd->ElemCount;
			}
			vtxOffset += (int)cmdList->VtxBuffer.size();
		}

		mFrameIndex = (mFrameIndex + 1) % MAX_FRAMES;
	}

	bool ImGuiGuiDriver::IsFocused()
	{
		ImGui::SetCurrentContext(pImGuiContext);
		ImGuiIO& io = ImGui::GetIO();
		return io.WantCaptureMouse;
	}

	bool ImGuiGuiDriver::OnText(const wchar_t* pText)
	{
		ImGui::SetCurrentContext(pImGuiContext);
		ImGuiIO& io = ImGui::GetIO();
		uint32_t len = (uint32_t)wcslen(pText);
		for (uint32_t i = 0; i < len; ++i)
			io.AddInputCharacter(pText[i]);

		return !ImGui::GetIO().WantCaptureMouse;
	}

	bool ImGuiGuiDriver::OnButton(uint32_t button, bool press, const Vec2* vec)
	{
		ImGui::SetCurrentContext(pImGuiContext);
		ImGuiIO& io = ImGui::GetIO();
		pMovePosition = vec;

		switch (button)
		{
			//case SG_KEY_DPAD_LEFT: mNavInputs[ImGuiNavInput_DpadLeft] = (float)press; break;
			//case SG_KEY_DPAD_RIGHT: mNavInputs[ImGuiNavInput_DpadRight] = (float)press; break;
			//case SG_KEY_DPAD_UP: mNavInputs[ImGuiNavInput_DpadUp] = (float)press; break;
			//case SG_KEY_DPAD_DOWN: mNavInputs[ImGuiNavInput_DpadDown] = (float)press; break;
			//case SG_KEY_EAST: mNavInputs[ImGuiNavInput_Cancel] = (float)press; break;
			//case SG_KEY_WEST: mNavInputs[ImGuiNavInput_Menu] = (float)press; break;
			//case SG_KEY_NORTH: mNavInputs[ImGuiNavInput_Input] = (float)press; break;
			//case SG_KEY_L1: mNavInputs[ImGuiNavInput_FocusPrev] = (float)press; break;
			//case SG_KEY_R1: mNavInputs[ImGuiNavInput_FocusNext] = (float)press; break;
			//case SG_KEY_L2: mNavInputs[ImGuiNavInput_TweakSlow] = (float)press; break;
			//case SG_KEY_R2: mNavInputs[ImGuiNavInput_TweakFast] = (float)press; break;
			//case SG_KEY_R3: if (!press) { mActive = !mActive; } break;
		case SG_KEY_SHIFTL:
		case SG_KEY_SHIFTR:
			io.KeyShift = press;
			break;
			//case SG_KEY_SOUTH:
		case SG_MOUSE_RIGHT:
		case SG_MOUSE_MIDDLE:
		case SG_MOUSE_SCROLL_UP:
		case SG_MOUSE_SCROLL_DOWN:
		{
			const float scrollScale = 0.25f; // This should maybe be customized by client?  1.f would scroll ~5 lines of txt according to ImGui doc.
			mNavInputs[ImGuiNavInput_Activate] = (float)press;
			if (pMovePosition)
			{
				//if (SG_KEY_SOUTH == button)
				//	io.MouseDown[0] = press;
				if (SG_MOUSE_RIGHT == button)
					io.MouseDown[1] = press;
				else if (SG_MOUSE_MIDDLE == button)
					io.MouseDown[2] = press;
				else if (SG_MOUSE_SCROLL_UP == button)
					io.MouseWheel = 1.f * scrollScale;
				else if (SG_MOUSE_SCROLL_DOWN == button)
					io.MouseWheel = -1.f * scrollScale;

			}
			if (!mIsActive)
				return true;

			if (io.MousePos.x != -FLT_MAX && io.MousePos.y != -FLT_MAX)
			{
				return !(io.WantCaptureMouse);
			}
			else if (pMovePosition)
			{
				io.MousePos = { pMovePosition->x, pMovePosition->y };
				for (uint32_t i = 0; i < mLastUpdateCount; ++i)
				{
					if (ImGui::IsMouseHoveringRect({ mLastUpdateMin[i].x, mLastUpdateMin[i].y },
						{ mLastUpdateMax[i].x, mLastUpdateMax[i].y }, false))
					{
						io.WantCaptureMouse = true;
						return false;
					}
				}
				return true;
			}

		}

		// Note that for keyboard keys, we only set them to true here if they are pressed because we may have a press/release
		// happening in one frame and it would never get registered.  Instead, unpressed are deferred at the end of update().
		// This scenario occurs with mobile soft (on-screen) keyboards.
		case SG_KEY_BACK:
			if (press)
				io.KeysDown[SG_KEY_BACK] = true;
			mPostUpdateKeyDownStates[SG_KEY_BACK] = press;
			break;
		case SG_KEY_LEFT:
			if (press)
				io.KeysDown[SG_KEY_LEFT] = true;
			mPostUpdateKeyDownStates[SG_KEY_LEFT] = press;
			break;
		case SG_KEY_RIGHT:
			if (press)
				io.KeysDown[SG_KEY_RIGHT] = true;
			mPostUpdateKeyDownStates[SG_KEY_RIGHT] = press;
			break;
		case SG_KEY_HOME:
			if (press)
				io.KeysDown[SG_KEY_HOME] = true;
			mPostUpdateKeyDownStates[SG_KEY_HOME] = press;
			break;
		case SG_KEY_END:
			if (press)
				io.KeysDown[SG_KEY_END] = true;
			mPostUpdateKeyDownStates[SG_KEY_END] = press;
			break;
		case SG_KEY_DELETE:
			if (press)
				io.KeysDown[SG_KEY_DELETE] = true;
			mPostUpdateKeyDownStates[SG_KEY_DELETE] = press;
			break;
		default:
			break;
		}

		return false;
	}

	uint8_t ImGuiGuiDriver::WantTextInput() const
	{
		ImGui::SetCurrentContext(pImGuiContext);
		// the User flags are not what I expect them to be.
		// we need access to Per-Component InputFlags
		ImGuiContext* guiContext = (ImGuiContext*)this->pImGuiContext;
		ImGuiInputTextFlags currentInputFlags = guiContext->InputTextState.UserFlags;

		// 0 -> Not pressed
		// 1 -> Digits Only keyboard
		// 2 -> Full Keyboard (Chars + Digits)
		int inputState = ImGui::GetIO().WantTextInput ? 2 : 0;
		// keyboard only Numbers
		if (inputState > 0 && (currentInputFlags & ImGuiInputTextFlags_CharsDecimal))
		{
			inputState = 1;
		}

		return inputState;
	}

	void init_gui_driver(GUIDriver** ppDriver)
	{
		ImGuiGuiDriver* pDriver = sg_new(ImGuiGuiDriver);
		*ppDriver = pDriver;
	}

	void delete_gui_driver(GUIDriver* pDriver)
	{
		sg_delete(pDriver);
	}

#pragma region (Widget OnDraw)

	static Vec4 ToVec4Color(unsigned int color)
	{
		Vec4 col;    // Translate colours back by bit shifting
		col.x = (float)((color & 0xFF000000) >> 24);
		col.y = (float)((color & 0x00FF0000) >> 16);
		col.z = (float)((color & 0x0000FF00) >> 8);
		col.w = (float)(color & 0x000000FF);
		return col;
	}	
	
	static unsigned int ToUintColor(Vec4 color)
	{
		unsigned int c = (((unsigned int)color.x << 24) & 0xFF000000) |
			(((unsigned int)color.y << 16) & 0x00FF0000) |
			(((unsigned int)color.z << 8) & 0x0000FF00) |
			(((unsigned int)color.w) & 0x000000FF);
		return c;
	}

	void LabelWidget::OnDraw()
	{
		ImGui::Text("%s", mLabel.c_str());
		ProcessCallback();
	}

	void ColorLabelWidget::OnDraw()
	{
		ImGui::TextColored({ mColor.x, mColor.y, mColor.z, mColor.w }, "%s", mLabel.c_str());
		ProcessCallback();
	}

	void ButtonWidget::OnDraw()
	{
		ImGui::Button(mLabel.c_str());
		ProcessCallback();
	}

	void RadioButtonWidget::OnDraw()
	{
		ImGui::RadioButton(mLabel.c_str(), pData, mRadioId);
		ProcessCallback();
	}

	void SliderFloatWidget::OnDraw()
	{
		ImGui::Text("%s", mLabel.c_str());
		ImGui::SliderFloatWithSteps(mLabel.c_str(), pData, mMin, mMax, mStep, mFormat.c_str());
		ProcessCallback();
	}

	void SliderFloat2Widget::OnDraw()
	{
		ImGui::Text("%s", mLabel.c_str());
		for (uint32_t i = 0; i < 2; i++)
		{
			ImGui::SliderFloatWithSteps(LABELID1(&pData->operator[](i)), &pData->operator[](i), mMin[i], mMax[i], mStep[i], mFormat.c_str());
			ProcessCallback();
		}
	}

	void SliderFloat3Widget::OnDraw()
	{
		ImGui::Text("%s", mLabel.c_str());
		for (uint32_t i = 0; i < 3; i++)
		{
			ImGui::SliderFloatWithSteps(LABELID1(&pData->operator[](i)), &pData->operator[](i), mMin[i], mMax[i], mStep[i], mFormat.c_str());
			ProcessCallback();
		}
	}

	void SliderFloat4Widget::OnDraw()
	{
		ImGui::Text("%s", mLabel.c_str());
		for (uint32_t i = 0; i < 4; i++)
		{
			ImGui::SliderFloatWithSteps(LABELID1(&pData->operator[](i)), &pData->operator[](i), mMin[i], mMax[i], mStep[i], mFormat.c_str());
			ProcessCallback();
		}
	}

	void SliderIntWidget::OnDraw()
	{
		ImGui::Text("%s", mLabel.c_str());
		ImGui::SliderIntWithSteps(LABELID1(pData), pData, mMin, mMax, mStep, mFormat.c_str());
		ProcessCallback();
	}

	void SliderUintWidget::OnDraw()
	{
		ImGui::Text("%s", mLabel.c_str());
		ImGui::SliderIntWithSteps(LABELID1(pData), (int32_t*)pData, (int32_t)mMin, (int32_t)mMax, (int32_t)mStep, mFormat.c_str());
		ProcessCallback();
	}

	void CheckboxWidget::OnDraw()
	{
		ImGui::Text(mLabel.c_str());
		ImGui::Checkbox(LABELID1(pData), pData);
		ProcessCallback();
	}

	void OneLineCheckboxWidget::OnDraw()
	{
		ImGui::Checkbox(LABELID1(pData), pData);
		ImGui::SameLine();
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(mColor), "%s", mLabel.c_str());
		ProcessCallback();
	}

	void CursorLocationWidget::OnDraw()
	{
		ImGui::SetCursorPos({ mLocation.x, mLocation.y });
		ProcessCallback();
	}

	void DropdownWidget::OnDraw()
	{
		uint32_t& current = *pData;
		ImGui::Text("%s", mLabel.c_str());

		if (ImGui::BeginCombo(LABELID1(pData), mNames[current].c_str()))
		{
			for (uint32_t i = 0; i < (uint32_t)mNames.size(); ++i)
			{
				if (ImGui::Selectable(mNames[i].c_str())) // add a selectable item
				{
					uint32_t prevVal = current;
					current = i;

					// Note that callbacks are sketchy with BeginCombo/EndCombo, so we manually process them here
					if (pOnEdited)
						pOnEdited();

					if (current != prevVal)
					{
						if (pOnDeactivatedAfterEdit)
							pOnDeactivatedAfterEdit();
					}
				}
			}
			ImGui::EndCombo();
		}
	}

	void ColumnWidget::OnDraw()
	{
		// Test a simple 4 col table.
		ImGui::BeginColumns(mLabel.c_str(), mNumColumns, ImGuiColumnsFlags_NoResize | ImGuiColumnsFlags_NoForceWithinWindow);
		for (uint32_t i = 0; i < mNumColumns; ++i)
		{
			mPerColumnWidgets[i]->OnDraw();
			ImGui::NextColumn();
		}
		ImGui::EndColumns();

		ProcessCallback();
	}

	void ProgressBarWidget::OnDraw()
	{
		size_t currProgress = *pData;
		ImGui::Text("%s", mLabel.c_str());
		ImGui::ProgressBar((float)currProgress / mMaxProgress);
		ProcessCallback();
	}

	void ColorPickerWidget::OnDraw()
	{
		unsigned int& colorPick = *(unsigned int*)pData;
		Vec4 comboColor = ToVec4Color(colorPick) / 255.0f;

		float col[4] = { comboColor.x, comboColor.y, comboColor.z, comboColor.w };
		ImGui::Text("%s", mLabel.c_str());
		if (ImGui::ColorPicker4(LABELID1(pData), col, ImGuiColorEditFlags_AlphaPreview))
		{
			if (col[0] != comboColor.x || col[1] != comboColor.y || col[2] != comboColor.z || col[3] != comboColor.w)
			{
				comboColor = { col[0], col[1], col[2], col[3] };
				colorPick = ToUintColor(comboColor * 255.0f);
			}
		}
		ProcessCallback();
	}

	void ColorSliderWidget::OnDraw()
	{
		unsigned int& colorPick = *(unsigned int*)pData;
		Vec4 comboColor = ToVec4Color(colorPick) / 255.0f;

		float col[4] = { comboColor.x, comboColor.y, comboColor.z, comboColor.w };
		ImGui::Text("%s", mLabel.c_str());
		if (ImGui::ColorEdit4(LABELID1(pData), col, ImGuiColorEditFlags_AlphaPreview))
		{
			if (col[0] != comboColor.x || col[1] != comboColor.y || col[2] != comboColor.z || col[3] != comboColor.w)
			{
				comboColor = { col[0], col[1], col[2], col[3] };
				colorPick = ToUintColor(comboColor * 255.0f);
			}
		}
		ProcessCallback();
	}

	void TextboxWidget::OnDraw()
	{
		ImGui::InputText(LABELID1(pData), (char*)pData, mLength, mAutoSelectAll ? ImGuiInputTextFlags_AutoSelectAll : 0);
		ProcessCallback();
	}

	void DrawTextWidget::OnDraw()
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		ImVec2 offset = { window->Pos.x - window->Scroll.x, window->Pos.y - window->Scroll.y };
		Vec2 pos = { offset.x + mPos.x, offset.y + mPos.y };
		ImVec2 textSize = ImGui::CalcTextSize(mLabel.c_str());
		const Vec2 lineSize = { textSize.x, textSize.y };

		ImGui::GetWindowDrawList()->AddText({ pos.x, pos.y }, ImGui::GetColorU32(mColor), mLabel.c_str());

		Vec2 maxSize = pos + lineSize;
		ImRect boundingBox({ pos.x, pos.y }, { maxSize.x, maxSize.y });
		ImGui::ItemSize(boundingBox);
		ImGui::ItemAdd(boundingBox, 0);

		ProcessCallback();
	}

	void DrawLineWidget::OnDraw()
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		ImVec2 offset = { window->Pos.x - window->Scroll.x, window->Pos.y - window->Scroll.y };
		Vec2 pos1 = { offset.x + mPos1.x, offset.y + mPos2.y };
		Vec2 pos2 = { offset.x + mPos1.x, offset.y + mPos2.y };

		ImGui::GetWindowDrawList()->AddLine({ pos1.x, pos1.y }, { pos2.x, pos2.y }, ImGui::GetColorU32(mColor));

		if (mAddItem)
		{
			ImRect boundingBox({ pos1.x, pos1.y }, { pos2.x, pos2.y });
			ImGui::ItemSize(boundingBox);
			ImGui::ItemAdd(boundingBox, 0);
		}

		ProcessCallback();
	}

	void DrawCurveWidget::OnDraw()
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();

		for (uint32_t i = 0; i < mNumPoints - 1; i++)
		{
			ImVec2 offset = { window->Pos.x - window->Scroll.x, window->Pos.y - window->Scroll.y };
			Vec2 pos1 = { offset.x + mPos[i].x, offset.y + mPos[i].y };
			Vec2 pos2 = { offset.x + mPos[i + 1].x, offset.y + mPos[i + 1].y };
			ImGui::GetWindowDrawList()->AddLine({ pos1.x, pos1.y }, { pos2.x, pos2.y }, ImGui::GetColorU32(mColor), mThickness);
		}

		ProcessCallback();
	}

#pragma endregion (Widget OnDraw)


}