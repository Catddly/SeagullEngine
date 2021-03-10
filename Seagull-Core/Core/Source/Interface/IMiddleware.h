#pragma once

#include <stdint.h>

namespace SG
{

	struct RenderTarget;
	struct Cmd;
	struct Renderer;

	class IMiddleware
	{
	public:
		virtual bool OnInit(Renderer* pRenderer) = 0;
		virtual void OnExit() = 0;

		virtual bool OnLoad(RenderTarget** ppRenderTargets, uint32_t count = 1) = 0;
		virtual bool OnUnload() = 0;

		virtual bool OnUpdate(float deltaTime) = 0;
		virtual bool OnDraw(Cmd* pCmd) = 0;
	};

}