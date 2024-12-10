#include "LightData.h"
#include "ConditionParser.h"

bool Timer::UpdateTimer(float a_interval)
{
	lastUpdateTime += RE::BSTimer::GetSingleton()->delta;
	if (lastUpdateTime >= a_interval) {
		lastUpdateTime = 0.0f;
		return true;
	}
	return false;
}

ObjectREFRParams::ObjectREFRParams(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_object, RE::TESModel* a_model) :
	ObjectREFRParams(a_ref, a_ref->Get3D(), a_object, a_model)
{}

ObjectREFRParams::ObjectREFRParams(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root, RE::TESBoundObject* a_object, RE::TESModel* a_model) :
	ref(a_ref),
	root(a_root->AsNode()),
	handle(a_ref->CreateRefHandle().native_handle()),
	baseID(a_object->GetFormID())
{
	RE::TESModel* model = a_model;
	if (!model) {
		model = a_object->As<RE::TESModel>();
	}
	if (model) {
		modelPath = model->GetModel();
	}

	if (auto parentCell = a_ref->GetParentCell()) {
		cellID = parentCell->GetFormID();
	}
	if (auto worldSpace = a_ref->GetWorldspace()) {
		worldSpaceID = worldSpace->GetFormID();
	}
	if (auto location = a_ref->GetCurrentLocation()) {
		locationID = location->GetFormID();
	}
}

bool ObjectREFRParams::IsValid() const
{
	return !ref->IsDisabled() && !ref->IsDeleted() && !modelPath.empty() && root && cellID != 0;
}

bool LightData::IsValid() const
{
	return light != nullptr;
}

std::string LightData::GetName(std::uint32_t a_index) const
{
	return std::format("{} [{:X}|{}|{}] #{}", LP_ID, light->GetFormID(), radius, fade, a_index);
}

std::string LightData::GetNodeName(const RE::NiPoint3& a_point, std::uint32_t a_index)
{
	return std::format("{} [{}{}{}] #{}", LP_NODE, a_point.x, a_point.y, a_point.z, a_index);
}

std::string LightData::GetNodeName(RE::NiAVObject* a_obj, std::uint32_t a_index)
{
	return std::format("{} {}{}", a_obj->name.c_str(), LP_NODE, a_index);
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
	const auto get_marker = [this] {
		if (GetCastsShadows()) {
			if (light->data.flags.any(RE::TES_LIGHT_FLAGS::kSpotShadow)) {
				return std::make_tuple("marker_spotlight.nif", 1.0f, true);
			}
			return std::make_tuple("marker_lightshadow.nif", 0.3f, false);
		}
		return std::make_tuple("marker_light.nif", 0.3f, false);
	};

	RE::NiPointer<RE::NiNode>                   loadedModel;
	constexpr RE::BSModelDB::DBTraits::ArgsType args{};

	auto [model, scale, flip] = get_marker();
	if (const auto error = Demand(model, loadedModel, args); error == RE::BSResource::ErrorCode::kNone) {
		if (auto clonedModel = loadedModel->Clone()) {
			clonedModel->name = "LP_DebugMarker";
			clonedModel->local.scale = scale;
			if (flip) {
				clonedModel->local.rotate.SetEulerAnglesXYZ(RE::deg_to_rad(-180), 0, RE::deg_to_rad(-180));
			}
			RE::AttachNode(a_node, clonedModel);
		}
	}
}

bool LightData::GetCastsShadows() const
{
	return flags.any(LightFlags::Shadow) /*|| light->data.flags.any(RE::TES_LIGHT_FLAGS::kOmniShadow, RE::TES_LIGHT_FLAGS::kHemiShadow, RE::TES_LIGHT_FLAGS::kSpotShadow)*/;
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
	return flags.any(LightFlags::PortalStrict) || light->data.flags.any(RE::TES_LIGHT_FLAGS::kPortalStrict);
}

RE::NiNode* LightData::GetOrCreateNode(RE::NiNode* a_root, const std::string& a_nodeName, std::uint32_t a_index) const
{
	auto obj = a_root->GetObjectByName(a_nodeName);
	return obj ? GetOrCreateNode(a_root, obj, a_index) : nullptr;
}

