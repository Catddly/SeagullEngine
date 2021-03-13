#pragma once

#include <stdint.h>

namespace SG
{

	struct Renderer;
	struct RenderTarget;
	struct Cmd;

	class IMiddleware
	{
	public:
		virtual bool OnInit(Renderer* pRenderer) = 0;
		virtual void OnExit() = 0;

		virtual bool OnLoad(RenderTarget** ppRenderTarget, uint32_t renderTargetCount = 1) = 0;
		virtual void OnUnload() = 0;

		virtual bool OnUpdate(float deltaTime) = 0;
		virtual bool OnDraw(Cmd* pCmd) = 0;
	};

}