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

	bool GetGameLightsDisabled() const;
	bool IsGameLightAllowed(const RE::TESObjectREFR* a_ref) const;

private:
	void ReadSettings(std::string_view a_path);

	// members
	bool showDebugMarkers{ false };
	bool loadDebugMarkers{ false };

	bool                disableGameLights{ false };
	StringSet           whiteListedLights;
	FlatSet<RE::FormID> whiteListedLightsRefs;
};
