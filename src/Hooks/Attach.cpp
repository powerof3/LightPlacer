#include "Attach.h"

namespace Hooks::Attach
{
	// adds light on 3d load
	struct Load3D
	{
		static RE::NiAVObject* thunk(RE::TESObjectREFR* a_this, bool a_backgroundLoading)
		{
			auto node = func(a_this, a_backgroundLoading);
			if (node) {
				if (auto baseObject = a_this->GetObjectReference(); baseObject && RE::ShouldAttachLight(baseObject)) {
					LightManager::GetSingleton()->AddLights(a_this, baseObject, node);
				}
			}
			return node;
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr std::size_t                   idx{ 0x6A };

		static void Install()
		{
			stl::write_vfunc<RE::TESObjectREFR, Load3D>();
			logger::info("Hooked TESObjectREFR::Load3D"sv);
		}
	};
	
	// armor/weapons
	struct AddAddonNodes
	{
		static void thunk(RE::NiAVObject* a_clonedNode, RE::NiAVObject* a_node, std::int32_t a_slot, RE::TESObjectREFR* a_actor, RE::BSTSmartPointer<RE::BipedAnim>& a_bipedAnim)
		{
			LightManager::GetSingleton()->AddWornLights(a_actor, a_bipedAnim, a_slot, a_node);

			func(a_clonedNode, a_node, a_slot, a_actor, a_bipedAnim);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(15527, 15704) };
			stl::hook_function_prologue<AddAddonNodes, 5>(target.address());

			logger::info("Hooked BipedAnim::AddAddonNodes");
		}
	};

	// casting art
	struct AttachEnchantmentVisuals
	{
		static void thunk(RE::ActorMagicCaster* a_this)
		{
			LightManager::GetSingleton()->AddCastingLights(a_this);

			func(a_this);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(33373, 34154) };
			stl::hook_function_prologue<AttachEnchantmentVisuals, 6>(target.address());

			logger::info("Hooked ActorMagicCaster::AttachEnchantmentVisuals");
		}
	};

	// reattach ref lights
	struct AttachLight
	{
		static void thunk(RE::TESObjectREFR* a_this, bool a_isMagicLight)
		{
			LightManager::GetSingleton()->ReattachLights(a_this, a_this->GetBaseObject());

			func(a_this, a_isMagicLight);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(19252, 19678) };
			stl::hook_function_prologue<AttachLight, 6>(target.address());

			logger::info("Hooked TESObjectREFR::AttachLight");
		}
	};

	// reattach actor lights
	struct ReAddCasterLights
	{
		static void thunk(RE::Actor* a_this, RE::ShadowSceneNode& a_shadowSceneNode)
		{
			LightManager::GetSingleton()->ReattachWornLights(a_this->CreateRefHandle());

			func(a_this, a_shadowSceneNode);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(37826, 38780) };
			stl::hook_function_prologue<ReAddCasterLights, 5>(target.address());

			logger::info("Hooked Actor::ReAddCasterLights");
		}
	};

	struct ShaderReferenceEffect_Resume
	{
		static void thunk(RE::ShaderReferenceEffect* a_this)
		{
			bool suspended = a_this->flags.any(RE::ShaderReferenceEffect::Flag::kSuspended);
			func(a_this);
			if (suspended != a_this->flags.any(RE::ShaderReferenceEffect::Flag::kSuspended)) {
				LightManager::GetSingleton()->ReattachReferenceEffectLights(a_this);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr std::size_t                   idx{ 0x38 };

		static void Install()
		{
			stl::write_vfunc<RE::ShaderReferenceEffect, ShaderReferenceEffect_Resume>();
			logger::info("Hooked ShaderReferenceEffect::Resume"sv);
		}
	};

	void Install()
	{
		Load3D::Install();

		ReferenceEffect::Init<RE::ShaderReferenceEffect>::Install();
		ReferenceEffect::Init<RE::ModelReferenceEffect>::Install();
		AttachEnchantmentVisuals::Install();
		AddAddonNodes::Install();

		AttachLight::Install();
		ReAddCasterLights::Install();
		ShaderReferenceEffect_Resume::Install();
	}
}
