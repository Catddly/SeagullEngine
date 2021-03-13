#include "Interface/ILog.h"

#include "UIMiddleware.h"

namespace SG
{

	extern void init_gui_driver(GUIDriver** ppDriver);
	extern void delete_gui_driver(GUIDriver* pDriver);

	UIMiddleware::UIMiddleware(int32_t const fontAtlasSize, uint32_t const maxDynamicUIUpdatesPerBatch, uint32_t const fontStashRingSizeBytes)
		:mFontAtlasSize(fontAtlasSize), mMaxDynamicUIUpdatesPerBatch(maxDynamicUIUpdatesPerBatch), mFontStashRingSizeBytes(fontStashRingSizeBytes)
	{
	}

	bool UIMiddleware::OnInit(Renderer* pRenderer)
	{
		mShowDemoUiWindow = false;

		pImpl = sg_new(UIAppImpl);
		pImpl->pRenderer = pRenderer;// renderer receive from outer space
		pDriver = nullptr;

		if (mFontAtlasSize <= 0) // then we assume we'll only draw debug text in the UI, in which case the atlas size can be kept small
			mFontAtlasSize = 256;

		init_gui_driver(&pDriver);
		if (pCustomShader)
			pDriver->SetCustomShader(pCustomShader);

		return pDriver->OnInit(pImpl->pRenderer, mMaxDynamicUIUpdatesPerBatch);
	}

	void UIMiddleware::OnExit()
	{
		RemoveAllGuiComponents();

		pDriver->OnExit();
		delete_gui_driver(pDriver);
		pDriver = nullptr;

		sg_delete(pImpl);
		pImpl = nullptr;
	}

	bool UIMiddleware::OnLoad(RenderTarget** ppRenderTargets, uint32_t renderTargetCount)
	{
		ASSERT(ppRenderTargets && ppRenderTargets[0]);

		mWidth = (float)ppRenderTargets[0]->width;
		mHeight = (float)ppRenderTargets[0]->height;
		SG_LOG_DEBUG("width: %f, heihgt: %f", mWidth, mHeight);

		return pDriver->OnLoad(ppRenderTargets, renderTargetCount);
	}

	void UIMiddleware::OnUnload()
	{
		pDriver->OnUnload();
	}

	bool UIMiddleware::OnUpdate(float deltaTime)
	{
		if (pImpl->isUpdated || !pImpl->componentsToUpdate.size())
			return false;

		pImpl->isUpdated = true;

		eastl::vector<GuiComponent*> activeComponents(pImpl->componentsToUpdate.size());
		uint32_t                     activeComponentCount = 0;
		for (uint32_t i = 0; i < (uint32_t)pImpl->componentsToUpdate.size(); ++i)
			if (pImpl->componentsToUpdate[i]->isActive)
				activeComponents[activeComponentCount++] = pImpl->componentsToUpdate[i];

		// update all the active gui
		GUIDriver::GUIUpdate guiUpdate { activeComponents.data(), activeComponentCount, deltaTime, mWidth, mHeight, mShowDemoUiWindow };
		pDriver->OnUpdate(&guiUpdate);

		pImpl->componentsToUpdate.clear();
		return true;
	}

	bool UIMiddleware::OnDraw(Cmd* pCmd)
	{
		if (pImpl->isUpdated) // after the update
		{
			pImpl->isUpdated = false;
			pDriver->OnDraw(pCmd);
		}
		return true;
	}

	//unsigned int UIMiddleware::LoadFont(const char* filepath)
	//{
	//	return 0;
	//}

	SG::GuiComponent* UIMiddleware::AddGuiComponent(const char* pTitle, const GuiCreateDesc* pDesc)
	{
		GuiComponent* pComponent = sg_placement_new<GuiComponent>(sg_calloc(1, sizeof(GuiComponent)));
		pComponent->hasCloseButton = false;
		pComponent->flags = SG_GUI_FLAGS_ALWAYS_AUTO_RESIZE;
		pComponent->initialWindowRect = { pDesc->startPosition.x, pDesc->startPosition.y, pDesc->startSize.x, pDesc->startSize.y };

		pComponent->isActive = true;
		pComponent->title = pTitle;
		pComponent->alpha = 1.0f;
		pImpl->components.emplace_back(pComponent);

		return pComponent;
	}

	void UIMiddleware::RemoveGuiComponent(GuiComponent* pComponent)
	{
		ASSERT(pComponent);

		pComponent->RemoveAllWidgets();
		GuiComponent** it = eastl::find(pImpl->components.begin(), pImpl->components.end(), pComponent);
		if (it != pImpl->components.end())
		{
			(*it)->RemoveAllWidgets();
			pImpl->components.erase(it);
			GuiComponent** activeIter = eastl::find(pImpl->componentsToUpdate.begin(), pImpl->componentsToUpdate.end(), pComponent);
			if (activeIter != pImpl->componentsToUpdate.end())
				pImpl->componentsToUpdate.erase(activeIter);
			pComponent->widgets.clear();
		}

		pComponent->~GuiComponent();
		sg_free(pComponent);
	}

	void UIMiddleware::RemoveAllGuiComponents()
	{
		for (uint32_t i = 0; i < (uint32_t)pImpl->components.size(); ++i)
		{
			pImpl->components[i]->RemoveAllWidgets();
			pImpl->components[i]->~GuiComponent();
			sg_free(pImpl->components[i]);
		}

		pImpl->components.clear();
		pImpl->componentsToUpdate.clear();
	}

	void UIMiddleware::AddUpdateGui(GuiComponent* pGui)
	{
		pImpl->componentsToUpdate.emplace_back(pGui);
	}

