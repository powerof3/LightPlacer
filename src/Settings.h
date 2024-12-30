#pragma once

struct LightData;

class Settings : public REX::Singleton<Settings>
{
public:
	void LoadSettings();

	void OnDataLoad();

	bool CanCullLights() const;

	bool CanShowDebugMarkers() const;
	bool LoadDebugMarkers() const;
	void ToggleDebugMarkers();

	bool ShouldDisableLights() const;
	bool GetGameLightDisabled(const RE::TESObjectREFR* a_ref, const RE::TESBoundObject* a_base) const;

private:
	void ReadSettings(std::string_view a_path);

	// members
	bool showDebugMarkers{ false };
	bool loadDebugMarkers{ false };
	bool cullLights{ true };

	bool disableAllGameLights{ false };

	StringSet           blackListedLights;
	FlatSet<RE::FormID> blackListedLightsRefs;

	StringSet           whiteListedLights;
	FlatSet<RE::FormID> whiteListedLightsRefs;
};
