#pragma once

#include "Manager.h"

namespace Hooks::Attach
{
	namespace ObjectReference
	{
		template <class T>
		struct Load3D
		{
			static RE::NiAVObject* thunk(T* a_this, bool a_backgroundLoading)
			{
				auto node = func(a_this, a_backgroundLoading);
				if (node) {
					if (auto baseObject = a_this->GetObjectReference()) {
						LightManager::GetSingleton()->AddLights(a_this, baseObject, node);
					}
				}
				return node;
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t                   idx{ 0x6A };

			static void Install()
			{
				stl::write_vfunc<T, Load3D>();
				logger::info("Hooked {}::Load3D"sv, typeid(T).name());
			}
		};
	}

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
