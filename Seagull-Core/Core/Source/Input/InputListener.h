#pragma once

#include "InputListenerImpl.h"

namespace SG
{

	class InputListener
	{
	public:
		virtual bool Init() = 0;
		virtual void Exit() = 0;

		virtual void OnUpdate() = 0;
	private:
		InputListenerImpl* mImpl = nullptr;
	};

}