#pragma once

#include <stdint.h>

#include "Math/MathTypes.h"

namespace SG
{

	struct Renderer;
	struct RenderTarget;
	struct PipelineCache;

	typedef struct TextDrawDesc
	{
		TextDrawDesc(unsigned int font = 0, uint32_t color = 0xffffffff, float size = 15.0f, float spacing = 0.0f, float blur = 0.0f) :
			fontID(font),
			fontColor(color),
			fontSize(size),
			fontSpacing(spacing),
			fontBlur(blur) 
		{}

		uint32_t fontID;
		uint32_t fontColor;
		float    fontSize;
		float    fontSpacing;
		float    fontBlur;
	} TextDrawDesc;

	class FontStash
	{
	public:
		virtual ~FontStash() = default;

		bool OnInit(Renderer* pRenderer, uint32_t width, uint32_t height, uint32_t ringSizeBytes);
		void OnExit();

		bool OnLoad(RenderTarget** pRts, uint32_t count, PipelineCache* pCache);
		void OnUnload();

		//! Makes a font available to the font stash.
		//! - Fonts can not be undefined in a FontStash due to its dynamic nature (once packed into an atlas, they cannot be unpacked, unless it is fully rebuilt)
		//! - Defined fonts will automatically be unloaded when the Fontstash is destroyed.
		//! - When it is paramount to be able to unload individual fonts, use multiple fontstashes.
		int DefineFont(const char* identification, const char* pFontPath);

		void*    GetFontBuffer(uint32_t fontId);
		uint32_t GetFontBufferSize(uint32_t fontId);

		//! Draw text.
		void OnDrawText(
			struct Cmd* pCmd, const char* message, float x, float y, int fontID, unsigned int color = 0xffffffff, float size = 16.0f,
			float spacing = 0.0f, float blur = 0.0f);

		//! Draw text in worldSpace.
		void OnDrawText(
			struct Cmd* pCmd, const char* message, const Matrix4& projView, const Matrix4& worldMat, int fontID, unsigned int color = 0xffffffff,
			float size = 16.0f, float spacing = 0.0f, float blur = 0.0f);

		//! Measure text boundaries. Results will be written to outBounds (x,y,x2,y2).
		float MeasureText(
			float* outBounds, const char* message, float x, float y, int fontID, unsigned int color = 0xffffffff, float size = 16.0f,
			float spacing = 0.0f, float blur = 0.0f);
	protected:
		float fontMaxSize;
		class FontStash_Impl* impl;
	};

}