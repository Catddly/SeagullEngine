#pragma once

#include "Interface/IMiddleware.h"
#include "Widget.h"

namespace SG
{

	typedef struct GuiCreateDesc
	{
		GuiCreateDesc(
			const Vec2& startPos = { 0.0f, 150.0f }, const Vec2& startSize = { 600.0f, 550.0f })
			: startPosition(startPos), startSize(startSize) {}

		Vec2 startPosition;
		Vec2 startSize;
	} GuiCreateDesc;

	enum GuiFlags
	{
		SG_GUI_FLAGS_NONE = 0,
		SG_GUI_FLAGS_NO_TITLE_BAR = 1 << 0,          // Disable title-bar
		SG_GUI_FLAGS_NO_RESIZE = 1 << 1,             // Disable user resizing
		SG_GUI_FLAGS_NO_MOVE = 1 << 2,               // Disable user moving the window
		SG_GUI_FLAGS_NO_SCROLLBAR = 1 << 3,          // Disable scrollbars (window can still scroll with mouse or programatically)
		SG_GUI_FLAGS_NO_COLLAPSE = 1 << 4,           // Disable user collapsing window by double-clicking on it
		SG_GUI_FLAGS_ALWAYS_AUTO_RESIZE = 1 << 5,    // Resize every window to its content every frame
		SG_GUI_FLAGS_NO_INPUTS = 1 << 6,             // Disable catching mouse or keyboard inputs, hovering test with pass through.
		SG_GUI_FLAGS_MEMU_BAR = 1 << 7,              // Has a menu-bar
		SG_GUI_FLAGS_HORIZONTAL_SCROLLBAR = 1 << 8,  // Allow horizontal scrollbar to appear (off by default).
		SG_GUI_FLAGS_NO_FOCUS_ON_APPEARING = 1 << 9, // Disable taking focus when transitioning from hidden to visible state
		SG_GUI_FLAGS_NO_BRING_TO_FRONT_ON_FOCUS =
		1 << 10,    // Disable bringing window to front when taking focus (e.g. clicking on it or programatically giving it focus)
		SG_GUI_FLAGS_ALWAYS_VERTICAL_SCROLLBAR = 1 << 11,      // Always show vertical scrollbar (even if ContentSize.y < Size.y)
		SG_GUI_FLAGS_ALWAYS_HORIZONTAL_SCROLLBAR = 1 << 12,    // Always show horizontal scrollbar (even if ContentSize.x < Size.x)
		SG_GUI_FLAGS_ALWAYS_USE_WINDOW_PADDING =
		1
		<< 13,    // Ensure child windows without border uses style.WindowPadding (ignored by default for non-bordered child windows, because more convenient)
		SG_GUI_FLAGS_NO_NAV_INPUT = 1 << 14,    // No gamepad/keyboard navigation within the window
		SG_GUI_FLAGS_NO_NAV_FOCUS =
		1 << 15,    // No focusing toward this window with gamepad/keyboard navigation (e.g. skipped by CTRL+TAB)
		SG_GUI_FLAGS_START_COLLAPSED = 1 << 16
	};


	class GuiComponent
	{
	public:
		IWidget* AddWidget(const IWidget& widget, bool clone = true);
		void     RemoveWidget(IWidget* pWidget);
		void     RemoveAllWidgets();

		eastl::vector<IWidget*>        widgets;
		eastl::vector<bool>            widgetsCloned;
		// Contextual menus when right clicking the title bar
		eastl::vector<eastl::string>   contextualMenuLabels;
		eastl::vector<WidgetCallbackFunc> contextualMenuCallbacks;
		Vec4                           initialWindowRect;
		Vec4                           currentWindowRect;
		eastl::string                  title;
		uintptr_t                      font;
		float                          alpha;
		// defaults to GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE
		// on mobile, GUI_COMPONENT_FLAGS_START_COLLAPSED is also set
		int32_t                        flags;

		bool                           isActive;
		// UI Component settings that can be modified at runtime by the client.
		bool                           hasCloseButton;
	};

