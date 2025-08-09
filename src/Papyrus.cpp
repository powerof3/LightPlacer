#include "Papyrus.h"

#include "Manager.h"

namespace Papyrus
{
	void TogglePlacedLight(RE::StaticFunctionTag*, RE::TESObjectREFR* a_ref, bool a_hide)
	{
		if (!a_ref || !a_ref->Is3DLoaded()) {
			return;
		}

		auto refHandle = a_ref->CreateRefHandle();
		LightManager::GetSingleton()->ForEachLight(a_ref, refHandle.native_handle(), [&](const auto&, auto& processedLight) {
			processedLight.ToggleLightsScript(a_hide);
			return true;
		});
	}

	bool IsPlacedLightToggled(RE::StaticFunctionTag*, RE::TESObjectREFR* a_ref)
	{
		if (!a_ref || !a_ref->Is3DLoaded()) {
			return false;
		}

		bool result = false;

		auto refHandle = a_ref->CreateRefHandle();
		LightManager::GetSingleton()->ForEachLight(a_ref, refHandle.native_handle(), [&](const auto&, auto& processedLight) {
			result = processedLight.GetLightsToggledScript();
			return !result;
		});

		return result;
	}

	bool Register(RE::BSScript::IVirtualMachine* a_vm)
	{
		if (!a_vm) {
			return false;
		}

		a_vm->RegisterFunction("TogglePlacedLight", SCRIPT, TogglePlacedLight);
		a_vm->RegisterFunction("IsPlacedLightToggled", SCRIPT, IsPlacedLightToggled);

		logger::info("Registered {} class", SCRIPT);

		return true;
	}
}
