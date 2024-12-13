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
	logger::info("bShowMarkers is {}", showDebugMarkers);
	logger::info("bDisableGameLights is {}", disableGameLights);
	logger::info("AllowedGameLights : {} entries", whiteListedLights.size());

	loadDebugMarkers = showDebugMarkers;
}

void Settings::OnDataLoad()
{
	erase_if(whiteListedLights, [this](const auto& str) {
		if (!str.starts_with("0x")) { // assume formid
			return false;
		}
		if (auto formID = RE::GetFormID(str); formID != 0) {
			this->whiteListedLightsRefs.emplace(formID);
		}
		return true;
	});
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

bool Settings::GetGameLightsDisabled() const
{
	return disableGameLights;
}

bool Settings::IsGameLightAllowed(const RE::TESObjectREFR* a_ref) const
{
	return whiteListedLights.contains(a_ref->GetFile(0)->fileName) || whiteListedLightsRefs.contains(a_ref->GetFormID());
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
	if (!disableGameLights) {
		disableGameLights = ini.GetBoolValue("Settings", "bDisableGameLights", false);
	}

	CSimpleIniA::TNamesDepend keys;
	ini.GetAllKeys("AllowedGameLights", keys);

	for (const auto& key : keys) {
		whiteListedLights.emplace(string::trim_copy(key.pItem));
	}
}
