#pragma once

#include "Interface/IMiddleware.h"

namespace SG
{

	class UIMiddleware : public IMiddleware
	{
	public:
		UIMiddleware(int32_t const fontAtlasSize = 0, uint32_t const maxDynamicUIUpdatesPerBatch = 20u, uint32_t const fontStashRingSizeBytes = 1024 * 1024);

		virtual bool OnInit(Renderer* pRenderer) override;
		virtual void OnExit() override;

		virtual bool OnLoad(RenderTarget** ppRenderTargets, uint32_t count = 1) override;
		virtual bool OnUnload() override;

		virtual bool OnUpdate(float deltaTime) override;
		virtual bool OnDraw(Cmd* pCmd) override;

		unsigned int LoadFont(const char* filepath);
	private:
		float mWidth;
		float mHeight;
		int32_t  mFontAtlasSize;
		uint32_t mMaxDynamicUIUpdatesPerBatch;
		uint32_t mFontStashRingSizeBytes;
	};

}