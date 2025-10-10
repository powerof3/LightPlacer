#include "Debug.h"

#include "Manager.h"
#include "Settings.h"

namespace Debug
{
	struct LogLights
	{
		constexpr static auto OG_COMMAND = "ToggleHeapTracking"sv;

		constexpr static auto LONG_NAME = "LogLP"sv;
		constexpr static auto SHORT_NAME = "LogLP"sv;
		constexpr static auto HELP = "List all Light Placer lights attached to the reference\n"sv;

		constexpr static RE::SCRIPT_PARAMETER SCRIPT_PARAMS = { "ObjectRef", RE::SCRIPT_PARAM_TYPE::kObjectRef, true };

		static bool Execute(const RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR* a_obj, RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&, std::uint32_t&)
		{
			if (!a_obj) {
				return false;
			}

			const auto root = a_obj->GetCurrent3D();

			if (!root) {
				return false;
			}

			std::vector<std::vector<RE::NiAVObject*>> lightSceneGraph;
			RE::BSVisit::TraverseScenegraphLights(root, [&](RE::NiPointLight* ptLight) {
				std::vector<RE::NiAVObject*> hierarchy;
				GetNodeHierarchy(ptLight, hierarchy);
				lightSceneGraph.push_back(hierarchy);

				return RE::BSVisit::BSVisitControl::kContinue;
			});

			RE::ConsoleLog::GetSingleton()->Print("%08X (%u lights)", a_obj->GetFormID(), lightSceneGraph.size());

			if (lightSceneGraph.empty()) {
				return false;
			}

			if (lightSceneGraph.size() == 1 && lightSceneGraph[0].size() <= 2) {
				RE::ConsoleLog::GetSingleton()->Print("\t%s", OutputSceneGraph(lightSceneGraph[0]).c_str());
				return false;
			}

			SceneGraphMap mergedSceneGraph;

			for (const auto& nodeHierarchy : lightSceneGraph) {
				for (std::size_t i = 0; i + 1 < nodeHierarchy.size(); ++i) {
					auto& children = mergedSceneGraph[nodeHierarchy[i]];
					if (std::ranges::find(children, nodeHierarchy[i + 1]) == children.end()) {
						children.push_back(nodeHierarchy[i + 1]);
					}
				}
			}

			std::vector<std::string> output;
			OutputSceneGraph(mergedSceneGraph, root, output, "", true);
			for (auto& line : output) {
				RE::ConsoleLog::GetSingleton()->Print("%s", line.c_str());
			}

			return false;
		}

	private:
		using SceneGraphMap = std::unordered_map<RE::NiAVObject*, std::vector<RE::NiAVObject*>>;

		static std::string GetDetails(RE::NiAVObject* a_currentNode)
		{
			std::string details{};

			if (auto light = netimmerse_cast<RE::NiPointLight*>(a_currentNode)) {
				details = std::format(" (radius: {}|fade: {}|{})", light->radius.x, light->fade, LightData::GetCulledStatus(light));
			}

			return details;
		}

		static void GetNodeHierarchy(RE::NiAVObject* a_obj, std::vector<RE::NiAVObject*>& hierarchy)
		{
			static auto BSMultiBoundNode_RTTI = REL::Relocation<RE::NiRTTI*>(RE::BSMultiBoundNode::RTTI.address());

			while (a_obj && a_obj->GetRTTI() != BSMultiBoundNode_RTTI.get()) {  // object root is attached to nameless BSMultiBoundNode
				hierarchy.emplace_back(a_obj);
				a_obj = a_obj->parent;
			}
			std::ranges::reverse(hierarchy);  // root -> lights
		}

		static std::string OutputSceneGraph(const std::vector<RE::NiAVObject*>& nodeHierarchy)
		{
			std::string output;
			for (std::size_t i = 0; i < nodeHierarchy.size(); ++i) {
				auto currentNode = nodeHierarchy[i];
				output += currentNode->name.c_str() + GetDetails(currentNode);
				if (i < nodeHierarchy.size() - 1) {
					output += " > ";
				}
			}
			return output;
		}

