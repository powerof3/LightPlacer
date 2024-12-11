#include "Debug.h"
#include "Manager.h"

namespace Debug
{
	struct LogLights
	{
		constexpr static auto OG_COMMAND = "ToggleHeapTracking"sv;

		constexpr static auto LONG_NAME = "LogLP"sv;
		constexpr static auto SHORT_NAME = "LogLP"sv;
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

		constexpr static auto LONG_NAME = "ToggleLPMarkers"sv;
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

	struct ReloadConfig
	{
		constexpr static auto OG_COMMAND = "ToggleBoundVisGeom"sv;

		constexpr static auto LONG_NAME = "ReloadLP"sv;
		constexpr static auto SHORT_NAME = "ReloadLP"sv;
		constexpr static auto HELP = "Reload Light Placer configs from disk\n"sv;

		static bool Execute(const RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*, RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&, std::uint32_t&)
		{
			const auto refs = LightManager::GetSingleton()->GetLightAttachedRefs();

			for (auto& ref : refs) {
				ref->Disable();
			}

			LightManager::GetSingleton()->ReloadConfigs();

			for (auto& ref : refs) {
				ref->Enable(false);
			}

			RE::ConsoleLog::GetSingleton()->Print("%u refs reloaded", refs.size());

			return false;
		}
	};

	void Install()
	{
		logger::info("{:*^50}", "DEBUG");

		ConsoleCommandHandler<LogLights>::Install();
		ConsoleCommandHandler<ToggleLightMarkers>::Install();
		ConsoleCommandHandler<ReloadConfig>::Install();
	}
}
