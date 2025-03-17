#include "ProcessedLights.h"

#include "SourceData.h"

bool ProcessedLights::IsNewLight(RE::NiPointLight* a_niLight)
{
	return std::find(lights.begin(), lights.end(), a_niLight) == lights.end();
}

bool ProcessedLights::emplace_back(const LightSourceData& a_lightSrcData, RE::NiPointLight* a_niLight, RE::BSLight* a_bsLight, RE::NiAVObject* a_debugMarker, RE::TESObjectREFR* a_ref, float a_scale)
{
	if (IsNewLight(a_niLight)) {
		lights.emplace_back(a_lightSrcData, a_bsLight, a_niLight, a_debugMarker, a_ref, a_scale);
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
		if (a_show) {
			light.ShowDebugMarker();
		} else {
			light.HideDebugMarker();
		}
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
	auto conditionUpdateFlags = ConditionUpdateFlags::Skip;
	if (firstLoad) {
		conditionUpdateFlags = ConditionUpdateFlags::Forced;
	} else if (UpdateTimer(a_params.delta, 1.0f)) {
		conditionUpdateFlags = ConditionUpdateFlags::Normal;
	}

	const bool  withinFlickerDistance = a_params.ref->GetPosition().GetSquaredDistance(a_params.pcPos) < 67108864.0f;  // 8192.0f * 8192.0f
	const float scale = withinFlickerDistance ? a_params.ref->GetScale() : 1.0f;

	for (auto& lightData : lights) {
		if (!lightData.GetLight() || lightData.DimLight(a_params.dimFactor)) {
			continue;
		}

		lightData.UpdateConditions(a_params.ref, nodeVisHelper, conditionUpdateFlags);

		if (!lightData.GetLight()->GetAppCulled() && withinFlickerDistance) {
			lightData.UpdateAnimation(a_params.delta, scale);
			lightData.UpdateVanillaFlickering();
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
	stl::unique_insert(updatingLights, a_handle);

	for (auto& lightData : a_processedLights.lights) {
		if (a_isObject && lightData.data.emittanceForm) {
			stl::unique_insert(emittanceLights, a_handle);
			break;
		}
	}
}

void LightsToUpdate::emplace(RE::RefHandle a_handle)
{
	stl::unique_insert(updatingLights, a_handle);
}

void LightsToUpdate::erase(RE::RefHandle a_handle)
{
	stl::unique_erase(updatingLights, a_handle);
	stl::unique_erase(emittanceLights, a_handle);
}
