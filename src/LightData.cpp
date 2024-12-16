#include "LightData.h"
#include "ConditionParser.h"
#include "Settings.h"
#include "SourceData.h"

bool LightData::IsValid() const
{
	return light != nullptr;
}

std::string LightData::GetName(const SourceData& a_srcData, std::uint32_t a_index) const
{
	std::size_t seed = 0;
	boost::hash_combine(seed, light->GetFormID());
	boost::hash_combine(seed, radius);
	boost::hash_combine(seed, fade);
	if (a_srcData.effectID != std::numeric_limits<std::uint32_t>::max()) {
		boost::hash_combine(seed, a_srcData.effectID);
	}

	return std::format("{}[{}]#{}", LP_LIGHT, seed, a_index);
}

std::string LightData::GetNodeName(const RE::NiPoint3& a_point, std::uint32_t a_index)
{
	return std::format("{}[{},{},{}]#{}", LP_NODE, a_point.x, a_point.y, a_point.z, a_index);
}

std::string LightData::GetNodeName(RE::NiAVObject* a_obj, std::uint32_t a_index)
{
	return std::format("{}[{}]#{}", LP_NODE, a_obj->name.c_str(), a_index);
}

bool LightData::IsDynamicLight(RE::TESObjectREFR* a_ref) const
{
	if (light->data.flags.any(RE::TES_LIGHT_FLAGS::kDynamic) || GetCastsShadows()) {
		return true;
	}

	if (a_ref) {
		if (RE::IsActor(a_ref)) {
			return true;
		}
		if (const auto baseObject = a_ref->GetBaseObject(); baseObject && baseObject->IsInventoryObject()) {
			return true;
		}
	}

	return false;
}

void LightData::AttachDebugMarker(RE::NiNode* a_node) const
{
	const auto settings = Settings::GetSingleton();

	if (!settings->LoadDebugMarkers()) {
		return;
	}

	const auto get_marker = [this] {
		if (GetCastsShadows()) {
			if (light->data.flags.any(RE::TES_LIGHT_FLAGS::kSpotShadow)) {
				return std::make_tuple("marker_spotlight.nif", 1.0f, true);
			}
			return std::make_tuple("marker_lightshadow.nif", 0.25f, false);
		}
		return std::make_tuple("marker_light.nif", 0.25f, false);
	};

	RE::NiPointer<RE::NiNode>                   loadedModel;
	constexpr RE::BSModelDB::DBTraits::ArgsType args{};

	auto [model, scale, flip] = get_marker();
	if (const auto error = Demand(model, loadedModel, args); error == RE::BSResource::ErrorCode::kNone) {
		if (const auto clonedModel = loadedModel->Clone()) {
			if (!settings->CanShowDebugMarkers()) {
				clonedModel->SetAppCulled(true);
			}
			clonedModel->name = LP_DEBUG;
			clonedModel->local.scale = scale;
			if (flip) {
				clonedModel->local.rotate.SetEulerAnglesXYZ(RE::deg_to_rad(-180), 0, RE::deg_to_rad(-180));
			}
			RE::AttachNode(a_node, clonedModel);
		}
	}
}

void LightData::ToggleAddonNodes(RE::TESObjectREFR* a_ref, bool a_enable) const
{
	if (flags.any(Flags::SyncAddonNodes)) {
		if (const auto root = a_ref->Get3D()) {
			RE::ToggleMasterParticleAddonNodes(root->AsNode(), a_enable);
		}
	}
}

bool LightData::GetCastsShadows() const
{
	return flags.any(Flags::Shadow) /*|| light->data.flags.any(RE::TES_LIGHT_FLAGS::kOmniShadow, RE::TES_LIGHT_FLAGS::kHemiShadow, RE::TES_LIGHT_FLAGS::kSpotShadow)*/;
}

RE::NiColor LightData::GetDiffuse() const
{
	auto diffuse = (color == RE::COLOR_BLACK) ? RE::NiColor(light->data.color) : color;
	return light->data.flags.any(RE::TES_LIGHT_FLAGS::kNegative) ? -diffuse : diffuse;
}

float LightData::GetRadius() const
{
	return radius > 0.0f ? radius : static_cast<float>(light->data.radius);
}

float LightData::GetFade() const
{
	return fade > 0.0f ? fade : light->fade;
}

