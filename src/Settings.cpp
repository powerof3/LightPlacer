#include "Settings.h"

void Settings::LoadSettings()
{
	logger::info("{:*^50}", "SETTINGS");

	ReadSettings(R"(Data\SKSE\Plugins\po3_LightPlacer.ini)");

	std::filesystem::path dir{ R"(Data\LightPlacer)" };

	std::error_code ec;
	if (std::filesystem::exists(dir, ec)) {
		for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(dir)) {
			if (dirEntry.is_directory() || dirEntry.path().extension() != ".ini"sv) {
				continue;
			}
			ReadSettings(dirEntry.path().string());
		}
	}

	logger::info("");
	logger::info("bShowMarkers : {}", showDebugMarkers);
	logger::info("bCullLights : {}", cullLights);
	logger::info("bDisableAllGameLights : {}", disableAllGameLights);
	logger::info("LightBlackList : {} entries", blackListedLights.size());
	logger::info("LightWhiteList : {} entries", whiteListedLights.size());

	loadDebugMarkers = showDebugMarkers;
}

void Settings::OnDataLoad()
{
	constexpr auto post_process = [](StringSet& a_strSet, FlatSet<RE::FormID>& a_formSet) {
		erase_if(a_strSet, [&](const auto& str) {
			if (!str.starts_with("0x")) {  // assume formid
				return false;
			}
			if (auto formID = RE::GetFormID(str); formID != 0) {
				a_formSet.emplace(formID);
			}
			return true;
		});
	};

	post_process(blackListedLights, blackListedLightsRefs);
	post_process(whiteListedLights, whiteListedLightsRefs);
}

bool Settings::CanCullLights() const
{
	return cullLights;
}

bool Settings::CanShowDebugMarkers() const
{
	return showDebugMarkers;
}

bool Settings::LoadDebugMarkers() const
{
	return loadDebugMarkers;
}

void Settings::ToggleDebugMarkers()
{
	showDebugMarkers = !showDebugMarkers;
}

bool Settings::ShouldDisableLights() const
{
	return disableAllGameLights || !blackListedLights.empty() || !blackListedLightsRefs.empty();
}

bool Settings::GetGameLightDisabled(const RE::TESObjectREFR* a_ref) const
{
	if (disableAllGameLights) {
		return !whiteListedLights.contains(a_ref->GetFile(0)->fileName) && !whiteListedLightsRefs.contains(a_ref->GetFormID());
	}

	return blackListedLights.contains(a_ref->GetFile(0)->fileName) || blackListedLightsRefs.contains(a_ref->GetFormID());
}

void Settings::ReadSettings(std::string_view a_path)
{
	logger::info("Reading {}...", a_path);

	CSimpleIniA ini;
	ini.SetUnicode();
	ini.SetAllowKeyOnly();

	ini.LoadFile(a_path.data());

	if (!showDebugMarkers) {
		showDebugMarkers = ini.GetBoolValue("Settings", "bShowMarkers", false);
	}
	if (!disableAllGameLights) {
		disableAllGameLights = ini.GetBoolValue("Settings", "bDisableAllGameLights", false);
	}
	if (cullLights) {
		cullLights = ini.GetBoolValue("Settings", "bCullLights", true);
	}

	const auto add_to_list = [&](std::string_view a_listName, StringSet& a_list) {
		CSimpleIniA::TNamesDepend keys;
		ini.GetAllKeys(a_listName.data(), keys);
		for (const auto& key : keys) {
			a_list.emplace(string::trim_copy(key.pItem));
		}
	};

	add_to_list("LightBlackList"sv, blackListedLights);
	add_to_list("LightWhiteList"sv, whiteListedLights);
}
