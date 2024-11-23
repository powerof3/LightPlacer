#include "Hooks.h"

namespace Hooks
{
	namespace AttachDetach
	{
		struct AddAddonNodes
		{
			static void thunk(RE::NiAVObject* a_clonedNode, RE::NiAVObject* a_node, std::int32_t a_slot, RE::TESObjectREFR* a_actor, RE::BSTSmartPointer<RE::BipedAnim>& a_bipedAnim)
			{
				func(a_clonedNode, a_node, a_slot, a_actor, a_bipedAnim);

				LightManager::GetSingleton()->TryAttachLights(a_actor, a_bipedAnim, a_slot, a_node);
			};
			static inline REL::Relocation<decltype(thunk)> func;

			static void Install()
			{
				REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(15527, 15704) };
				stl::hook_function_prologue<AddAddonNodes, 5>(target.address());

				logger::info("Hooked BipedAnim::AddAddonNodes");
			}
		};

		struct AttachLight
		{
			static void thunk(RE::TESObjectREFR* a_this, bool a_isMagicLight)
			{
				LightManager::GetSingleton()->TryAttachLights(a_this, a_this->GetBaseObject());

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

		// clears light from the shadowscenenode
		struct RemoveLight
		{
			static void thunk(RE::TESObjectREFR* a_this, bool a_isMagicLight)
			{
				LightManager::GetSingleton()->DetachLights(a_this, false);

				func(a_this, a_isMagicLight);
			};
			static inline REL::Relocation<decltype(thunk)> func;

			static void Install()
			{
				REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(19253, 19679) };
				stl::hook_function_prologue<RemoveLight, 8>(target.address());

				logger::info("Hooked TESObjectREFR::RemoveLight");
			}
		};

		// clears light from the shadowscenenode + nilight ptr
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

			static void Install()
			{
				std::array targets{
					std::make_pair(RELOCATION_ID(19102, 19504), OFFSET(0xC0, 0xCA)),    // TESObjectREFR::ClearData
					std::make_pair(RELOCATION_ID(19302, 19729), OFFSET(0x63C, 0x63A)),  // TESObjectREFR::Set3D
				};

				for (auto& [address, offset] : targets) {
					REL::Relocation<std::uintptr_t> target{ address, offset };
					stl::write_thunk_call<GetLightData>(target.address());
				}

				logger::info("Hooked ExtraDataList::GetLightData");
			}
		};

		struct RunBiped3DDetach
		{
			static void thunk(const RE::ActorHandle& a_handle, RE::NiAVObject* a_node)
			{
				LightManager::GetSingleton()->DetachWornLights(a_handle, a_node);

				func(a_handle, a_node);
			};
			static inline REL::Relocation<decltype(thunk)> func;

			static void Install()
			{
				REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(15495, 15660) };
				stl::hook_function_prologue<RunBiped3DDetach, 5>(target.address());

				logger::info("Hooked BipedAnim::RunBiped3DDetach");
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
			AttachLight::Install();
			AddAddonNodes::Install();
			ReAddCasterLights::Install();

			RemoveLight::Install();
			GetLightData::Install();
			RunBiped3DDetach::Install();
		}
	}

	namespace ProcessedLights
	{
		struct CheckUsesExternalEmittancePatch
		{
			static void Install()
			{
				REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(19002, 19413), OFFSET(0x9E1, 0x936) };  //TESObjectCELL::AttachReference3D

				struct Patch : Xbyak::CodeGenerator
				{
					Patch(std::uintptr_t a_func)
					{
						Xbyak::Label f;
#ifdef SKYRIM_AE
						mov(rdx, r15);
#else
						mov(rdx, r14);
#endif
						jmp(ptr[rip + f]);

						L(f);
						dq(a_func);
					}
				};

				Patch patch{ reinterpret_cast<std::uintptr_t>(CheckUsesExternalEmittance) };
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				_CheckUsesExternalEmittance = trampoline.write_call<5>(target.address(), trampoline.allocate(patch));

				logger::info("Patched TESObjectREFR::CheckUsesExternalEmittance");
			}

		private:
			static bool CheckUsesExternalEmittance(RE::TESObjectREFR* a_ref, RE::TESObjectCELL* a_cell)
			{
				if (a_cell && a_cell->loadedData && a_ref && a_ref->Get3D()) {
					LightManager::GetSingleton()->AddLightsToProcessQueue(a_cell, a_ref);
				}
				return _CheckUsesExternalEmittance(a_ref);
			}
			static inline REL::Relocation<bool(RE::TESObjectREFR*)> _CheckUsesExternalEmittance;
		};

		// update flickering (TESObjectCELL::RunAnimations)
		struct UpdateActivateParents
		{
			static void thunk(RE::TESObjectCELL* a_cell)
			{
				func(a_cell);

				if (a_cell && a_cell->loadedData) {
					LightManager::GetSingleton()->UpdateFlickeringAndConditions(a_cell);
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

				if (a_cell && a_cell->loadedData) {
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

		struct RemoveExternalEmittance
		{
			static void thunk(RE::TESObjectCELL* a_cell, const RE::ObjectRefHandle& a_handle)
			{
				func(a_cell, a_handle);

				if (a_cell && a_cell->loadedData) {
					LightManager::GetSingleton()->RemoveLightsFromProcessQueue(a_cell, a_handle);
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;

			static void Install()
			{
				std::array targets{
					std::make_pair(RELOCATION_ID(18568, 19032), OFFSET(0x190, 0x171)),  // TESObjectREFR::RemoveReference3D
					std::make_pair(RELOCATION_ID(19301, 19728), OFFSET(0x1BA, 0x206))   // TESObjectREFR::Release3DRelatedData
				};

				for (auto& [address, offset] : targets) {
					REL::Relocation<std::uintptr_t> target{ address, offset };
					stl::write_thunk_call<RemoveExternalEmittance>(target.address());
				}

				logger::info("Hooked TESObjectCELL::RemoveExternalEmittance");
			}
		};

		void Install()
		{
			CheckUsesExternalEmittancePatch::Install();

			UpdateActivateParents::Install();
			UpdateManagedNodes::Install();

			RemoveExternalEmittance::Install();
		}
	}

	void Install()
	{
		logger::info("{:*^30}", "HOOKS");

		AttachDetach::Install();
		ProcessedLights::Install();
	}
}