float LightData::GetScaledRadius(float a_radius, float a_scale) const
{
	return flags.any(Flags::IgnoreScale) ?
	           a_radius :
	           a_radius * a_scale;
}

float LightData::GetScaledFade(float a_fade, float a_scale) const
{
	return flags.any(Flags::IgnoreScale) ?
	           a_fade :
	           a_fade * a_scale;
}

float LightData::GetScaledRadius(float a_scale) const
{
	return GetScaledRadius(GetRadius(), a_scale);
}

float LightData::GetScaledFade(float a_scale) const
{
	return GetScaledFade(GetFade(), a_scale);
}

float LightData::GetFOV() const
{
	if (!GetCastsShadows()) {
		return 1.0;
	}
	if (light->data.flags.any(RE::TES_LIGHT_FLAGS::kSpotShadow)) {
		return RE::deg_to_rad(fov > 0.0f ? fov : light->data.fov);
	}
	if (light->data.flags.any(RE::TES_LIGHT_FLAGS::kHemiShadow)) {
		return RE::NI_PI;
	}
	return RE::NI_TWO_PI;
}

float LightData::GetFalloff() const
{
	return GetCastsShadows() ? light->data.fallofExponent : 1.0f;
}

float LightData::GetNearDistance() const
{
	return GetCastsShadows() ? light->data.nearDistance : 5.0f;
}

RE::ShadowSceneNode::LIGHT_CREATE_PARAMS LightData::GetParams(RE::TESObjectREFR* a_ref) const
{
	RE::ShadowSceneNode::LIGHT_CREATE_PARAMS params{};
	params.dynamic = IsDynamicLight(a_ref);
	params.shadowLight = GetCastsShadows();
	params.portalStrict = GetPortalStrict();
	params.affectLand = a_ref ? (a_ref->GetFormFlags() & RE::TESObjectREFR::RecordFlags::kDoesntLightLandscape) == 0 : true;
	params.affectWater = a_ref ? (a_ref->GetFormFlags() & RE::TESObjectREFR::RecordFlags::kDoesntLightWater) == 0 : true;
	params.neverFades = a_ref ? !a_ref->IsHeadingMarker() : true;
	params.fov = GetFOV();
	params.falloff = GetFalloff();
	params.nearDistance = GetNearDistance();
	params.depthBias = shadowDepthBias;
	params.sceneGraphIndex = 0;
	params.restrictedNode = nullptr;
	params.lensFlareData = light->lensFlare;
	return params;
}

bool LightData::GetPortalStrict() const
{
	return flags.any(Flags::PortalStrict) || light->data.flags.any(RE::TES_LIGHT_FLAGS::kPortalStrict);
}

std::pair<RE::BSLight*, RE::NiPointLight*> LightData::GenLight(RE::TESObjectREFR* a_ref, RE::NiNode* a_node, std::string_view a_lightName, float a_scale) const
{
	RE::BSLight*      bsLight = nullptr;
	RE::NiPointLight* niLight = nullptr;

	niLight = netimmerse_cast<RE::NiPointLight*>(a_node->GetObjectByName(a_lightName));
	if (!niLight) {
		niLight = RE::NiPointLight::Create();
		niLight->name = a_lightName;
		RE::AttachNode(a_node, niLight);
		AttachDebugMarker(a_node);
	}

	if (niLight) {
		niLight->ambient = RE::NiColor();
		niLight->ambient.red = static_cast<float>(flags.underlying());

		niLight->diffuse = GetDiffuse();

		const auto lightRadius = GetScaledRadius(a_scale);
		niLight->radius.x = lightRadius;
		niLight->radius.y = lightRadius;
		niLight->radius.z = lightRadius;

		auto* shadowSceneNode = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
		if (bsLight = shadowSceneNode->GetPointLight(niLight); !bsLight) {
			bsLight = shadowSceneNode->AddLight(niLight, GetParams(a_ref));
		}

		niLight->SetLightAttenuation(lightRadius);
		niLight->fade = GetScaledFade(a_scale);

		if (conditions && !conditions->IsTrue(a_ref, a_ref)) {
			niLight->SetAppCulled(true);
			ToggleAddonNodes(a_ref, false);
		}

		if (Settings::GetSingleton()->CanShowDebugMarkers()) {
			if (const auto debugMarker = a_node->GetObjectByName(LP_DEBUG)) {
				debugMarker->SetAppCulled(false);
			}
		}
	}

	return { bsLight, niLight };
}

