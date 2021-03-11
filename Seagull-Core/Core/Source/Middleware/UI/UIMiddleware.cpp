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
		return true;
	}

	void UIMiddleware::OnExit()
	{
	}

	bool UIMiddleware::OnLoad(RenderTarget** ppRenderTargets, uint32_t count /*= 1*/)
	{
		return true;
	}

	bool UIMiddleware::OnUnload()
	{
		return true;
	}

	bool UIMiddleware::OnUpdate(float deltaTime)
	{
		return true;
	}

	bool UIMiddleware::OnDraw(Cmd* pCmd)
	{
		return true;
	}

	unsigned int UIMiddleware::LoadFont(const char* filepath)
	{
		return 0;
	}

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



}