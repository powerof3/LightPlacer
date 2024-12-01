#include "Debug.h"
#include "Manager.h"

namespace Debug
{
	namespace LogLights
	{
		constexpr auto LONG_NAME = "LogLights"sv;
		constexpr auto SHORT_NAME = "loglp"sv;

		[[nodiscard]] const std::string& HelpString()
		{
			static auto help = []() {
				std::string buf;
				buf += "Log all Light Placer lights on an object\n";
				return buf;
			}();
			return help;
		}

		bool Execute(const RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR* a_obj, RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&, std::uint32_t&)
		{
			if (a_obj) {
				if (auto root = a_obj->Get3D()) {
					RE::ConsoleLog::GetSingleton()->Print("%08X", a_obj->GetFormID());
					RE::ConsoleLog::GetSingleton()->Print("\tMesh");
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

		void Install()
		{
			logger::info("{:*^50}", "DEBUG");

			if (const auto function = RE::SCRIPT_FUNCTION::LocateConsoleCommand("ToggleHeapTracking"); function) {
				static RE::SCRIPT_PARAMETER params[] = {
					{ "ObjectRef", RE::SCRIPT_PARAM_TYPE::kObjectRef, true }
				};

				function->functionName = LONG_NAME.data();
				function->shortName = SHORT_NAME.data();
				function->helpString = HelpString().data();
				function->referenceFunction = false;
				function->SetParameters(params);
				function->executeFunction = &Execute;
				function->conditionFunction = nullptr;

				logger::debug("Installed {} console command", LONG_NAME);
			}
		}
	}

	void Install()
	{
		LogLights::Install();
	}
}