LightSourceData::LightSourceData(const RE::NiStringsExtraData* a_data)
{
	constexpr auto get_pt3 = []<class T>(const std::string& a_value, T& valueOut) {
		if (const auto pt3 = string::split(a_value, ","sv); pt3.size() == 3) {
			valueOut[0] = string::to_num<float>(pt3[0]);
			valueOut[1] = string::to_num<float>(pt3[1]);
			valueOut[2] = string::to_num<float>(pt3[2]);
		}
	};

	std::vector<std::string> xData(a_data->value, a_data->value + a_data->size);

	for (const auto& str : xData) {
		std::istringstream strstream(str);
		std::string        key;
		std::string        value;

		if (std::getline(strstream, key, ':')) {
			std::getline(strstream, value);
			string::trim(value);

			switch (string::const_hash(key)) {
			case "light"_h:
				{
					lightEDID = value;
					data.light = RE::TESForm::LookupByEditorID<RE::TESObjectLIGH>(value);
				}
				break;
			case "color"_h:
				get_pt3(value, data.color);
				break;
			case "radius"_h:
				data.radius = string::to_num<float>(value);
				break;
			case "fade"_h:
				data.fade = string::to_num<float>(value);
				break;
			case "offset"_h:
				get_pt3(value, data.offset);
				break;
			case "emittanceForm"_h:
				{
					emittanceFormEDID = value;
					data.emittanceForm = RE::TESForm::LookupByEditorID(value);
				}
				break;
			case "flags"_h:
				flags = value;
				break;
			case "condition"_h:
				conditions.push_back(value);
				break;
			default:
				break;
			}
		}
	}

	if (data.IsValid()) {
		ReadFlags();
		ReadConditions();
	}
}

void LightSourceData::ReadFlags()
{
	// glaze doesn't reflect flag enums
	if (!flags.empty()) {
		const auto flagStrs = string::split(flags, "|");

		for (const auto& flagStr : flagStrs) {
			switch (string::const_hash(flagStr)) {
			case "PortalStrict"_h:
				data.flags.set(LightData::Flags::PortalStrict);
				break;
			case "Shadow"_h:
				data.flags.set(LightData::Flags::Shadow);
				break;
			case "Simple"_h:
				data.flags.set(LightData::Flags::Simple);
				break;
			case "SyncAddonNodes"_h:
				data.flags.set(LightData::Flags::SyncAddonNodes);
				break;
			case "IgnoreScale"_h:
				data.flags.set(LightData::Flags::IgnoreScale);
				break;
			case "RandomAnimStart"_h:
				data.flags.set(LightData::Flags::RandomAnimStart);
				break;
			case "NoExternalEmittance"_h:
				data.flags.set(LightData::Flags::NoExternalEmittance);
				break;
			default:
				break;
			}
		}
	}
}

void LightSourceData::ReadConditions()
{
	if (!conditions.empty()) {
		ConditionParser::BuildCondition(data.conditions, conditions);
	}
}

bool LightSourceData::PostProcess()
{
	if (!lightEDID.contains("|")) {
		data.light = RE::TESForm::LookupByEditorID<RE::TESObjectLIGH>(lightEDID);		
	} else {
		auto edids = string::split(lightEDID, "|");
		for (auto& edid : edids) {
			if (auto form = RE::TESForm::LookupByEditorID<RE::TESObjectLIGH>(edid)) {
				data.light = form;
				break;
			}
		}
	}

	if (!data.IsValid()) {
		return false;
	}

	data.emittanceForm = RE::TESForm::LookupByEditorID(emittanceFormEDID);

	ReadFlags();
	ReadConditions();

	return true;
}

bool LightSourceData::IsStaticLight() const
{
	return data.offset == RE::NiPoint3::Zero() && data.rotation == RE::MATRIX_ZERO && positionController.empty() && rotationController.empty();
}

RE::NiNode* LightSourceData::GetOrCreateNode(RE::NiNode* a_root, const RE::NiPoint3& a_point, std::uint32_t a_index) const
{
	if (a_point == RE::NiPoint3::Zero() && IsStaticLight()) {
		return a_root;
	}

	auto name = LightData::GetNodeName(a_point, a_index);
	auto node = a_root->GetObjectByName(name);
	if (!node) {
		node = RE::NiNode::Create(1);
		node->name = name;
		node->local.translate = a_point + data.offset;
		node->local.rotate = data.rotation;
		RE::AttachNode(a_root, node);
	}

	return node->AsNode();
}

