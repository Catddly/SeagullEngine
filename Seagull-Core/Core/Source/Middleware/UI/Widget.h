#pragma once

#include "BasicType/ISingleton.h"
#include "Math/MathTypes.h"

#include <include/EASTL/string.h>
#include <include/EASTL/vector.h>

#include "Interface/IMemory.h"

namespace SG
{

	typedef void(*WidgetCallbackFunc)();

	/// a widget is a small component of a window
	/// inherit form singleton to disable common copy
	class IWidget : public ISingleton
	{
	public:
		IWidget(const eastl::string& label)
			:mLabel(label),
			pOnHover(nullptr), pOnActive(nullptr), pOnFocus(nullptr), pOnEdited(nullptr), pOnDeactivated(nullptr), pOnDeactivatedAfterEdit(nullptr),
			useDeferred(false), isHovered(false), isActive(false), isFocused(false), isEdited(false), isDeactivated(false), isDeactivatedAfterEdit(false){}
		virtual ~IWidget() = default;

		virtual IWidget* OnCopy() const = 0;
		virtual void OnDraw() = 0;

		void ProcessCallback(bool useDeferred = false);

		/// widget states
		// common callbacks that can be used by the clients
		WidgetCallbackFunc pOnHover;       // Widget is hovered, usable, and not blocked by anything
		WidgetCallbackFunc pOnActive;      // Widget is currently active (ex. button being held) (suck as we click this widget)
		WidgetCallbackFunc pOnFocus;       // Widget is currently focused (for keyboard/gamepad nav)
		WidgetCallbackFunc pOnEdited;      // Widget just changed its underlying value or was pressed
		WidgetCallbackFunc pOnDeactivated; // Widget was just made inactive from an active state. this is useful for undo/redo patterns.
		WidgetCallbackFunc pOnDeactivatedAfterEdit; // Widget was just made inactive from an active state and changed its underlying value.  This is useful for undo/redo patterns.

		bool useDeferred; // set this to process deferred callbacks that may cause global program state changes.s
		bool isHovered;
		bool isActive;
		bool isFocused;
		bool isEdited;
		bool isDeactivated;
		bool isDeactivatedAfterEdit;

		eastl::string mLabel;
	protected:
		inline void CopyBase(IWidget* other) const
		{
			other->pOnHover = pOnHover;
			other->pOnActive = pOnActive;
			other->pOnFocus = pOnFocus;
			other->pOnEdited = pOnEdited;
			other->pOnDeactivated = pOnDeactivated;
			other->pOnDeactivatedAfterEdit = pOnDeactivatedAfterEdit;

			other->useDeferred = useDeferred;
		}
	};

#pragma region (DockSpace)

	class ViewportWidget : public IWidget
	{
	public:
		ViewportWidget(const eastl::string& label, void* texture, const Vec2& viewportSize,
			const Vec2& uv0 = { 0, 0 }, const Vec2& uv1 = { 1, 1 })
			:IWidget(label), mTexture(texture), mSize(viewportSize), mUV0(uv0), mUV1(uv1) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		void* mTexture;
		Vec2  mSize;
		Vec2  mUV0;
		Vec2  mUV1;
	};

#pragma endregion (DockSpace)

#pragma region (Labels)

	class LabelWidget : public IWidget
	{
	public:
		LabelWidget(const eastl::string& label)
			:IWidget(label) {}
			
		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	};

	class ColorLabelWidget : public IWidget
	{
	public:
		ColorLabelWidget(const eastl::string& label, const Vec4& color)
			:IWidget(label), mColor(color) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		Vec4 mColor;
	};

#pragma endregion (Labels)

#pragma region (Buttons)

	class ButtonWidget : public IWidget
	{
	public:
		ButtonWidget(const eastl::string& label)
			:IWidget(label) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	};

