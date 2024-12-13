#pragma once

struct LightData;

class Settings : public ISingleton<Settings>
{
public:
	void LoadSettings();

	void OnDataLoad();

	bool CanShowDebugMarkers() const;
	bool LoadDebugMarkers() const;
	void ToggleDebugMarkers();

	bool ShouldDisableLights() const;
	bool GetGameLightDisabled(const RE::TESObjectREFR* a_ref) const;

private:
	void ReadSettings(std::string_view a_path);

	// members
	bool showDebugMarkers{ false };
	bool loadDebugMarkers{ false };

	bool                disableAllGameLights{ false };
	StringSet           blackListedLights;
	FlatSet<RE::FormID> blackListedLightsRefs;
};