RE::NiNode* LightSourceData::GetOrCreateNode(RE::NiNode* a_root, const std::string& a_nodeName, std::uint32_t a_index) const
{
	const auto obj = a_root->GetObjectByName(a_nodeName);
	return obj ? GetOrCreateNode(a_root, obj, a_index) : nullptr;
}

RE::NiNode* LightSourceData::GetOrCreateNode(RE::NiNode* a_root, RE::NiAVObject* a_obj, std::uint32_t a_index) const
{
	if (const auto node = a_obj->AsNode()) {
		if (IsStaticLight()) {
			return node;
		}
	}

	const auto name = LightData::GetNodeName(a_obj, a_index);
	if (const auto node = a_root->GetObjectByName(name)) {
		return node->AsNode();
	}
	auto geometry = a_obj->AsGeometry();
	if (const auto newNode = RE::NiNode::Create(1); newNode) {
		newNode->name = name;
		if (geometry) {
			newNode->local.translate = geometry->modelBound.center;
		}
		newNode->local.translate += data.offset;
		newNode->local.rotate = data.rotation;
		RE::AttachNode(geometry ? a_root : a_obj->AsNode(), newNode);
		return newNode;
	}

	return nullptr;
}

REFR_LIGH::REFR_LIGH(const LightSourceData& a_lightSource, RE::BSLight* a_bsLight, RE::NiPointLight* a_niLight, RE::TESObjectREFR* a_ref, float a_scale) :
	data(a_lightSource.data),
	bsLight(a_bsLight),
	niLight(a_niLight),
	scale(a_scale),
	isReference(!RE::IsActor(a_ref))
{
	if (!data.emittanceForm && data.flags.none(LightData::Flags::NoExternalEmittance)) {
		auto xData = a_ref->extraList.GetByType<RE::ExtraEmittanceSource>();
		data.emittanceForm = xData ? xData->source : nullptr;
	}

	const bool randomAnimStart = data.flags.any(LightData::Flags::RandomAnimStart);

#define INIT_CONTROLLER(controller)                                                           \
	if (!a_lightSource.controller.empty()) {                                                  \
		(controller) = Animation::LightController(a_lightSource.controller, randomAnimStart); \
	}

	INIT_CONTROLLER(colorController)
	INIT_CONTROLLER(radiusController)
	INIT_CONTROLLER(fadeController)
	INIT_CONTROLLER(positionController)
	INIT_CONTROLLER(rotationController)
#undef INIT_CONTROLLER

	if (a_niLight && a_niLight->parent) {
		debugMarker.reset(niLight->parent->GetObjectByName(LightData::LP_DEBUG));
	}
}

bool REFR_LIGH::IsAnimated() const
{
	return data.light->GetNoFlicker() || data.conditions || colorController || fadeController || radiusController || positionController || rotationController;
}

bool REFR_LIGH::DimLight(const float a_dimmer) const
{
	if (niLight && a_dimmer <= 1.0f) {
		niLight->fade *= a_dimmer;
		return true;
	}
	return false;
}

void REFR_LIGH::ReattachLight(RE::TESObjectREFR* a_ref)
{
	auto lights = data.GenLight(a_ref, niLight->parent, niLight->name, scale);

	bsLight.reset(lights.first);
	niLight.reset(lights.second);
}

void REFR_LIGH::ReattachLight() const
{
	if (bsLight) {
		RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0]->AddLight(bsLight.get());
	}

	if (Settings::GetSingleton()->CanShowDebugMarkers()) {
		ShowDebugMarker(true);
	}
}

void REFR_LIGH::RemoveLight(bool a_clearData) const
{
	if (Settings::GetSingleton()->CanShowDebugMarkers()) {
		ShowDebugMarker(false);
	}

	if (bsLight) {
		RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0]->RemoveLight(bsLight);
	}
	if (a_clearData) {
		if (niLight && niLight->parent) {
			niLight->parent->DetachChild(niLight.get());
		}
	}
}

