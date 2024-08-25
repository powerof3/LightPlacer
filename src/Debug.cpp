#include "Debug.h"

namespace Debug
{
	namespace detail
	{
		constexpr auto LONG_NAME = "ToggleWindowLight"sv;
		constexpr auto SHORT_NAME = "window"sv;

		[[nodiscard]] const std::string& HelpString()
		{
			static auto help = []() {
				std::string buf;
				buf += "ToggleWindowLight\n";
				return buf;
			}();
			return help;
		}

		bool Execute(const RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR* a_obj, RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&, std::uint32_t&)
		{
			if (a_obj) {
				if (auto root = a_obj->Get3D()) {
					auto* shadowSceneNode = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
					RE::BSVisit::TraverseScenegraphLights(root, [&](RE::NiPointLight* ptLight) {
						ptLight->SetAppCulled(!ptLight->GetAppCulled());
						return RE::BSVisit::BSVisitControl::kContinue;
					});
				}
			}

			return false;
		}
	}

	void Install()
	{
		if (const auto function = RE::SCRIPT_FUNCTION::LocateConsoleCommand("ToggleHeapTracking"); function) {
			static RE::SCRIPT_PARAMETER params[] = {
				{ "ObjectRef", RE::SCRIPT_PARAM_TYPE::kObjectRef, true }
			};

			function->functionName = detail::LONG_NAME.data();
			function->shortName = detail::SHORT_NAME.data();
			function->helpString = detail::HelpString().data();
			function->referenceFunction = false;
			function->SetParameters(params);
			function->executeFunction = &detail::Execute;
			function->conditionFunction = nullptr;

			logger::debug("installed {}", detail::LONG_NAME);
		}
	}
}
