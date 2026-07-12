#pragma once

#include "Manager.h"

namespace Hooks::Detach
{
	// clears light from the shadowscenenode + nilight ptr
	template <std::size_t N>
	struct GetLightData
	{
		static RE::REFR_LIGHT* thunk(RE::ExtraDataList* a_list)
		{
			if (auto* ref = stl::adjust_pointer<RE::TESObjectREFR>(a_list, -0x70)) {
				LightManager::GetSingleton()->DetachLights(ref, true);
			}

			return func(a_list);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install();
	};

	// casting art
	template <std::size_t N>
	struct BGSAttachTechniques__DetachItem
	{
		static bool thunk(RE::RefAttachTechniqueInput& a_this)
		{
			auto actorMagicCaster = stl::adjust_pointer<RE::ActorMagicCaster>(&a_this, -static_cast<std::ptrdiff_t>(offsetof(RE::ActorMagicCaster, RE::ActorMagicCaster::castingArtData)));
			LightManager::GetSingleton()->DetachCastingLights(actorMagicCaster);

			return func(a_this);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};
	
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

	void Install_GetLightData();
	void Install_BGSAttachTechniques__DetachItem();

	void Install();
}
