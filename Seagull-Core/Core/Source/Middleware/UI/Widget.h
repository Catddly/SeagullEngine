#pragma once

#include "Math/MathTypes.h"
#include "BasicType/ISingleton.h"

#include <include/EASTL/string.h>

namespace SG
{

	typedef void(*WidgetCallbackFunc)();

	/// every "window" is a widget
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

#pragma region (Labels)

	class LabelWidget : public IWidget
	{
	public:
		LabelWidget(const eastl::string& label)
			:IWidget(label) {}
		~LabelWidget() = default;
			
		virtual IWidget* OnCopy() const override {}
		virtual void OnDraw() override {}
	};

	class ColorLabelWidget : public IWidget
	{
	public:
		ColorLabelWidget(const eastl::string& label, const Vec4& color)
			:IWidget(label), mColor(color) {}
		~ColorLabelWidget() = default;

		virtual IWidget* OnCopy() const override {}
		virtual void OnDraw() override {}
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
		~ButtonWidget() = default;

		virtual IWidget* OnCopy() const override {}
		virtual void OnDraw() override {}
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
		~SliderFloatWidget() = default;

		virtual IWidget* OnCopy() const override {}
		virtual void OnDraw() override {}
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
		~SliderFloat2Widget() = default;

		virtual IWidget* OnCopy() const override {}
		virtual void OnDraw() override {}
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
			IWidget(label),
			mFormat(format),
			pData(data),
			mMin(min),
			mMax(max),
			mStep(step)
		{
		}
		~SliderFloat3Widget() = default;

		virtual IWidget* OnCopy() const override {}
		virtual void OnDraw() override {}
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
			IWidget(label),
			mFormat(format),
			pData(data),
			mMin(min),
			mMax(max),
			mStep(step)
		{
		}
		~SliderFloat4Widget() = default;

		virtual IWidget* OnCopy() const override {}
		virtual void OnDraw() override {}
	protected:
		eastl::string mFormat;
		Vec4* pData;
		Vec4  mMin;
		Vec4  mMax;
		Vec4  mStep;
	};

#pragma endregion (Slider)

}