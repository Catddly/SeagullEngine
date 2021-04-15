#pragma once

namespace SG
{

	template <typename T>
	class ISingleton
	{
	protected:
		ISingleton() = default;
		virtual ~ISingleton() = default;
	private:
		ISingleton(const ISingleton&) {}
		ISingleton& operator=(const ISingleton&) {}
	public:
		static T* GetInstance()
		{
			static T instance;
			return &instance;
		}
	};

}