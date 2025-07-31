#include "ProcessedLights.h"

bool ProcessedLights::emplace_back(const LIGH::LightSourceData& a_lightSrcData, const LightOutput& a_lightOutput, const RE::TESObjectREFRPtr& a_ref, float a_scale)
{
	if (std::find(lights.begin(), lights.end(), a_lightOutput) == lights.end()) {
		lights.emplace_back(a_lightSrcData, a_lightOutput, a_ref, a_scale);
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
			light.output.ShowDebugMarker();
		} else {
			light.output.HideDebugMarker();
		}
	}
}

void ProcessedLights::ToggleLightsScript(bool a_toggle)
{
	for (auto& light : lights) {
		if (auto& niLight = light.GetLight()) {
			auto& debugMarker = light.output.debugMarker;
			LightData::CullLight(niLight.get(), debugMarker.get(), a_toggle, LIGHT_CULL_FLAGS::Script);
		}
	}
}

bool ProcessedLights::GetLightsToggledScript() const
{
	for (auto& light : lights) {
		if (auto& niLight = light.GetLight()) {
			if (niLight->GetAppCulled()) {
				return true;
			}
		}
	}
	return false;
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
		light.output.ReattachLight();
	}
}

void ProcessedLights::RemoveLights(bool a_clearData) const
{
	for (auto& light : lights) {
		light.output.RemoveLight(a_clearData);
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
		auto& niLight = lightData.output.GetLight();

		if (!niLight || lightData.output.DimLight(a_params.dimFactor)) {
			continue;
		}

		lightData.UpdateConditions(a_params.ref, nodeVisHelper, conditionUpdateFlags);

		if (!niLight->GetAppCulled() && withinFlickerDistance) {
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
