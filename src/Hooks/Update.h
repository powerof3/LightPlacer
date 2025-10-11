#pragma once

#include "Manager.h"

namespace Hooks::Update
{
	namespace ReferenceEffect
	{
		template <class T>
		struct UpdatePosition
		{
			static void thunk(T* a_this)
			{
				func(a_this);

				LightManager::GetSingleton()->UpdateReferenceEffectLights(a_this);
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t                   idx{ 0x3B };

			static void Install()
			{
				stl::write_vfunc<T, UpdatePosition>();
				logger::info("Hooked {}::UpdatePosition"sv, typeid(T).name());
			}
		};
	}

	void Install();
}
