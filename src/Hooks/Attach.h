#pragma once

#include "Manager.h"

namespace Hooks::Attach
{
	namespace ReferenceEffect
	{
		template <class T>
		struct Init
		{
			static bool thunk(T* a_this)
			{
				auto result = func(a_this);

				if (result) {
					RE::FormID effectID = 0;
					if constexpr (std::is_same_v<RE::ShaderReferenceEffect, T>) {
						if (a_this->effectData) {
							effectID = a_this->effectData->GetFormID();
						}
					} else if constexpr (std::is_same_v<RE::ModelReferenceEffect, T>) {
						if (a_this->artObject) {
							effectID = a_this->artObject->GetFormID();
						}
					}
					LightManager::GetSingleton()->AddReferenceEffectLights(a_this, effectID);
				}

				return result;
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t                   idx{ 0x36 };

			static void Install()
			{
				stl::write_vfunc<T, Init>();
				logger::info("Hooked {}::Init"sv, typeid(T).name());
			}
		};
	}

	void Install();
}
