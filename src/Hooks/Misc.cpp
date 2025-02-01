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
		static constexpr std::size_t                   idx{ 0x13 };

		static void Install()
		{
			stl::write_vfunc<RE::TESObjectREFR, InitItemImpl>();
			logger::info("Hooked TESObjectREFR::InitItemImpl");
		}
	};

	void Install()
	{
		if (Settings::GetSingleton()->ShouldDisableLights()) {
			InitItemImpl::Install();
		}
	}
}