	class RadioButtonWidget : public IWidget
	{
	public:
		RadioButtonWidget(const eastl::string& label, int32_t* data, const int32_t radioId) :
			IWidget(label),
			pData(data),
			mRadioId(radioId) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		int32_t* pData;
		int32_t  mRadioId;
	};

#pragma endregion (Buttons)

#pragma region (Slider)

	class SliderFloatWidget : public IWidget
	{
	public:
		SliderFloatWidget(
			const eastl::string& label, float* data, float min, float max, float step = 0.01f, const eastl::string& format = "%.3f") :
			IWidget(label), mFormat(format), pData(data),
			mMin(min), mMax(max), mStep(step) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		eastl::string mFormat;
		float* pData;
		float  mMin;
		float  mMax;
		float  mStep;
	};

	class SliderFloat2Widget : public IWidget
	{
	public:
		SliderFloat2Widget(
			const eastl::string& label, Vec2* data, const Vec2& min, const Vec2& max, const Vec2& step = Vec2(0.01f, 0.01f),
			const eastl::string& format = "%.3f") :
			IWidget(label), mFormat(format), pData(data),
			mMin(min), mMax(max), mStep(step) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		eastl::string mFormat;
		Vec2* pData;
		Vec2  mMin;
		Vec2  mMax;
		Vec2  mStep;
	};

	class SliderFloat3Widget : public IWidget
	{
	public:
		SliderFloat3Widget(
			const eastl::string& label, Vec3* data, const Vec3& min, const Vec3& max,
			const Vec3& step = Vec3(0.01f, 0.01f, 0.01f), const eastl::string& format = "%.3f") :
			IWidget(label), mFormat(format),pData(data),
			mMin(min), mMax(max), mStep(step) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		eastl::string mFormat;
		Vec3* pData;
		Vec3  mMin;
		Vec3  mMax;
		Vec3  mStep;
	};

	class SliderFloat4Widget : public IWidget
	{
	public:
		SliderFloat4Widget(
			const eastl::string& label, Vec4* data, const Vec4& min, const Vec4& max,
			const Vec4& step = Vec4(0.01f, 0.01f, 0.01f, 0.01f), const eastl::string& format = "%.3f") :
			IWidget(label), mFormat(format), pData(data),
			mMin(min), mMax(max), mStep(step) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		eastl::string mFormat;
		Vec4* pData;
		Vec4  mMin;
		Vec4  mMax;
		Vec4  mStep;
	};

	class SliderIntWidget : public IWidget
	{
	public:
		SliderIntWidget(
			const eastl::string& label, int32_t* data, int32_t min, int32_t max, int32_t step = 1,
			const eastl::string& format = "%d") :
			IWidget(label), mFormat(format), pData(data),
			mMin(min), mMax(max), mStep(step) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		eastl::string mFormat;
		int32_t* pData;
		int32_t  mMin;
		int32_t  mMax;
		int32_t  mStep;
	};

	class SliderUintWidget : public IWidget
	{
	public:
		SliderUintWidget(
			const eastl::string& label, uint32_t* data, uint32_t min, uint32_t max, uint32_t step = 1,
			const eastl::string& format = "%d") :
			IWidget(label), mFormat(format), pData(data),
			mMin(min), mMax(max), mStep(step) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		eastl::string mFormat;
		uint32_t* pData;
		uint32_t  mMin;
		uint32_t  mMax;
		uint32_t  mStep;
	};

#pragma endregion (Slider)

#pragma region (Checkbox)

	class CheckboxWidget : public IWidget
	{
	public:
		CheckboxWidget(const eastl::string& label, bool* data) : IWidget(label), pData(data) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		bool* pData;
	};

	class OneLineCheckboxWidget : public IWidget
	{
	public:
		OneLineCheckboxWidget(const eastl::string& label, bool* data, const uint32_t& color) : IWidget(label), pData(data), mColor(color) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		bool* pData;
		uint32_t mColor;
	};

#pragma endregion (Checkbox)

#pragma region (Utility)

