#include "Settings.h"

#include <SimpleIni.h>
#undef ERROR

namespace SETTINGS
{
	void Cache::LoadSettings()
	{
		REX::INFO("{:*^50}", "SETTINGS");

		ReadSettings(R"(Data\SKSE\Plugins\po3_LightPlacer.ini)");

		std::filesystem::path dir{ R"(Data\LightPlacer)" };

		if (std::error_code ec; std::filesystem::exists(dir, ec)) {
			for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(dir)) {
				if (dirEntry.is_directory() || dirEntry.path().extension() != ".ini"sv) {
					continue;
				}
				ReadSettings(dirEntry.path().string());
			}
		}

		REX::INFO("");
		REX::INFO("bShowMarkers : {}", showDebugMarkers);
		REX::INFO("bDisableAllGameLights : {}", disableAllGameLights);
		REX::INFO("fGlobalLightRadiusMult : {}", globalLightRadius);
		REX::INFO("fGlobalLightFadeMult : {}", globalLightFade);
		REX::INFO("LightBlackList : {} entries", blackListedLights.size());
		REX::INFO("LightWhiteList : {} entries", whiteListedLights.size());

		loadDebugMarkers = showDebugMarkers;
	}

	void Cache::OnDataLoad()
	{
		constexpr auto is_formID = [](const auto& str) {
			return str.starts_with("0x") || str.starts_with("0X");
		};

		constexpr auto post_process = [](StringSet& a_strSet, FlatSet<RE::FormID>& a_formSet) {
			erase_if(a_strSet, [&](const auto& str) {
				if (!is_formID(str)) {  // assume formid
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

	bool Cache::CanShowDebugMarkers() const
	{
		return showDebugMarkers;
	}

	bool Cache::LoadDebugMarkers() const
	{
		return loadDebugMarkers;
	}

	void Cache::ToggleDebugMarkers()
	{
		showDebugMarkers = !showDebugMarkers;
	}

	float Cache::GetGlobalLightFade() const
	{
		return globalLightFade;
	}

	float Cache::GetGlobalLightRadius() const
	{
		return globalLightRadius;
	}

	bool Cache::ShouldDisableLights() const
	{
		return disableAllGameLights || !blackListedLights.empty() || !blackListedLightsRefs.empty();
	}

	bool Cache::GetGameLightDisabled(const RE::TESObjectREFR* a_ref, const RE::TESBoundObject* a_base) const
	{
		auto fileName = a_ref->GetFile(0)->fileName;
		auto lastFileName = a_ref->GetDescriptionOwnerFile()->fileName;

		if (disableAllGameLights) {
			return !whiteListedLights.contains(fileName) && !whiteListedLights.contains(lastFileName) && !whiteListedLightsRefs.contains(a_ref->GetFormID()) && !whiteListedLightsRefs.contains(a_base->GetFormID());
		}

		return blackListedLights.contains(fileName) || blackListedLights.contains(lastFileName) || blackListedLightsRefs.contains(a_ref->GetFormID()) || blackListedLightsRefs.contains(a_base->GetFormID());
	}

	void Cache::ReadSettings(std::string_view a_path)
	{
		REX::INFO("Reading {}...", a_path);

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

		globalLightFade = static_cast<float>(ini.GetDoubleValue("Settings", "fGlobalLightFadeMult", globalLightFade));
		globalLightRadius = static_cast<float>(ini.GetDoubleValue("Settings", "fGlobalLightRadiusMult", globalLightRadius));

		const auto add_to_list = [&](std::string_view a_listName, StringSet& a_list) {
			CSimpleIniA::TNamesDepend keys;
			ini.GetAllKeys(a_listName.data(), keys);
			for (const auto& key : keys) {
				a_list.emplace(REX::STR::TRIM_COPY(key.pItem));
			}
		};

		add_to_list("LightBlackList"sv, blackListedLights);
		add_to_list("LightWhiteList"sv, whiteListedLights);
	}
}
