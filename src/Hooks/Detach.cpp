#include "Detach.h"

namespace Hooks::Detach
{
	// clears light from the shadowscenenode
	struct RemoveLight
	{
		static void thunk(RE::TESObjectREFR* a_this, bool a_isMagicLight)
		{
			LightManager::GetSingleton()->DetachLights(a_this, false);

			func(a_this, a_isMagicLight);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(19253, 19679) };
			stl::hook_function_prologue<RemoveLight, 8>(target.address());

			logger::info("Hooked TESObjectREFR::RemoveLight");
		}
	};

	struct RunBiped3DDetach
	{
		static void thunk(const RE::ActorHandle& a_handle, RE::NiAVObject* a_node)
		{
			LightManager::GetSingleton()->DetachWornLights(a_handle, a_node);

			func(a_handle, a_node);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(15495, 15660) };
			stl::hook_function_prologue<RunBiped3DDetach, 5>(target.address());

			logger::info("Hooked BipedAnim::RunBiped3DDetach");
		}
	};

	struct Hazard__Release3DRelatedData
	{
		static void thunk(RE::Hazard* a_this)
		{
			LightManager::GetSingleton()->DetachHazardLights(a_this);

			func(a_this);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr std::size_t                   idx{ 0x6B };

		static void Install()
		{
			stl::write_vfunc<RE::Hazard, Hazard__Release3DRelatedData>();
			logger::info("Hooked Hazard::Release3DRelatedData");
		}
	};

	struct Explosion__Release3DRelatedData
	{
		static void thunk(RE::Explosion* a_this)
		{
			LightManager::GetSingleton()->DetachExplosionLights(a_this);

			func(a_this);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr std::size_t                   idx{ 0x6B };

		static void Install()
		{
			stl::write_vfunc<RE::Explosion, Explosion__Release3DRelatedData>();
			logger::info("Hooked Explosion::Release3DRelatedData");
		}
	};

	struct ShaderReferenceEffect_Suspend
	{
		static void thunk(RE::ShaderReferenceEffect* a_this)
		{
			bool suspended = a_this->flags.any(RE::ShaderReferenceEffect::Flag::kSuspended);
			func(a_this);
			if (suspended != a_this->flags.any(RE::ShaderReferenceEffect::Flag::kSuspended)) {
				LightManager::GetSingleton()->DetachReferenceEffectLights(a_this, false);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr std::size_t                   idx{ 0x37 };

		static void Install()
		{
			stl::write_vfunc<RE::ShaderReferenceEffect, ShaderReferenceEffect_Suspend>();
			logger::info("Hooked ShaderReferenceEffect::Suspend"sv);
		}
	};

	void Install_GetLightData()
	{
		REL::Relocation<std::uintptr_t> target_0{ RELOCATION_ID(19102, 19504), OFFSET(0xC0, 0xCA) };  // TESObjectREFR::ClearData
		stl::write_thunk_call<GetLightData<0>>(target_0.address());

		REL::Relocation<std::uintptr_t> target_1{ RELOCATION_ID(19302, 19729), OFFSET(0x63C, 0x63A) };  // TESObjectREFR::Set3D
		stl::write_thunk_call<GetLightData<1>>(target_1.address());

		logger::info("Hooked ExtraDataList::GetLightData");
	}

	void Install_BGSAttachTechniques__DetachItem()
	{
		REL::Relocation<std::uintptr_t> target_0{ RELOCATION_ID(33371, 34152), OFFSET(0x26, 0xA7) };
		stl::write_thunk_call<BGSAttachTechniques__DetachItem<0>>(target_0.address());

#ifndef SKYRIM_AE
		REL::Relocation<std::uintptr_t> target_1{ RELOCATION_ID(33375, 0), OFFSET(0x52, 0) };
		stl::write_thunk_call<BGSAttachTechniques__DetachItem<1>>(target_1.address());
#endif
	}

	void Install()
	{
		RemoveLight::Install();
		Install_GetLightData();
		RunBiped3DDetach::Install();
		Install_BGSAttachTechniques__DetachItem();
		Hazard__Release3DRelatedData::Install();
		Explosion__Release3DRelatedData::Install();
		BSTempEffect::Detach<RE::ShaderReferenceEffect>::Install();
		BSTempEffect::Detach<RE::ModelReferenceEffect>::Install();
		ShaderReferenceEffect_Suspend::Install();
	}
}
