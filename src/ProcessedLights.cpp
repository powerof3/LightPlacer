#include "ProcessedLights.h"

#include "SourceData.h"

bool ProcessedLights::IsNewLight(RE::NiPointLight* a_niLight)
{
	return std::find(lights.begin(), lights.end(), a_niLight) == lights.end();
}

bool ProcessedLights::emplace_back(const SourceData& a_srcData, const LightSourceData& a_lightSrcData, RE::NiPointLight* a_niLight, RE::BSLight* a_bsLight, RE::NiAVObject* a_debugMarker)
{
	if (IsNewLight(a_niLight)) {
		lights.emplace_back(a_lightSrcData, a_bsLight, a_niLight, a_debugMarker, a_srcData.ref, a_srcData.scale);
		return true;
	}
	return false;
}

void ProcessedLights::emplace_back(const REFR_LIGH& a_lightREFRData)
{
	lights.emplace_back(a_lightREFRData);
}

void ProcessedLights::ShowDebugMarkers(bool a_show) const
{
	for (auto& light : lights) {
		light.ShowDebugMarker(a_show);
	}
}

void ProcessedLights::ReattachLights(RE::TESObjectREFR* a_ref)
{
	for (auto& light : lights) {
		light.ReattachLight(a_ref);
	}
}

void ProcessedLights::ReattachLights() const
{
	for (auto& light : lights) {
		light.ReattachLight();
	}
}

void ProcessedLights::RemoveLights(bool a_clearData) const
{
	for (auto& light : lights) {
		light.RemoveLight(a_clearData);
	}
}

bool ProcessedLights::UpdateTimer(float a_delta, float a_interval)
{
	lastUpdateTime += a_delta;
	if (lastUpdateTime >= a_interval) {
		lastUpdateTime = 0.0f;
		return true;
	}
	return false;
}

void ProcessedLights::UpdateConditions(RE::TESObjectREFR* a_ref, std::string_view a_nodeName, ConditionUpdateFlags a_flags)
{
	nodeVisHelper.Reset();

	for (auto& lightData : lights) {
		lightData.UpdateConditions(a_ref, nodeVisHelper, a_flags);
	}

	nodeVisHelper.UpdateNodeVisibility(a_ref, a_nodeName);
}

void ProcessedLights::UpdateLightsAndRef(const UpdateParams& a_params)
{
	const bool  updateConditions = UpdateTimer(a_params.delta, 1.0f);
	const bool  withinFlickerDistance = a_params.ref->IsPlayerRef() || a_params.ref->GetPosition().GetSquaredDistance(a_params.pcPos) < a_params.flickeringDistance;
	const float scale = withinFlickerDistance ? a_params.ref->GetScale() : 1.0f;

	const auto conditionUpdateFlags = firstLoad ? ConditionUpdateFlags::Forced : ConditionUpdateFlags::Normal;

	for (auto& lightData : lights) {
		if (a_params.dimFactor <= 1.0f) {
			lightData.DimLight(a_params.dimFactor);
			continue;
		}
		if (conditionUpdateFlags == ConditionUpdateFlags::Forced || updateConditions) {
			lightData.UpdateConditions(a_params.ref, nodeVisHelper, conditionUpdateFlags);
		}
		lightData.UpdateAnimation(withinFlickerDistance, scale);
		if (withinFlickerDistance) {
			lightData.UpdateFlickering();
		}
	}

	nodeVisHelper.UpdateNodeVisibility(a_params.ref, a_params.nodeName);

	firstLoad = false;
}

void ProcessedLights::UpdateEmittance() const
{
	for (auto& light : lights) {
		light.UpdateEmittance();
	}
}

void LightsToUpdate::emplace(const ProcessedLights& a_processedLights, RE::RefHandle a_handle, bool a_isObject)
{
	bool hasAnimatedLight = false;
	bool hasEmittanceLight = false;

	for (auto& lightData : a_processedLights.lights) {
		hasAnimatedLight |= lightData.IsAnimated();
		hasEmittanceLight |= (a_isObject && lightData.data.emittanceForm);

		if (hasAnimatedLight && hasEmittanceLight) {
			break;
		}
	}

	if (hasAnimatedLight) {
		stl::unique_insert(animatedLights, a_handle);
	}

	if (hasEmittanceLight) {
		stl::unique_insert(emittanceLights, a_handle);
	}
}

void LightsToUpdate::emplace(const REFR_LIGH& a_lightData, RE::RefHandle a_handle)
{
	if (a_lightData.IsAnimated()) {
		stl::unique_insert(animatedLights, a_handle);
	}
}

void LightsToUpdate::erase(RE::RefHandle a_handle)
{
	stl::unique_erase(animatedLights, a_handle);
	stl::unique_erase(emittanceLights, a_handle);
}
