#include "PlacedLights.h"

#include "Settings.h"

void PlacedLight::NodeVisHelper::InsertConditionalNodes(const std::vector<std::string>& a_nodes, bool a_isVisble)
{
	for (const auto& nodeName : a_nodes) {
		conditionalNodes.insert_or_assign(nodeName, a_isVisble);
	}
}

void PlacedLight::NodeVisHelper::UpdateNodeVisibility(const RE::TESObjectREFR* a_ref, std::string_view a_nodeName)
{
	if (canCullAddonNodes || canCullNodes) {
		RE::NiAVObject* node = nullptr;
		if (a_nodeName.empty()) {
			node = a_ref->Get3D();
		} else {
			node = RE::GetObjectByName(a_ref->Get3D(), a_nodeName);
		}
		if (node) {
			if (canCullAddonNodes) {
				RE::ToggleMasterParticleAddonNodes(node->AsNode(), isVisible);
			}
			if (canCullNodes) {
				RE::BSVisit::TraverseScenegraphObjects(node, [&](RE::NiAVObject* a_obj) {
					if (const auto it = conditionalNodes.find(a_obj->name.c_str()); it != conditionalNodes.end()) {
						a_obj->SetAppCulled(!it->second);
					}
					return RE::BSVisit::BSVisitControl::kContinue;
				});
			}
		}
		Reset();
	}
}

void PlacedLight::NodeVisHelper::Reset()
{
	isVisible = false;
	canCullAddonNodes = false;
	canCullNodes = false;
	conditionalNodes.clear();
}

PlacedLight::PlacedLight(const LIGH::LightDefinitionPtr& a_lightDef, const LightInstance& a_lightInstance, const RE::TESObjectREFRPtr& a_ref) :
	definition(a_lightDef),
	instance(a_lightInstance),
	emittanceForm(GetEmittanceForm(a_lightDef, a_ref))
{
	if (a_lightDef->HasControllers()) {
		lightControllers = std::make_unique<LightControllers>(*a_lightDef);
	}
}

RE::TESForm* PlacedLight::GetEmittanceForm(const LIGH::LightDefinitionPtr& a_lightDef, const RE::TESObjectREFRPtr& a_ref)
{
	if (a_lightDef->data.emittanceForm) {
		return a_lightDef->data.emittanceForm;
	}

	if (a_lightDef->data.flags.none(LIGHT_FLAGS::NoExternalEmittance)) {
		auto xData = a_ref->extraList.GetByType<RE::ExtraEmittanceSource>();
		return xData ? xData->source : nullptr;
	}

	return nullptr;
}

void PlacedLight::ReattachLight(RE::TESObjectREFR* a_ref)
{
	const auto& niLight = GetLight();

	if (!niLight || !niLight->parent) {
		return;
	}

	instance = GetData().GenLight(a_ref, niLight->parent, niLight->name, a_ref->GetScale());

	if (Settings::GetSingleton()->CanShowDebugMarkers()) {
		instance.ShowDebugMarker();
	}
}

bool PlacedLight::ShouldUpdateConditions(const ConditionUpdateFlags a_flags) const
{
	auto& data = GetData();

	if (!data.conditions || a_flags == ConditionUpdateFlags::Skip) {
		return false;
	}

	auto& niLight = GetLight();

	const REX::TEnumSet<LIGHT_CULL_FLAGS, std::uint8_t> cullFlags{ LightData::GetCulledFlag(niLight.get()) };

	if (cullFlags.any(LIGHT_CULL_FLAGS::Game, LIGHT_CULL_FLAGS::Script)) {
		return false;
	}

	if (a_flags == ConditionUpdateFlags::Forced) {
		return true;
	}

	const bool requiresCellTransition = data.flags.any(LIGHT_FLAGS::UpdateOnCellTransition);
	const bool requiresWaiting = data.flags.any(LIGHT_FLAGS::UpdateOnWaiting);

	if (requiresCellTransition || requiresWaiting) {
		if (requiresCellTransition && requiresWaiting) {
			return (a_flags & ConditionUpdateFlags::UpdateRequired) != 0;
		}

		if (requiresCellTransition) {
			return (a_flags & ConditionUpdateFlags::CellTransition) != 0;
		}

		if (requiresWaiting) {
			return (a_flags & ConditionUpdateFlags::Waiting) != 0;
		}
	}

	return true;
}