void REFR_LIGH::ShowDebugMarker(bool a_show) const
{
	if (debugMarker) {
		debugMarker->SetAppCulled(!a_show);
	}
}

void REFR_LIGH::UpdateAnimation(bool a_withinRange, float a_scalingFactor)
{
	if (!niLight) {
		return;
	}

	// update color but not transforms when out of vanilla flicker distance

	if (colorController) {
		niLight->diffuse = colorController->GetValue(RE::BSTimer::GetSingleton()->delta);
	}

	if (!a_withinRange) {
		return;
	}

	scale = a_scalingFactor;

	if (radiusController) {
		const auto newRadius = data.GetScaledRadius(radiusController->GetValue(RE::BSTimer::GetSingleton()->delta), scale);
		niLight->radius.x = newRadius;
		niLight->radius.y = newRadius;
		niLight->radius.z = newRadius;
		niLight->SetLightAttenuation(newRadius);
	}
	if (fadeController) {
		niLight->fade = data.GetScaledFade(fadeController->GetValue(RE::BSTimer::GetSingleton()->delta), scale);
	}
	if (auto parentNode = niLight->parent) {
		if (positionController) {
			parentNode->local.translate = positionController->GetValue(RE::BSTimer::GetSingleton()->delta);
		}
		if (rotationController) {
			auto rotation = rotationController->GetValue(RE::BSTimer::GetSingleton()->delta);
			parentNode->local.rotate.SetEulerAnglesXYZ(RE::deg_to_rad(rotation[0]), RE::deg_to_rad(rotation[1]), RE::deg_to_rad(rotation[2]));
		}
		if (positionController || rotationController) {
			if (RE::TaskQueueInterface::ShouldUseTaskQueue()) {
				RE::TaskQueueInterface::GetSingleton()->QueueUpdateNiObject(parentNode);
			} else {
				RE::NiUpdateData updateData;
				parentNode->Update(updateData);
			}
		}
	}
}

void REFR_LIGH::UpdateConditions(RE::TESObjectREFR* a_ref)
{
	if (data.conditions && niLight) {
		const bool cullState = data.conditions->IsTrue(a_ref, a_ref);
		if (lastCulledState != cullState) {
			lastCulledState = cullState;

			niLight->SetAppCulled(!cullState);
			data.ToggleAddonNodes(a_ref, cullState);
			ShowDebugMarker(cullState);
		}
	}
}

void REFR_LIGH::UpdateFlickering() const
{
	if (data.light && niLight) {
		if (niLight->GetAppCulled()) {
			return;
		}
		UpdateLight();
	}
}