	// helper class to remove and add the widget easily
	typedef struct DynamicUIWidgets
	{
		~DynamicUIWidgets()
		{
			Destroy();
		}

		IWidget* AddWidget(const IWidget& widget)
		{
			mDynamicProperties.emplace_back(widget.OnCopy());
			return mDynamicProperties.back();
		}

		void ShowWidgets(GuiComponent* pGui) // add all the widget to the Gui Component
		{
			for (size_t i = 0; i < mDynamicProperties.size(); ++i)
			{
				pGui->AddWidget(*mDynamicProperties[i], false);
			}
		}

		void HideWidgets(GuiComponent* pGui)
		{
			for (size_t i = 0; i < mDynamicProperties.size(); i++)
			{
				// TODO: fix it
				// We should not erase the widgets in this for-loop, otherwise the IDs
				// in mDynamicPropHandles will not match once GuiComponent::mWidgets changes size.
				pGui->RemoveWidget(mDynamicProperties[i]);
			}
		}

		void Destroy()
		{
			for (size_t i = 0; i < mDynamicProperties.size(); ++i)
			{
				mDynamicProperties[i]->~IWidget();
				sg_free(mDynamicProperties[i]);
			}

			mDynamicProperties.set_capacity(0);
		}
	private:
		eastl::vector<IWidget*> mDynamicProperties;
	} DynamicUIWidgets;

	// abstract interface for handling GUI
	class GUIDriver
	{
	public:
		struct GUIUpdate
		{
			GuiComponent** pGuiComponents;
			uint32_t componentCount;
			float deltaTime;
			float width;
			float height;
			bool  isShowDemoWindow;
		};
		virtual ~GUIDriver() {}

		virtual bool OnInit(Renderer* pRenderer, uint32_t const maxDynamicUIUpdatesPerBatch) = 0;
		virtual void OnExit() = 0;

		virtual bool OnLoad(RenderTarget** ppRenderTargets, uint32_t renderTargetCount = 1) = 0;
		virtual void OnUnload() = 0;

		// for GUI with custom shaders not necessary in a normal application
		virtual void SetCustomShader(Shader* pShader) = 0;

		virtual bool AddFont(void* pFontBuffer, uint32_t fontBufferSize, void* pFontGlyphRanges, float fontSize, uintptr_t* pFont) = 0;

		virtual void* GetContext() = 0;

		virtual bool OnUpdate(GUIUpdate* update) = 0;
		virtual void OnDraw(Cmd* q) = 0;

		virtual bool     IsFocused() = 0;
		virtual bool     OnText(const wchar_t* pText) = 0;
		virtual bool     OnButton(uint32_t button, bool press, const Vec2* vec) = 0;
		virtual uint8_t  WantTextInput() const = 0;
	protected:
		// Since gestures events always come first, we want to dismiss any other inputs after that
		bool mHandledGestures;
	};

	struct UIAppImpl
	{
		Renderer* pRenderer;

		eastl::vector<GuiComponent*> components;
		eastl::vector<GuiComponent*> componentsToUpdate;
		bool                         isUpdated;
	};
	
	class UIMiddleware : public IMiddleware
	{
	public:
		UIMiddleware(int32_t const fontAtlasSize = 0, uint32_t const maxDynamicUIUpdatesPerBatch = 20u, uint32_t const fontStashRingSizeBytes = 1024 * 1024);

		virtual bool OnInit(Renderer* pRenderer) override;
		virtual void OnExit() override;

		virtual bool OnLoad(RenderTarget** ppRenderTarget, uint32_t renderTargetCount = 1) override;
		virtual void OnUnload() override;

		virtual bool OnUpdate(float deltaTime) override;
		virtual bool OnDraw(Cmd* pCmd) override;

		//unsigned int LoadFont(const char* filepath);

		GuiComponent* AddGuiComponent(const char* pTitle, const GuiCreateDesc* pDesc);
		void          RemoveGuiComponent(GuiComponent* pComponent);
		void          RemoveAllGuiComponents();

		void          AddUpdateGui(GuiComponent* pGui);

		void    DummyFunc(RenderTarget* rt);

		bool    OnText(const wchar_t* pText) { return pDriver->OnText(pText); }
		bool    OnButton(uint32_t button, bool press, const Vec2* vec) { return pDriver->OnButton(button, press, vec); }
		uint8_t WantTextInput() { return pDriver->WantTextInput(); }
		bool    IsFocused() { return pDriver->IsFocused(); }

		GUIDriver* pDriver; // response for the Gui drawing implementation
		UIAppImpl* pImpl;
		Shader* pCustomShader = nullptr;

		// following var is useful for seeing UI capabilities and tweaking style settings.
		// will only take effect if at least one GUI Component is active.
		bool mShowDemoUiWindow;
	private:
		float mWidth;
		float mHeight;
		int32_t  mFontAtlasSize = 0;
		uint32_t mMaxDynamicUIUpdatesPerBatch = 20;
		uint32_t mFontStashRingSizeBytes = 0;
	};

}