		static void OutputSceneGraph(const SceneGraphMap& a_mergedSceneGraph, RE::NiAVObject* a_currentNode, std::vector<std::string>& a_output, const std::string& a_indent, bool a_isLastChild)
		{
			if (!a_currentNode) {
				return;
			}

			a_output.emplace_back(std::format("{}> {}{}", a_indent, a_currentNode->name.c_str(), GetDetails(a_currentNode)));

			if (auto it = a_mergedSceneGraph.find(a_currentNode); it != a_mergedSceneGraph.end()) {
				const auto& children = it->second;
				for (std::size_t i = 0; i < children.size(); ++i) {
					std::string childIndent = a_indent + (a_isLastChild ? "    " : "|   ");
					OutputSceneGraph(a_mergedSceneGraph, children[i], a_output, childIndent, i == children.size() - 1);
				}
			}
		}
	};

	struct ToggleLightPlacer
	{
		constexpr static auto OG_COMMAND = "TogglePoolTracking"sv;

		constexpr static auto LONG_NAME = "ToggleLP"sv;
		constexpr static auto SHORT_NAME = "tlp"sv;
		constexpr static auto HELP = "Toggle Light Placer functionality.\n(<id>: 0 - Lights | 1 - Debug Markers"sv;

		constexpr static RE::SCRIPT_PARAMETER SCRIPT_PARAMS = { "Integer", RE::SCRIPT_PARAM_TYPE::kInt, false };

		static bool Execute(const RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData* a_scriptData, RE::TESObjectREFR*, RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&, std::uint32_t&)
		{
			const auto settings = Settings::GetSingleton();

			switch (a_scriptData->GetIntegerChunk()->GetInteger()) {
			case 0:
				{
					auto consoleRefHandle = RE::Console::GetSelectedRefHandle();
					auto consoleRef = consoleRefHandle.get();
					if (consoleRef) {
						bool toggled = false;
						LightManager::GetSingleton()->ForEachLight(consoleRef.get(), consoleRefHandle.native_handle(), [&](const auto&, const auto& processedLights) {
							toggled = !processedLights.GetLightsToggled(LIGHT_CULL_FLAGS::Script);
							processedLights.ToggleLights(toggled, LIGHT_CULL_FLAGS::Script);
							return true;
						});
						RE::ConsoleLog::GetSingleton()->Print("Light Placer Lights %s on %u", !toggled ? "ON" : "OFF", consoleRef->GetFormID());
					} else {
						bool toggled = false;
						LightManager::GetSingleton()->ForAllLights([&](const auto& processedLights) {
							toggled = !processedLights.GetLightsToggled(LIGHT_CULL_FLAGS::Script);
							processedLights.ToggleLights(toggled, LIGHT_CULL_FLAGS::Script);
						});
						RE::ConsoleLog::GetSingleton()->Print("Light Placer Lights %s", !toggled ? "ON" : "OFF");					
					}	
				}
				break;
			case 1:
				{
					if (!settings->LoadDebugMarkers()) {
						RE::ConsoleLog::GetSingleton()->Print("bShowMarkers not enabled in Data/SKSE/Plugins/po3_LightPlacer.ini");
						return false;
					}

					settings->ToggleDebugMarkers();

					bool showDebugMarkers = settings->CanShowDebugMarkers();
					LightManager::GetSingleton()->ForAllLights([&](const auto& processedLights) {
						processedLights.ShowDebugMarkers(showDebugMarkers);
					});

					RE::ConsoleLog::GetSingleton()->Print("Light Placer Markers %s", showDebugMarkers ? "ON" : "OFF");
				}
				break;
			default:
				break;
			}

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

			LightManager::GetSingleton()->ReloadConfigs();

			SKSE::GetTaskInterface()->AddTask([refs]() {
				for (auto& ref : refs) {
					if (ref) {
						ref->Disable();
						ref->Enable(false);
					}
				}
				RE::TES::GetSingleton()->PurgeBufferedCells();
			});

			RE::ConsoleLog::GetSingleton()->Print("%u refs reloaded", refs.size());

			return false;
		}
	};

	void Install()
	{
		logger::info("{:*^50}", "DEBUG");

		ConsoleCommandHandler<LogLights>::Install();
		ConsoleCommandHandler<ToggleLightPlacer>::Install();
		ConsoleCommandHandler<ReloadConfig>::Install();
	}
}
