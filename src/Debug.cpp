#include "Debug.h"
#include "Manager.h"

namespace Debug
{
	struct LogLights
	{
		constexpr static auto OG_COMMAND = "ToggleHeapTracking"sv;

		constexpr static auto LONG_NAME = "LogLights"sv;
		constexpr static auto SHORT_NAME = "loglp"sv;
		constexpr static auto HELP = "Log all Light Placer lights on an object\n"sv;

		constexpr static RE::SCRIPT_PARAMETER SCRIPT_PARAMS = { "ObjectRef", RE::SCRIPT_PARAM_TYPE::kObjectRef, true };

		static bool Execute(const RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR* a_obj, RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&, std::uint32_t&)
		{
			if (a_obj) {
				if (auto root = a_obj->Get3D()) {
					RE::ConsoleLog::GetSingleton()->Print("%08X\n\tMesh", a_obj->GetFormID());

					RE::BSVisit::TraverseScenegraphLights(root, [&](RE::NiPointLight* ptLight) {
						RE::ConsoleLog::GetSingleton()->Print("\t\t%s", ptLight->name.c_str());
						return RE::BSVisit::BSVisitControl::kContinue;
					});

					RE::ConsoleLog::GetSingleton()->Print("\tLightData");

					LightManager::GetSingleton()->ForEachLight(a_obj->CreateRefHandle().native_handle(), [](const auto& lightData) {
						RE::ConsoleLog::GetSingleton()->Print("\t\t%s", lightData.bsLight->light->name.c_str());
					});
				}
			}

			return false;
		}
	};

	struct ToggleLightMarkers
	{
		constexpr static auto OG_COMMAND = "TogglePoolTracking"sv;

		constexpr static auto LONG_NAME = "ToggleLightMarkers"sv;
		constexpr static auto SHORT_NAME = "tlpm"sv;
		constexpr static auto HELP = "Toggle Light Markers\n"sv;

		static bool Execute(const RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*, RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&, std::uint32_t&)
		{
			showDebugMarker = !showDebugMarker;
			LightManager::GetSingleton()->ForAllLights([](const auto& lightData) {
				lightData.ShowDebugMarker(showDebugMarker);
			});

			RE::ConsoleLog::GetSingleton()->Print("Light Placer Markers %s", showDebugMarker ? "ON" : "OFF");

			return false;
		}
	};

	void Install()
	{
		ConsoleCommandHandler<LogLights>::Install();
		ConsoleCommandHandler<ToggleLightMarkers>::Install();
	}
}
