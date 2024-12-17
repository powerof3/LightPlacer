#include "ProcessedLights.h"

#include "SourceData.h"

bool ProcessedLights::IsNewLight(RE::NiPointLight* a_niLight)
{
	return std::find(lights.begin(), lights.end(), a_niLight) == lights.end();
}

bool ProcessedLights::emplace_back(const SourceData& a_srcData, const LightSourceData& a_lightSrcData, RE::NiPointLight* a_niLight, RE::BSLight* a_bsLight)
{
	if (IsNewLight(a_niLight)) {
		lights.emplace_back(a_lightSrcData, a_bsLight, a_niLight, a_srcData.ref, a_srcData.scale);
		return true;
	}
	return false;
}

void ProcessedLights::emplace_back(const REFR_LIGH& a_lightREFRData)
{
	lights.emplace_back(a_lightREFRData);
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

void ProcessedLights::UpdateLightsAndRef(RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_pcPos, float a_flickeringDistance, float a_delta, std::string_view a_nodeName, float a_dimFactor)
{
	const bool  updateConditions = UpdateTimer(a_delta, 0.25f);
	const bool  withinFlickerDistance = a_ref->IsPlayerRef() || a_ref->GetPosition().GetSquaredDistance(a_pcPos) < a_flickeringDistance;
	const float scale = withinFlickerDistance ? a_ref->GetScale() : 1.0f;

	bool shouldCullAddonNodes = false;
	bool cullAddonNodes = false;

	for (auto& lightData : lights) {
		if (a_dimFactor <= 1.0f) {
			lightData.DimLight(a_dimFactor);
			continue;
		}
		if (updateConditions) {
			lightData.UpdateConditions(a_ref, shouldCullAddonNodes, cullAddonNodes);
		}
		lightData.UpdateAnimation(withinFlickerDistance, scale);
		if (withinFlickerDistance) {
			lightData.UpdateFlickering();
		}
	}

	if (shouldCullAddonNodes) {
		RE::NiAVObject* node = nullptr;
		if (a_nodeName.empty()) {
			node = a_ref->Get3D();
		} else {
			node = a_ref->Get3D()->GetObjectByName(a_nodeName);
		}
		if (node) {
			RE::ToggleMasterParticleAddonNodes(node->AsNode(), cullAddonNodes);
		}
	}
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
