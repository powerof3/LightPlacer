#pragma once

namespace Hooks::Misc
{
	struct detail
	{
		static bool should_disable_light(RE::TESObjectLIGH* light, RE::TESObjectREFR* ref);
	};
	
	template <std::size_t N>
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
	};
	
	void Install();
}