void PlacedLight::UpdateAnimation(float a_delta, float a_scale) const
{
	if (lightControllers) {
		lightControllers->UpdateAnimation(GetLight(), a_delta, GetScalingFactor(a_scale));
	}
}

void PlacedLight::UpdateConditions(RE::TESObjectREFR* a_ref, std::unique_ptr<NodeVisHelper>& a_nodeVisHelper, ConditionUpdateFlags a_flags)
{
	if (!ShouldUpdateConditions(a_flags)) {
		return;
	}

	if (a_flags != ConditionUpdateFlags::Normal) {
		lastVisibleState = std::nullopt;
	}

	auto& data = GetData();

	const bool isVisible = data.conditions->IsTrue(a_ref, a_ref);
	if (lastVisibleState != isVisible) {
		lastVisibleState = isVisible;

		instance.CullLight(!isVisible, LIGHT_CULL_FLAGS::Conditions);

		if (!a_nodeVisHelper) {
			a_nodeVisHelper = std::make_unique<NodeVisHelper>();
		}

		a_nodeVisHelper->isVisible |= isVisible;
		a_nodeVisHelper->canCullAddonNodes |= data.flags.any(LIGHT_FLAGS::SyncAddonNodes);
		a_nodeVisHelper->canCullNodes |= !data.conditionalNodes.empty();

		if (!data.conditionalNodes.empty()) {
			a_nodeVisHelper->InsertConditionalNodes(data.conditionalNodes, isVisible);
		}
	}
}

void PlacedLight::UpdateEmittance(RE::TESObjectCELL* a_cell) const
{
	auto& niLight = GetLight();

	if (niLight && emittanceForm) {
		auto emittanceColor = RE::COLOR_WHITE;
		if (const auto lightForm = emittanceForm->As<RE::TESObjectLIGH>()) {
			emittanceColor = lightForm->emittanceColor;
		} else if (const auto region = emittanceForm->As<RE::TESRegion>()) {
			auto& emittanceSrcMap = a_cell->loadedData->emittanceSourceRefMap;

			emittanceColor = region->emittanceColor;
			if (emittanceColor == RE::COLOR_BLACK || emittanceSrcMap.find(region) == emittanceSrcMap.end()) {
				RE::UpdateRegionEmittance(emittanceColor, region);
			}
		}
		niLight->diffuse = GetData().GetDiffuse() * emittanceColor;
	}
}

