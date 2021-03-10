#pragma once

namespace SG
{

	class ISingleton
	{
	protected:
		ISingleton() = default;
		virtual ~ISingleton() = default;
	private:
		ISingleton(const ISingleton&) = delete;
		ISingleton(ISingleton&&) = delete;
		ISingleton& operator=(const ISingleton&) = delete;
		ISingleton& operator=(ISingleton&&) = delete;
	};

}