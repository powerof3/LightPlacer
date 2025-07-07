#pragma once

namespace Papyrus
{
	inline constexpr auto SCRIPT = "LightPlacer"sv;

	bool IsPlacedLightToggled(RE::StaticFunctionTag*, RE::TESObjectREFR* a_ref);
	void TogglePlacedLight(RE::StaticFunctionTag*, RE::TESObjectREFR* a_ref, bool a_hide);

	bool Register(RE::BSScript::IVirtualMachine* a_vm);
}