	class CursorLocationWidget : public IWidget
	{
	public:
		CursorLocationWidget(const eastl::string& label, const Vec2& location) : IWidget(label), mLocation(location) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		Vec2 mLocation;
	};

	class DropdownWidget : public IWidget
	{
	public:
		DropdownWidget(const eastl::string& label, uint32_t* data, const char** names, const uint32_t* values, uint32_t count) :
			IWidget(label),
			pData(data)
		{
			mValues.resize(count);
			mNames.resize(count);
			for (uint32_t i = 0; i < count; ++i)
			{
				mValues[i] = values[i];
				mNames[i] = names[i];
			}
		}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		uint32_t* pData;
		eastl::vector<uint32_t>      mValues;
		eastl::vector<eastl::string> mNames;
	};

	class ColumnWidget : public IWidget
	{
	public:
		ColumnWidget(const eastl::string& label, const eastl::vector<IWidget*>& perColWidgets) : IWidget(label)
		{
			mNumColumns = (uint32_t)perColWidgets.size();
			for (uint32_t i = 0; i < perColWidgets.size(); ++i)
			{
				mPerColumnWidgets.push_back(perColWidgets[i]);
			}
		}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		eastl::vector<IWidget*> mPerColumnWidgets;
		uint32_t mNumColumns;
	};

	class ProgressBarWidget : public IWidget
	{
	public:
		ProgressBarWidget(const eastl::string& label, size_t* data, size_t const maxProgress) :
			IWidget(label), pData(data), mMaxProgress(maxProgress) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		size_t* pData;
		size_t  mMaxProgress;
	};

#pragma endregion (Utility)

#pragma region (Color Relative)

	class ColorPickerWidget : public IWidget
	{
	public:
		ColorPickerWidget(const eastl::string& label, uint32_t* data) : IWidget(label), pData(data) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		uint32_t* pData;
	};

	class ColorSliderWidget : public IWidget
	{
	public:
		ColorSliderWidget(const eastl::string& label, uint32_t* data) : IWidget(label), pData(data) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		uint32_t* pData;
	};

#pragma endregion (Color Relative)

#pragma region (Text)

	class TextboxWidget : public IWidget
	{
	public:
		TextboxWidget(const eastl::string& label, char* data, uint32_t const length, bool const autoSelectAll = true) :
			IWidget(label),
			pData(data),
			mLength(length),
			mAutoSelectAll(autoSelectAll) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		char* pData;
		uint32_t mLength;
		bool     mAutoSelectAll; // when we selected the textbox, it will automatically select all at the very beginning
	};

	class DrawTextWidget : public IWidget
	{
	public:
		DrawTextWidget(const eastl::string& label, const Vec2& pos, const uint32_t& colorHex) :
			IWidget(label),
			mPos(pos),
			mColor(colorHex) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		Vec2 mPos;
		uint32_t mColor;
	};

#pragma endregion (Text)

#pragma region (Lines)

	class DrawLineWidget : public IWidget
	{
	public:
		DrawLineWidget(const eastl::string& label, const Vec2& pos1, const Vec2& pos2, const uint32_t& colorHex, const bool& addItem) :
			IWidget(label),
			mPos1(pos1),
			mPos2(pos2),
			mColor(colorHex),
			mAddItem(addItem)
		{}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		Vec2 mPos1;
		Vec2 mPos2;
		uint32_t mColor;
		bool mAddItem;
	};

	class DrawCurveWidget : public IWidget
	{
	public:
		DrawCurveWidget(const eastl::string& label, Vec2* positions, uint32_t numPoints, float thickness, const uint32_t& colorHex) :
			IWidget(label),
			mPos(positions),
			mNumPoints(numPoints),
			mThickness(thickness),
			mColor(colorHex) {}

		virtual IWidget* OnCopy() const override;
		virtual void OnDraw() override;
	protected:
		Vec2* mPos;
		uint32_t mNumPoints;
		float mThickness;
		uint32_t mColor;
	};

#pragma endregion (Lines)

}