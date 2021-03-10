#include "UIMiddleware.h"

namespace SG
{

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

}