#include "Misc.h"

#include "Manager.h"
#include "Settings.h"

namespace Hooks::Misc
{
	struct InitItemImpl
	{
		static void thunk(RE::TESObjectREFR* a_ref)
		{
			func(a_ref);

			const auto base = a_ref->GetBaseObject();
			if (a_ref->IsDynamicForm() || !base || base->IsNot(RE::FormType::Light)) {
				return;
			}

			auto light = base->As<RE::TESObjectLIGH>();
			if (!light || light->data.flags.any(RE::TES_LIGHT_FLAGS::kCanCarry)) {
				return;
			}

			if (!Settings::GetSingleton()->GetGameLightDisabled(a_ref, base)) {
				return;
			}

			a_ref->formFlags |= RE::TESObjectREFR::RecordFlags::kInitiallyDisabled;
			a_ref->data.location.z -= 30000.0f;
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr std::size_t                   size{ 0x13 };

		static void Install()
		{
			stl::write_vfunc<RE::TESObjectREFR, InitItemImpl>();
			logger::info("Hooked TESObjectREFR::InitItemImpl");
		}
	};

	struct FreeCamera_Begin
	{
		static void thunk(RE::FreeCameraState* a_this)
		{
			func(a_this);

			LightManager::GetSingleton()->SetFreeCameraMode(true);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr std::size_t                   size{ 0x1 };

		static void Install()
		{
			stl::write_vfunc<RE::FreeCameraState, FreeCamera_Begin>();
			logger::info("Hooked FreeCameraState::Begin");
		}
	};

	struct FreeCamera_End
	{
		static void thunk(RE::FreeCameraState* a_this)
		{
			func(a_this);

			LightManager::GetSingleton()->SetFreeCameraMode(false);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static constexpr std::size_t                   size{ 0x2 };

		static void Install()
		{
			stl::write_vfunc<RE::FreeCameraState, FreeCamera_End>();
			logger::info("Hooked FreeCameraState::Begin");
		}
	};

#ifdef SKYRIMVR
	struct Patch_NiFrustrumPlanes__Set
	{
		static void Install()
		{
			const REL::Relocation NiFrustrumPlanes__Set{ REL::Offset(0xCB2760) };

			// mov rdx, [rdx+180h] (48 8B 92 80 01 00 00)
			//                         v  v  v
			// add rdx, 1CCh       (48 81 C2 CC 01 00 00)

			REL::safe_fill(NiFrustrumPlanes__Set.address() + 0x5, 0x81, 1);
			REL::safe_fill(NiFrustrumPlanes__Set.address() + 0x6, 0xC2, 1);
			REL::safe_fill(NiFrustrumPlanes__Set.address() + 0x7, 0xCC, 1);
			logger::info("Patched NiFrustrumPlanes::Set");
		}
	};
#endif

	void Install()
	{
		if (Settings::GetSingleton()->ShouldDisableLights()) {
			InitItemImpl::Install();
		}

		FreeCamera_Begin::Install();
		FreeCamera_End::Install();
#ifdef SKYRIMVR
		Patch_NiFrustrumPlanes__Set::Install();
#endif
	}
}
