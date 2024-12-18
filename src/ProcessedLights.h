#pragma once

#include "LightData.h"

struct ProcessedLights
{
	ProcessedLights() = default;

	bool IsNewLight(RE::NiPointLight* a_niLight);

	bool emplace_back(const SourceData& a_srcData, const LightSourceData& a_lightSrcData, RE::NiPointLight* a_niLight, RE::BSLight* a_bsLight, RE::NiAVObject* a_debugMarker);
	void emplace_back(const REFR_LIGH& a_lightREFRData);

	void ShowDebugMarkers(bool a_show) const;

	void ReattachLights(RE::TESObjectREFR* a_ref);
	void ReattachLights() const;
	void RemoveLights(bool a_clearData) const;

	bool UpdateTimer(float a_delta, float a_interval);
	void UpdateLightsAndRef(RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_pcPos, float a_flickeringDistance, float a_delta, std::string_view a_nodeName = ""sv, float a_dimFactor = std::numeric_limits<float>::max());
	void UpdateEmittance() const;

	// members
	float                    lastUpdateTime{ 0.0f };
	std::vector<REFR_LIGH>   lights;
	REFR_LIGH::NodeVisHelper nodeVisHelper;
};

struct LightsToUpdate
{
	LightsToUpdate() = default;

	void emplace(const ProcessedLights& a_processedLights, RE::RefHandle a_handle, bool a_isObject);
	void emplace(const REFR_LIGH& a_lightData, RE::RefHandle a_handle);

	void erase(RE::RefHandle a_handle);

	// members
	std::vector<RE::RefHandle> animatedLights;
	std::vector<RE::RefHandle> emittanceLights;
};
