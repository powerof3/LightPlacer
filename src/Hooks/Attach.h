#pragma once

#include "Manager.h"

namespace Hooks::Attach
{
	// adds light on 3d load
	template <class T, std::size_t index = 0>
	struct Clone3D
	{
		static RE::NiAVObject* thunk(T* a_this, RE::TESObjectREFR* a_ref)
		{
			auto node = func(a_this, a_ref);
			if constexpr (std::is_same_v<RE::BGSMovableStatic, T>) {
				LightManager::GetSingleton()->AddLights(a_ref, a_ref ? a_ref->GetBaseObject() : nullptr, node);
			} else {
				LightManager::GetSingleton()->AddLights(a_ref, a_this, node);
			}
			return node;
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline constexpr std::size_t            size{ 0x4A };

		static void Install()
		{
			stl::write_vfunc<T, index, Clone3D>();
			logger::info("Hooked {}::Clone3D"sv, typeid(T).name());
		}
	};

	namespace BSTempEffect
	{
		template <class T>
		struct Init
		{
			static void thunk(T* a_this)
			{
				func(a_this);
				
				RE::FormID effectID = 0;
				if constexpr (std::is_same_v<RE::ShaderReferenceEffect, T>) {
					effectID = a_this->effectData ? a_this->effectData->GetFormID() : 0;
				} else if constexpr (std::is_same_v<RE::ModelReferenceEffect, T>) {
					effectID = a_this->artObject ? a_this->artObject->GetFormID() : 0;
				}
				LightManager::GetSingleton()->AddTempEffectLights(a_this, effectID);
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static inline constexpr std::size_t            size{ 0x36 };

			static void Install()
			{
				stl::write_vfunc<T, Init>();
				logger::info("Hooked {}::Init"sv, typeid(T).name());
			}
		};
	}
}