void PlacedLight::UpdateVanillaFlickering(float a_delta, float a_scale) const
{
	auto& niLight = GetLight();
	auto& tesLight = GetData().light;

	if (tesLight->data.flags.any(RE::TES_LIGHT_FLAGS::kFlicker, RE::TES_LIGHT_FLAGS::kFlickerSlow)) {
		const auto flickerDelta = a_delta * tesLight->data.flickerPeriodRecip;

		thread_local auto rng = REX::TRandom<float>();

		auto constAttenOffset = niLight->constAttenuation + (rng.Generate(1.1f, 13.1f) * flickerDelta);
		auto linearAttenOffset = niLight->linearAttenuation + (rng.Generate(1.2f, 13.2f) * flickerDelta);
		auto quadraticAttenOffset = niLight->quadraticAttenuation + (rng.Generate(1.3f, 19.3f) * flickerDelta);

		constAttenOffset = std::fmod(constAttenOffset, RE::NI_TWO_PI);
		linearAttenOffset = std::fmod(linearAttenOffset, RE::NI_TWO_PI);
		quadraticAttenOffset = std::fmod(quadraticAttenOffset, RE::NI_TWO_PI);

		niLight->constAttenuation = constAttenOffset;
		niLight->linearAttenuation = linearAttenOffset;
		niLight->quadraticAttenuation = quadraticAttenOffset;

		if (!lightControllers || !lightControllers->positionController) {
			const auto constAttenSine = RE::NiSinQ(constAttenOffset + 1.7f);
			const auto linearAttenSine = RE::NiSinQ(linearAttenOffset + 0.5f);

			auto flickerMovementMult = ((tesLight->data.flickerMovementAmplitude * constAttenSine) * linearAttenSine) * 0.5f;
			if ((flickerMovementMult + tesLight->data.flickerMovementAmplitude) <= 0.0f) {
				flickerMovementMult = 0.0f;
			}

			niLight->local.translate.x = flickerMovementMult * constAttenSine;
			niLight->local.translate.y = flickerMovementMult * linearAttenSine;
			niLight->local.translate.z = flickerMovementMult * RE::NiSinQ(quadraticAttenOffset + 0.3f);

			RE::UpdateNode(niLight.get());
		}

		if (!lightControllers || !lightControllers->fadeController) {
			const auto halfIntensityAmplitude = tesLight->data.flickerIntensityAmplitude * 0.5f;

			const auto flickerIntensity = std::clamp((RE::NiSinQImpl(linearAttenOffset * 1.3f * (512.0f / RE::NI_TWO_PI) + 52.966763f) + 1.0f) * 0.5f *
															 (RE::NiSinQImpl(constAttenOffset * 1.1f * (512.0f / RE::NI_TWO_PI) + 152.38132f) + 1.0f) * 0.5f * 0.33333331f +
														 RE::NiSinQImpl(quadraticAttenOffset * 3.0f * (512.0f / RE::NI_TWO_PI) + 73.3386f) * 0.2f,
				-1.0f, 1.0f);

			niLight->fade = ((halfIntensityAmplitude * flickerIntensity) + (1.0f - halfIntensityAmplitude)) * GetData().GetScaledFade(a_scale);
		}

	} else {
		if (tesLight->data.flags.none(RE::TES_LIGHT_FLAGS::kPulse, RE::TES_LIGHT_FLAGS::kPulseSlow)) {
			return;
		}

		auto constAttenuation = std::fmod(niLight->constAttenuation + (RE::BSTimer::GetSingleton()->delta * tesLight->data.flickerPeriodRecip), RE::NI_TWO_PI);
		niLight->constAttenuation = constAttenuation;

		auto constAttenCosine = RE::NiCosQ(constAttenuation);
		auto constAttenSine = RE::NiSinQ(constAttenuation);

		if (!lightControllers || !lightControllers->fadeController) {
			const auto halfIntensityAmplitude = tesLight->data.flickerIntensityAmplitude * 0.5f;
			niLight->fade = ((constAttenCosine * halfIntensityAmplitude) + (1.0f - halfIntensityAmplitude)) * GetData().GetScaledFade(a_scale);
		}

		if (!lightControllers || !lightControllers->positionController) {
			const auto movementAmplitude = tesLight->data.flickerMovementAmplitude;

			niLight->local.translate.x = movementAmplitude * constAttenCosine;
			niLight->local.translate.y = movementAmplitude * constAttenSine;
			niLight->local.translate.z = movementAmplitude * (constAttenSine * constAttenCosine);

			RE::UpdateNode(niLight.get());
		}
	}
}

PlacedLights::PlacedLights(const LIGH::LightDefinitionPtr& a_lightDef, const LightInstance& a_lightInstance, const RE::TESObjectREFRPtr& a_ref)
{
	lights.emplace_back(a_lightDef, a_lightInstance, a_ref);
}

bool PlacedLights::emplace_back(const LIGH::LightDefinitionPtr& a_lightDef, const LightInstance& a_lightInstance, const RE::TESObjectREFRPtr& a_ref)
{
	if (std::find(lights.begin(), lights.end(), a_lightInstance) == lights.end()) {
		lights.emplace_back(a_lightDef, a_lightInstance, a_ref);
		return true;
	}
	return false;
}

void PlacedLights::ShowDebugMarkers(bool a_show) const
{
	for (auto& light : lights) {
		if (a_show) {
			light.instance.ShowDebugMarker();
		} else {
			light.instance.HideDebugMarker();
		}
	}
}

void PlacedLights::ToggleLights(bool a_toggle, LIGHT_CULL_FLAGS a_flags) const
{
	for (auto& light : lights) {
		light.instance.CullLight(a_toggle, a_flags);
	}
}

