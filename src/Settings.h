#pragma once

struct LightData;

namespace SETTINGS
{
	class Cache
	{
	public:
		static Cache* GetSingleton()
		{
			return &instance;
		};

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

		static Cache instance;
	};

	inline StringSet           blackListedLights;
	inline FlatSet<RE::FormID> blackListedLightsRefs;
	inline StringSet           whiteListedLights;
	inline FlatSet<RE::FormID> whiteListedLightsRefs;
}

using Settings = SETTINGS::Cache;
inline constinit Settings Settings::instance;