	void UIMiddleware::DummyFunc(RenderTarget* rt)
	{
		uint32_t w = rt->width;
		uint32_t h = rt->height;

		SG_LOG_DEBUG("Width: %d", rt->width);
		SG_LOG_DEBUG("Height: %d", rt->height);
	}

	IWidget* GuiComponent::AddWidget(const IWidget& widget, bool clone /* = true*/)
	{
		widgets.emplace_back((clone ? widget.OnCopy() : (IWidget*)&widget));
		widgetsCloned.emplace_back(clone);
		return widgets.back();
	}

	void GuiComponent::RemoveWidget(IWidget* pWidget)
	{
		decltype(widgets)::iterator it = eastl::find(widgets.begin(), widgets.end(), pWidget);
		if (it != widgets.end())
		{
			IWidget* pWidget = *it;
			if (widgetsCloned[it - widgets.begin()]) // if it is the copied widget, we need to free its memory
			{
				pWidget->~IWidget();
				sg_free(pWidget);
			}
			widgetsCloned.erase(widgetsCloned.begin() + (it - widgets.begin()));
			widgets.erase(it);
		}
	}

	void GuiComponent::RemoveAllWidgets()
	{
		for (uint32_t i = 0; i < (uint32_t)widgets.size(); ++i)
		{
			if (widgetsCloned[i])
			{
				widgets[i]->~IWidget();
				sg_free(widgets[i]);
			}
		}

		widgets.clear();
		widgetsCloned.clear();
	}

#pragma region (Widget Copy Funcs)

	IWidget* LabelWidget::OnCopy() const
	{
		LabelWidget* pWidget = sg_placement_new<LabelWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* ColorLabelWidget::OnCopy() const
	{
		ColorLabelWidget* pWidget = sg_placement_new<ColorLabelWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->mColor);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* ButtonWidget::OnCopy() const
	{
		ButtonWidget* pWidget = sg_placement_new<ButtonWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* RadioButtonWidget::OnCopy() const
	{
		RadioButtonWidget* pWidget = sg_placement_new<RadioButtonWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mRadioId);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* SliderFloatWidget::OnCopy() const
	{
		SliderFloatWidget* pWidget = sg_placement_new<SliderFloatWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData,
			this->mMin, this->mMax, this->mStep, this->mFormat);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* SliderFloat2Widget::OnCopy() const
	{
		SliderFloat2Widget* pWidget = sg_placement_new<SliderFloat2Widget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData,
			this->mMin, this->mMax, this->mStep, this->mFormat);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* SliderFloat3Widget::OnCopy() const
	{
		SliderFloat3Widget* pWidget = sg_placement_new<SliderFloat3Widget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData,
			this->mMin, this->mMax, this->mStep, this->mFormat);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* SliderFloat4Widget::OnCopy() const
	{
		SliderFloat4Widget* pWidget = sg_placement_new<SliderFloat4Widget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData,
			this->mMin, this->mMax, this->mStep, this->mFormat);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* SliderIntWidget::OnCopy() const
	{
		SliderIntWidget* pWidget = sg_placement_new<SliderIntWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData,
			this->mMin, this->mMax, this->mStep, this->mFormat);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* SliderUintWidget::OnCopy() const
	{
		SliderUintWidget* pWidget = sg_placement_new<SliderUintWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData,
			this->mMin, this->mMax, this->mStep, this->mFormat);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* CheckboxWidget::OnCopy() const
	{
		CheckboxWidget* pWidget = sg_placement_new<CheckboxWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* OneLineCheckboxWidget::OnCopy() const
	{
		OneLineCheckboxWidget* pWidget = sg_placement_new<OneLineCheckboxWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mColor);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* CursorLocationWidget::OnCopy() const
	{
		CursorLocationWidget* pWidget = sg_placement_new<CursorLocationWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->mLocation);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* DropdownWidget::OnCopy() const
	{
		const char** ppNames = (const char**)alloca(mValues.size() * sizeof(const char*));
		for (uint32_t i = 0; i < (uint32_t)mValues.size(); ++i)
			ppNames[i] = mNames[i].c_str();
		DropdownWidget* pWidget = sg_placement_new<DropdownWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, ppNames,
			this->mValues.data(), this->mValues.size());
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* ColumnWidget::OnCopy() const
	{
		ColumnWidget* pWidget = sg_placement_new<ColumnWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->mPerColumnWidgets);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* ProgressBarWidget::OnCopy() const
	{
		ProgressBarWidget* pWidget = sg_placement_new<ProgressBarWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mMaxProgress);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* ColorPickerWidget::OnCopy() const
	{
		ColorPickerWidget* pWidget = sg_placement_new<ColorPickerWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* ColorSliderWidget::OnCopy() const
	{
		ColorSliderWidget* pWidget = sg_placement_new<ColorSliderWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* TextboxWidget::OnCopy() const
	{
		TextboxWidget* pWidget = sg_placement_new<TextboxWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mLength, this->mAutoSelectAll);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* DrawTextWidget::OnCopy() const
	{
		DrawTextWidget* pWidget = sg_placement_new<DrawTextWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->mPos, this->mColor);
		CopyBase(pWidget);
		return pWidget;
	}

	IWidget* DrawLineWidget::OnCopy() const
	{
		DrawLineWidget* pWidget = sg_placement_new<DrawLineWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->mPos1, this->mPos2,
			this->mColor, this->mAddItem);
		CopyBase(pWidget);
		return pWidget;
	}
	
	IWidget* DrawCurveWidget::OnCopy() const
	{
		DrawCurveWidget* pWidget = sg_placement_new<DrawCurveWidget>(sg_calloc(1, sizeof(*pWidget)), this->mLabel, this->mPos, this->mNumPoints,
			this->mThickness, this->mColor);
		CopyBase(pWidget);
		return pWidget;
	}

#pragma endregion (Widget Copy Funcs)

}