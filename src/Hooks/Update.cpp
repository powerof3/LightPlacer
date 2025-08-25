#include "Update.h"

#include "LightData.h"

namespace Hooks::Update
{
	// add lights to queue
	struct CheckUsesExternalEmittancePatch
	{
		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(19002, 19413), OFFSET(0x9E1, (RE::GetGameVersion() >= SKSE::RUNTIME_SSE_LATEST ? 0x936 : 0x948)) };  //TESObjectCELL::AttachReference3D

			struct Patch : Xbyak::CodeGenerator
			{
				Patch(std::uintptr_t a_func)
				{
					Xbyak::Label f;
#if defined(SKYRIM_AE) || defined(SKYRIMVR)
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
				LightManager::GetSingleton()->AddLightsToUpdateQueue(a_cell, a_ref);
			}
			return _CheckUsesExternalEmittance(a_ref);
		}
		static inline REL::Relocation<bool(RE::TESObjectREFR*)> _CheckUsesExternalEmittance;
	};

	// update flickering
	struct UpdateActivateParents
	{
		static void thunk(RE::TESObjectCELL* a_cell)
		{
			func(a_cell);

			if (a_cell && a_cell->loadedData) {
				LightManager::GetSingleton()->UpdateLights(a_cell);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(18458, 18889), 0x52 };  // TESObjectCELL::RunAnimations
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

	struct Hazard__CheckInit3D
	{
		static bool thunk(RE::Hazard* a_hazard)
		{
			auto result = func(a_hazard);

			if (result && a_hazard && a_hazard->flags.none(RE::Hazard::Flags::kExpired)) {
				LightManager::GetSingleton()->UpdateHazardLights(a_hazard);
			}

			return result;
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(42791, 43959), OFFSET(0x11, 0x1F) };  // Hazard::Update
			stl::write_thunk_call<Hazard__CheckInit3D>(target.address());

			logger::info("Hooked Hazard::CheckInit3D");
		}
	};

	struct Explosion__CheckInit3D
	{
		static bool thunk(RE::Explosion* a_explosion)
		{
			auto result = func(a_explosion);

			if (result && a_explosion) {
				LightManager::GetSingleton()->UpdateExplosionLights(a_explosion);
			}

			return result;
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(42664, 43836), 0x11 };  // Explosion::Update
			stl::write_thunk_call<Explosion__CheckInit3D>(target.address());

			logger::info("Hooked Explosion::CheckInit3D");
		}
	};

	struct ActorMagicCaster__Update
	{
		static void thunk(RE::ActorMagicCaster* a_this, float a_delta)
		{
			LightManager::GetSingleton()->UpdateCastingLights(a_this, a_delta);

			func(a_this, a_delta);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr std::size_t                   idx{
#ifndef SKYRIMVR
			0x1D
#else
			0x1F
#endif
		};

		static void Install()
		{
			stl::write_vfunc<RE::ActorMagicCaster, ActorMagicCaster__Update>();
			logger::info("Hooked ActorMagicCaster::Update"sv);
		}
	};

	// remove lights
	struct RemoveExternalEmittance
	{
		static void thunk(RE::TESObjectCELL* a_cell, const RE::ObjectRefHandle& a_handle)
		{
			func(a_cell, a_handle);

			if (a_cell && a_cell->loadedData) {
				LightManager::GetSingleton()->RemoveLightsFromUpdateQueue(a_cell, a_handle);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			std::array targets{
				std::make_pair(RELOCATION_ID(18568, 19032), OFFSET(0x190, 0x171)),  // TESObjectREFR::RemoveReference3D
				std::make_pair(RELOCATION_ID(19301, 19728), OFFSET(0x1BA, 0x206))   // TESObjectREFR::Release3DRelatedData
			};

			for (const auto& [address, offset] : targets) {
				REL::Relocation<std::uintptr_t> target{ address, offset };
				stl::write_thunk_call<RemoveExternalEmittance>(target.address());
			}

			logger::info("Hooked TESObjectCELL::RemoveExternalEmittance");
		}
	};

	struct NiSwitchNode_UpdateDownwardsPass
	{
		static void thunk(RE::NiSwitchNode* a_this, RE::NiUpdateData& a_data, std::uint32_t a_arg2)
		{
			if (a_this->children.size() == 2) {
				auto switch_idx = static_cast<std::uint16_t>(a_this->index);
				// inactive node
				RE::BSVisit::TraverseScenegraphLights(a_this->children[!switch_idx].get(), [](RE::NiPointLight* a_light) {
					LightData::CullLight(a_light, nullptr, true, LIGHT_CULL_FLAGS::Game);
					return RE::BSVisit::BSVisitControl::kContinue;
				});

				// active node
				RE::BSVisit::TraverseScenegraphLights(a_this->children[switch_idx].get(), [](RE::NiPointLight* a_light) {
					LightData::CullLight(a_light, nullptr, false, LIGHT_CULL_FLAGS::Game);
					return RE::BSVisit::BSVisitControl::kContinue;
				});
			}

			func(a_this, a_data, a_arg2);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr std::size_t                   idx{ 0x2C };

		static void Install()
		{
			stl::write_vfunc<RE::NiSwitchNode, NiSwitchNode_UpdateDownwardsPass>();
			logger::info("Hooked NiSwitchNode::UpdateDownwardsPass");
		}
	};

	void Install()
	{
		CheckUsesExternalEmittancePatch::Install();

		UpdateActivateParents::Install();
		UpdateManagedNodes::Install();
		BSTempEffect::UpdatePosition<RE::ShaderReferenceEffect>::Install();
		BSTempEffect::UpdatePosition<RE::ModelReferenceEffect>::Install();
		Hazard__CheckInit3D::Install();
		Explosion__CheckInit3D::Install();
		ActorMagicCaster__Update::Install();
		NiSwitchNode_UpdateDownwardsPass::Install();

		RemoveExternalEmittance::Install();
	}
}