RE::NiNode* LightData::GetOrCreateNode(RE::NiNode* a_root, RE::NiAVObject* a_obj, std::uint32_t a_index) const
{
	const auto name = GetNodeName(a_obj, a_index);
	if (auto node = a_root->GetObjectByName(name)) {
		return node->AsNode();
	}
	auto geometry = a_obj->AsGeometry();
	if (auto newNode = RE::NiNode::Create(0); newNode) {
		newNode->name = name;
		if (geometry) {
			newNode->local.translate = geometry->modelBound.center;
		}
		newNode->local.translate += offset;
		newNode->local.rotate = rotation;
		RE::AttachNode(geometry ? a_root : a_obj->AsNode(), newNode);
		return newNode;
	}
	return nullptr;
}

std::pair<RE::BSLight*, RE::NiPointLight*> LightData::GenLight(RE::TESObjectREFR* a_ref, RE::NiNode* a_node, std::uint32_t a_index) const
{
	RE::BSLight*      bsLight = nullptr;
	RE::NiPointLight* niLight = nullptr;

	auto name = GetName(a_index);

	niLight = netimmerse_cast<RE::NiPointLight*>(a_node->GetObjectByName(name));
	if (!niLight) {
		niLight = RE::NiPointLight::Create();
		niLight->name = name;
		RE::AttachNode(a_node, niLight);

		//AttachDebugMarker(a_node);
	}

	if (niLight) {
		niLight->ambient = RE::NiColor();
		niLight->ambient.red = static_cast<float>(flags.underlying());

		niLight->diffuse = GetDiffuse();

		const auto lightRadius = GetRadius();
		niLight->radius.x = lightRadius;
		niLight->radius.y = lightRadius;
		niLight->radius.z = lightRadius;

		auto* shadowSceneNode = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
		if (bsLight = shadowSceneNode->GetPointLight(niLight); !bsLight) {
			bsLight = shadowSceneNode->AddLight(niLight, GetParams(a_ref));
		}

		niLight->SetLightAttenuation(lightRadius);
		niLight->fade = GetFade();

		if (conditions && !conditions->IsTrue(a_ref, a_ref)) {
			niLight->SetAppCulled(true);
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
				data.flags.set(LightData::LightFlags::PortalStrict);
				break;
			case "Shadow"_h:
				data.flags.set(LightData::LightFlags::Shadow);
				break;
			case "Simple"_h:
				data.flags.set(LightData::LightFlags::Simple);
				break;
			case "NoExternalEmittance"_h:
				data.flags.set(LightData::LightFlags::NoExternalEmittance);
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
	data.light = RE::TESForm::LookupByEditorID<RE::TESObjectLIGH>(lightEDID);

	if (!data.IsValid()) {
		return false;
	}

	data.emittanceForm = RE::TESForm::LookupByEditorID(emittanceFormEDID);

	ReadFlags();
	ReadConditions();

	return true;
}

void REFR_LIGH::ReattachLight(RE::TESObjectREFR* a_ref)
{
	auto lights = data.GenLight(a_ref, niLight->parent, index);

	bsLight.reset(lights.first);
	niLight.reset(lights.second);
}

void REFR_LIGH::UpdateAnimation()
{
	if (niLight) {
		if (colorController) {
			niLight->diffuse = colorController->GetValue(RE::BSTimer::GetSingleton()->delta);
		}
		if (radiusController) {
			auto newRadius = radiusController->GetValue(RE::BSTimer::GetSingleton()->delta);
			niLight->radius.x = newRadius;
			niLight->radius.y = newRadius;
			niLight->radius.z = newRadius;
		}
		if (fadeController) {
			niLight->fade = fadeController->GetValue(RE::BSTimer::GetSingleton()->delta);
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
}

void REFR_LIGH::UpdateConditions(RE::TESObjectREFR* a_ref) const
{
	if (data.conditions && niLight) {
		niLight->SetAppCulled(!data.conditions->IsTrue(a_ref, a_ref));
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

			niLight->fade = ((halfIntensityAmplitude * flickerIntensity) + (1.0f - halfIntensityAmplitude)) * data.fade;
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
			niLight->fade = ((constAttenCosine * halfIntensityAmplitude) + (1.0f - halfIntensityAmplitude)) * data.fade;
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

void REFR_LIGH::ReattachLight() const
{
	if (bsLight) {
		RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0]->AddLight(bsLight.get());
	}
}

void REFR_LIGH::RemoveLight() const
{
	if (bsLight) {
		RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0]->RemoveLight(bsLight);
	}
}

void ProcessedREFRLights::emplace(const REFR_LIGH& a_lightData, RE::RefHandle a_handle)
{
	if (a_lightData.colorController || a_lightData.fadeController || a_lightData.radiusController || !a_lightData.data.light->GetNoFlicker() || a_lightData.data.conditions) {
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
