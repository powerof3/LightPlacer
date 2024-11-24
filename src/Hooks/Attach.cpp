#include "Attach.h"
#include "Hooks.h"

namespace Hooks::Attach
{
	// armor/weapons
	struct AddAddonNodes
	{
		static void thunk(RE::NiAVObject* a_clonedNode, RE::NiAVObject* a_node, std::int32_t a_slot, RE::TESObjectREFR* a_actor, RE::BSTSmartPointer<RE::BipedAnim>& a_bipedAnim)
		{
			LightManager::GetSingleton()->AddWornLights(a_actor, a_bipedAnim, a_slot, a_node);

			func(a_clonedNode, a_node, a_slot, a_actor, a_bipedAnim);
		};
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(15527, 15704) };
			stl::hook_function_prologue<AddAddonNodes, 5>(target.address());

			logger::info("Hooked BipedAnim::AddAddonNodes");
		}
	};

	// reattach ref lights
	struct AttachLight
	{
		static void thunk(RE::TESObjectREFR* a_this, bool a_isMagicLight)
		{
			LightManager::GetSingleton()->ReattachLights(a_this, a_this->GetBaseObject());

			func(a_this, a_isMagicLight);
		};
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

	void Install()
	{
		Clone3D<RE::BGSMovableStatic, 2>::Install();
		Clone3D<RE::TESFurniture>::Install();
		Clone3D<RE::TESObjectDOOR>::Install();
		Clone3D<RE::TESObjectMISC>::Install();
		Clone3D<RE::TESObjectSTAT>::Install();
		Clone3D<RE::TESObjectCONT>::Install();
		Clone3D<RE::TESSoulGem>::Install();
		Clone3D<RE::TESObjectACTI>::Install();
		Clone3D<RE::TESObjectBOOK>::Install();
		Clone3D<RE::TESObjectWEAP>::Install();
		Clone3D<RE::TESObjectARMO>::Install();
		AddAddonNodes::Install();

		AttachLight::Install();
		ReAddCasterLights::Install();
	}
}