bool PlacedLights::GetLightsToggled(LIGHT_CULL_FLAGS a_flags) const
{
	for (auto& light : lights) {
		if (auto& niLight = light.GetLight()) {
			if (niLight->GetAppCulled() && (static_cast<uint32_t>(LightData::GetCulledFlag(niLight.get())) & static_cast<uint32_t>(a_flags)) != 0) {
				return true;
			}
		}
	}
	return false;
}

void PlacedLights::ReattachLights(RE::TESObjectREFR* a_ref)
{
	for (auto& light : lights) {
		light.ReattachLight(a_ref);
	}
}

void PlacedLights::ReattachLights() const
{
	for (auto& light : lights) {
		light.instance.ReattachLight();
	}
}

void PlacedLights::RemoveLights(bool a_clearData) const
{
	for (auto& light : lights) {
		light.instance.RemoveLight(a_clearData);
	}
}

bool PlacedLights::UpdateTimer(float a_delta, float a_interval)
{
	lastUpdateTime += a_delta;
	if (lastUpdateTime >= a_interval) {
		lastUpdateTime = 0.0f;
		return true;
	}
	return false;
}

void PlacedLights::UpdateConditions(RE::TESObjectREFR* a_ref, std::string_view a_nodeName, ConditionUpdateFlags a_flags)
{
	if (nodeVisHelper) {
		nodeVisHelper->Reset();
	}

	for (auto& placedLight : lights) {
		placedLight.UpdateConditions(a_ref, nodeVisHelper, a_flags);
	}

	if (nodeVisHelper) {
		nodeVisHelper->UpdateNodeVisibility(a_ref, a_nodeName);
	}
}

void PlacedLights::UpdateLightsAndRef(const UpdateParams& a_params)
{
	auto conditionUpdateFlags = ConditionUpdateFlags::Skip;
	if (lastUpdateTime == std::numeric_limits<float>::max()) {
		lastUpdateTime = 0.0f;
		conditionUpdateFlags = ConditionUpdateFlags::Forced;
	} else if (UpdateTimer(a_params.delta, 1.0f)) {
		conditionUpdateFlags = ConditionUpdateFlags::Normal;
	}

	const bool  withinFlickerDistance = a_params.ref->GetPosition().GetSquaredDistance(a_params.pcPos) < 67108864.0f;  // 8192.0f * 8192.0f
	const float scale = withinFlickerDistance ? a_params.ref->GetScale() : 1.0f;
	const bool  dimming = LightInstance::IsDimming(a_params.dimFactor);

	for (auto& placedLight : lights) {
		auto& niLight = placedLight.GetLight();

		if (!niLight) {
			continue;
		}

		placedLight.UpdateConditions(a_params.ref, nodeVisHelper, conditionUpdateFlags);

		if (niLight->GetAppCulled()) {
			continue;
		}

		if (dimming) {
			niLight->fade = placedLight.GetData().GetScaledFade(scale);
		}

		if (withinFlickerDistance) {
			placedLight.UpdateAnimation(a_params.delta, scale);
			placedLight.UpdateVanillaFlickering(a_params.delta, scale);
		}

		if (dimming) {
			placedLight.instance.DimLight(a_params.dimFactor);
		}
	}

	if (nodeVisHelper) {
		nodeVisHelper->UpdateNodeVisibility(a_params.ref, a_params.nodeName);
	}
}

void PlacedLights::UpdateEmittance(RE::TESObjectCELL* a_cell) const
{
	for (auto& light : lights) {
		light.UpdateEmittance(a_cell);
	}
}

float PlacedLights::GetDimFactor(bool a_finished, float a_age, float a_lifetime)
{
	constexpr auto MAX_WAIT_TIME = 3.0f;

	if (!a_finished) {
		finishedLifetime = -1.0f; 
		return 1.0f;
	}
	if (finishedLifetime < 0.0f) {
		finishedLifetime = a_age;
	}

	const bool  hasLifetime = a_lifetime < std::numeric_limits<float>::max();
	const float fadeStart = hasLifetime ? a_lifetime : finishedLifetime;
	return std::clamp(1.0f - (a_age - fadeStart) / MAX_WAIT_TIME, 0.0f, 1.0f);
}