void REFR_LIGH::UpdateLight() const
{
	if (data.light->data.flags.any(RE::TES_LIGHT_FLAGS::kFlicker, RE::TES_LIGHT_FLAGS::kFlickerSlow)) {
		const auto flickerDelta = RE::BSTimer::GetSingleton()->delta * data.light->data.flickerPeriodRecip;

		auto constAttenOffset = niLight->constAttenuation + (clib_util::RNG().generate<float>(1.1f, 13.1f) * flickerDelta);
		auto linearAttenOffset = niLight->linearAttenuation + (clib_util::RNG().generate<float>(1.2f, 13.2f) * flickerDelta);
		auto quadraticAttenOffset = niLight->quadraticAttenuation + (clib_util::RNG().generate<float>(1.3f, 19.3f) * flickerDelta);

		constAttenOffset = std::fmod(constAttenOffset, RE::NI_TWO_PI);
		linearAttenOffset = std::fmod(linearAttenOffset, RE::NI_TWO_PI);
		quadraticAttenOffset = std::fmod(quadraticAttenOffset, RE::NI_TWO_PI);

		niLight->constAttenuation = constAttenOffset;
		niLight->linearAttenuation = linearAttenOffset;
		niLight->quadraticAttenuation = quadraticAttenOffset;

		if (!positionController) {
			const auto constAttenSine = RE::NiSinQ(constAttenOffset + 1.7f);
			const auto linearAttenSine = RE::NiSinQ(linearAttenOffset + 0.5f);

			auto flickerMovementMult = ((data.light->data.flickerMovementAmplitude * constAttenSine) * linearAttenSine) * 0.5f;
			if ((flickerMovementMult + data.light->data.flickerMovementAmplitude) <= 0.0f) {
				flickerMovementMult = 0.0f;
			}

			niLight->local.translate.x = flickerMovementMult * constAttenSine;
			niLight->local.translate.y = flickerMovementMult * linearAttenSine;
			niLight->local.translate.z = flickerMovementMult * RE::NiSinQ(quadraticAttenOffset + 0.3f);
		}

		if (!fadeController) {
			const auto halfIntensityAmplitude = data.light->data.flickerIntensityAmplitude * 0.5f;

			const auto flickerIntensity = std::clamp((RE::NiSinQImpl(linearAttenOffset * 1.3f * (512.0f / RE::NI_TWO_PI) + 52.966763f) + 1.0f) * 0.5f *
															 (RE::NiSinQImpl(constAttenOffset * 1.1f * (512.0f / RE::NI_TWO_PI) + 152.38132f) + 1.0f) * 0.5f * 0.33333331f +
														 RE::NiSinQImpl(quadraticAttenOffset * 3.0f * (512.0f / RE::NI_TWO_PI) + 73.3386f) * 0.2f,
				-1.0f, 1.0f);

			niLight->fade = ((halfIntensityAmplitude * flickerIntensity) + (1.0f - halfIntensityAmplitude)) * data.GetScaledFade(scale);
		}

	} else {
		if (data.light->data.flags.none(RE::TES_LIGHT_FLAGS::kPulse, RE::TES_LIGHT_FLAGS::kPulseSlow)) {
			return;
		}

		auto constAttenuation = std::fmod(niLight->constAttenuation + (RE::BSTimer::GetSingleton()->delta * data.light->data.flickerPeriodRecip), RE::NI_TWO_PI);
		niLight->constAttenuation = constAttenuation;

		auto constAttenCosine = RE::NiCosQ(constAttenuation);
		auto constAttenSine = RE::NiSinQ(constAttenuation);

		if (!fadeController) {
			const auto halfIntensityAmplitude = data.light->data.flickerIntensityAmplitude * 0.5f;
			niLight->fade = ((constAttenCosine * halfIntensityAmplitude) + (1.0f - halfIntensityAmplitude)) * data.GetScaledFade(scale);
		}

		if (!positionController) {
			const auto movementAmplitude = data.light->data.flickerMovementAmplitude;

			niLight->local.translate.x = movementAmplitude * constAttenCosine;
			niLight->local.translate.y = movementAmplitude * constAttenSine;
			niLight->local.translate.z = movementAmplitude * (constAttenSine * constAttenCosine);
		}
	}

	if (!positionController) {
		if (RE::TaskQueueInterface::ShouldUseTaskQueue()) {
			RE::TaskQueueInterface::GetSingleton()->QueueUpdateNiObject(niLight.get());
		} else {
			RE::NiUpdateData updateData;
			niLight->Update(updateData);
		}
	}
}

void REFR_LIGH::UpdateEmittance() const
{
	if (data.light && niLight && data.emittanceForm) {
		RE::NiColor emittanceColor(1.0, 1.0, 1.0);
		if (const auto lightForm = data.emittanceForm->As<RE::TESObjectLIGH>()) {
			emittanceColor = lightForm->emittanceColor;
		} else if (const auto region = data.emittanceForm->As<RE::TESRegion>()) {
			emittanceColor = region->emittanceColor;
		}
		niLight->diffuse = data.GetDiffuse() * emittanceColor;
	}
}

bool Timer::UpdateTimer(float a_delta, float a_interval)
{
	lastUpdateTime += a_delta;
	if (lastUpdateTime >= a_interval) {
		lastUpdateTime = 0.0f;
		return true;
	}
	return false;
}

bool Timer::UpdateTimer(float a_interval)
{
	return UpdateTimer(RE::BSTimer::GetSingleton()->delta, a_interval);
}

void ProcessedREFRLights::emplace(const REFR_LIGH& a_lightData, RE::RefHandle a_handle)
{
	if (a_lightData.IsAnimated()) {
		stl::unique_insert(animatedLights, a_handle);
	}

	if (a_lightData.isReference && a_lightData.data.emittanceForm) {
		stl::unique_insert(emittanceLights, a_handle);
	}
}

void ProcessedREFRLights::erase(RE::RefHandle a_handle)
{
	stl::unique_erase(animatedLights, a_handle);
	stl::unique_erase(emittanceLights, a_handle);
}
