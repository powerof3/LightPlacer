#include "Misc.h"

#include "Manager.h"
#include "Settings.h"

namespace Hooks::Misc
{
	struct detail
	{
		static bool should_disable_light(RE::TESObjectLIGH* light, RE::TESObjectREFR* ref)
		{
			return ref && light && !ref->IsDynamicForm() && light->data.flags.none(RE::TES_LIGHT_FLAGS::kCanCarry) && Settings::GetSingleton()->GetGameLightDisabled(ref, light);
		}
	};

	struct TESObjectLIGH_GenDynamic
	{
		static RE::NiPointLight* thunk(RE::TESObjectLIGH* light,
			RE::TESObjectREFR*                            ref,
			RE::NiNode*                                   node,
			bool                                          forceDynamic,
			bool                                          useLightRadius,
			bool                                          affectRequesterOnly)
		{
			return detail::should_disable_light(light, ref) ?
			           nullptr :
			           func(light, ref, node, forceDynamic, useLightRadius, affectRequesterOnly);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			std::array targets{
				std::make_pair(RELOCATION_ID(17206, 17603), 0x1D3),  // TESObjectLIGH::Clone3D
				std::make_pair(RELOCATION_ID(19252, 19678), 0xB8),   // TESObjectREFR::AddLight
			};

			for (const auto& [address, offset] : targets) {
				REL::Relocation<std::uintptr_t> target{ address, offset };
				stl::write_thunk_call<TESObjectLIGH_GenDynamic>(target.address());
			}

			logger::info("Installed TESObjectLIGH::GenDynamic patch"sv);
		}
	};

	void Install()
	{
		if (Settings::GetSingleton()->ShouldDisableLights()) {
			TESObjectLIGH_GenDynamic::Install();
		}
	}
}
