#include "Hooks.h"

namespace Hooks
{
	// reattach light on cell detach/attach/unload
	struct AttachLight
	{
		static void thunk(RE::TESObjectREFR* a_this, bool a_isMagicLight)
		{
			func(a_this, a_isMagicLight);

			LightManager::GetSingleton()->TryAttachLights(a_this, a_this->GetBaseObject());
		};
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(19252, 19678) };
			stl::hook_function_prologue<AttachLight, 6>(target.address());

			logger::info("Hooked TESObjectREFR::AttachLight");
		}
	};

	// detach light on cell detach/unload (probably not needed? game wipes lights on its own)
	struct RemoveLight
	{
		static void thunk(RE::TESObjectREFR* a_this, bool a_isMagicLight)
		{
			func(a_this, a_isMagicLight);

			LightManager::GetSingleton()->DetachLights(a_this);
		};
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(19253, 19679) };
			stl::hook_function_prologue<RemoveLight, 8>(target.address());

			logger::info("Hooked TESObjectREFR::RemoveLight");
		}
	};

	// update flickering (TESObjectCELL::RunAnimations)
	struct UpdateActivateParents
	{
		static void thunk(RE::TESObjectCELL* a_cell)
		{
			func(a_cell);

			if (a_cell->loadedData) {
				LightManager::GetSingleton()->UpdateFlickering(a_cell);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(18458, 18889), 0x52 };
			stl::write_thunk_call<UpdateActivateParents>(target.address());

			logger::info("Hooked TESObjectCELL::UpdateActivateParents");
		}
	};

	// update LP emittance after emittance source colors have been updated for vanilla lights
	// emittance not updated needs to be manually updated.
	struct UpdateManagedNodes
	{
		static void thunk(RE::TESObjectCELL* a_cell)
		{
			func(a_cell);

			if (a_cell->loadedData) {
				LightManager::GetSingleton()->UpdateEmittance(a_cell);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(18464, 18895) };
			stl::hook_function_prologue<UpdateManagedNodes, 5>(target.address());

			logger::info("Hooked TESObjectCELL::UpdateManagedNodes");
		}
	};

	// clear processed lights (flickering/emittance) on 3d unload
	struct Set3D
	{
		static void thunk(RE::TESObjectREFR* a_this, RE::NiAVObject* a_node, bool a_queue3DTasks)
		{
			if (!a_node) {
				LightManager::GetSingleton()->ClearProcessedLights(a_this);
			}

			func(a_this, a_node, a_queue3DTasks);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline constexpr std::size_t            size{ 0x6C };

		static void Install()
		{
			stl::write_vfunc<RE::TESObjectREFR, Set3D>();

			logger::info("Hooked TESObjectREFR::Set3D");
		}
	};

	void Install()
	{
		logger::info("{:*^30}", "HOOKS");

		Clone3D<RE::BGSMovableStatic, 2>::Install();
		Clone3D<RE::TESFurniture>::Install();
		Clone3D<RE::TESObjectDOOR>::Install();
		Clone3D<RE::TESObjectMISC>::Install();
		Clone3D<RE::TESObjectSTAT>::Install();
		Clone3D<RE::TESObjectCONT>::Install();

		AttachLight::Install();
		RemoveLight::Install();
		Set3D::Install();

		UpdateActivateParents::Install();
		UpdateManagedNodes::Install();
	}
}
