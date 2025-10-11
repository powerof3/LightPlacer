#pragma once

#include "Manager.h"

namespace Hooks::Detach
{
	namespace BSTempEffect
	{
		template <class T>
		struct Detach
		{
			static void thunk(T* a_this)
			{
				LightManager::GetSingleton()->DetachReferenceEffectLights(a_this, true);
				
				func(a_this);
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t                   idx{ 0x27 };

			static void Install()
			{
				stl::write_vfunc<T, Detach>();
				logger::info("Hooked {}::Detach"sv, typeid(T).name());
			}
		};
	}

	void Install();
}
