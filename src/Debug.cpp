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

			const auto root = a_obj->Get3D(false);

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

		static void GetNodeHierarchy(RE::NiAVObject* a_obj, std::vector<RE::NiAVObject*>& hierarchy)
		{
			while (a_obj && !a_obj->name.empty()) {  // object root is attached to nameless BSMultiBoundNode
				hierarchy.emplace_back(a_obj);
				a_obj = a_obj->parent;
			}
			std::ranges::reverse(hierarchy);  // root -> lights
		}

		static std::string OutputSceneGraph(const std::vector<RE::NiAVObject*>& nodeHierarchy)
		{
			std::string output;
			for (std::size_t i = 0; i < nodeHierarchy.size(); ++i) {
				output += nodeHierarchy[i]->name.c_str();
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

			a_output.emplace_back(a_indent + "> " + a_currentNode->name.c_str());

			if (auto it = a_mergedSceneGraph.find(a_currentNode); it != a_mergedSceneGraph.end()) {
				const auto& children = it->second;
				for (std::size_t i = 0; i < children.size(); ++i) {
					std::string childIndent = a_indent + (a_isLastChild ? "\t\t\t" : "|\t\t\t");
					OutputSceneGraph(a_mergedSceneGraph, children[i], a_output, childIndent, i == children.size() - 1);
				}
			}
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
			const auto settings = Settings::GetSingleton();

			if (!settings->LoadDebugMarkers()) {
				RE::ConsoleLog::GetSingleton()->Print("bShowMarkers not enabled in Data/SKSE/Plugins/po3_LightPlacer.ini");
				return false;
			}

			settings->ToggleDebugMarkers();

			bool showDebugMarkers = settings->CanShowDebugMarkers();
			LightManager::GetSingleton()->ForAllLights([&](const auto& lightData) {
				lightData.ShowDebugMarker(showDebugMarkers);
			});

			RE::ConsoleLog::GetSingleton()->Print("Light Placer Markers %s", showDebugMarkers ? "ON" : "OFF");

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
		ConsoleCommandHandler<ToggleLightMarkers>::Install();
		ConsoleCommandHandler<ReloadConfig>::Install();
	}
}
