#pragma once

#include "Manager.h"

namespace Hooks::Detach
{
	namespace BSTempEffect
	{
		template <class T>
		struct DetachImpl
		{
			static void thunk(T* a_this)
			{
				func(a_this);
				LightManager::GetSingleton()->DetachTempEffectLights(a_this, true);
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t                   idx{ 0x3E };

			static void Install()
			{
				stl::write_vfunc<T, DetachImpl>();
				logger::info("Hooked {}::DetachImpl"sv, typeid(T).name());
			}
		};
	}

	void Install();
}
