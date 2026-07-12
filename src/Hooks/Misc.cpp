#include "Misc.h"

#include "Settings.h"

namespace Hooks::Misc
{
	bool detail::should_disable_light(RE::TESObjectLIGH* light, RE::TESObjectREFR* ref)
	{
		return ref && light && !ref->IsDynamicForm() && light->data.flags.none(RE::TES_LIGHT_FLAGS::kCanCarry) && Settings::GetSingleton()->GetGameLightDisabled(ref, light);
	}

	void Install()
	{
		if (Settings::GetSingleton()->ShouldDisableLights()) {
			REL::Relocation<std::uintptr_t> target_0{ RELOCATION_ID(17206, 17603), 0x1D3 };  // TESObjectLIGH::Clone3D
			stl::write_thunk_call<TESObjectLIGH_GenDynamic<0>>(target_0.address());

			REL::Relocation<std::uintptr_t> target_1{ RELOCATION_ID(19252, 19678), 0xB8 };  // TESObjectREFR::AddLight
			stl::write_thunk_call<TESObjectLIGH_GenDynamic<1>>(target_1.address());

			logger::info("Installed TESObjectLIGH::GenDynamic patch"sv);
		}
	}
}
