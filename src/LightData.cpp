#include "LightData.h"
#include "ConditionParser.h"
#include "LightData.h"

ObjectRefData::ObjectRefData(RE::TESObjectREFR* a_ref) :
	ObjectRefData(a_ref, a_ref->Get3D())
{}

ObjectRefData::ObjectRefData(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root) :
	ref(a_ref),
	root(a_root ? a_root->AsNode() : nullptr),
	handle(a_ref->CreateRefHandle().native_handle())
{}

bool ObjectRefData::IsValid() const
{
	if (ref->IsDisabled() || ref->IsDeleted() || !root || !ref->GetParentCell()) {
		return false;
	}
	return true;
}

LightData::LightData(const RE::NiStringsExtraData* a_data)
{
	std::vector<std::string> data(a_data->value, a_data->value + a_data->size);

	for (const auto& str : data) {
		std::istringstream strstream(str);
		std::string        key, value;

		if (std::getline(strstream, key, ':')) {
			std::getline(strstream, value);
			string::trim(value);

			switch (string::const_hash(key)) {
			case "light"_h:
				{
					lightEDID = value;
					light = RE::TESForm::LookupByEditorID<RE::TESObjectLIGH>(value);
				}
				break;
			case "radius"_h:
				radius = string::to_num<float>(value);
				break;
			case "fade"_h:
				fade = string::to_num<float>(value);
				break;
			case "offset"_h:
				{
					if (auto pt3 = string::split(value, ","sv); pt3.size() == 3) {
						offset.x = string::to_num<float>(pt3[0]);
						offset.y = string::to_num<float>(pt3[1]);
						offset.z = string::to_num<float>(pt3[2]);
					}
				}
				break;
			case "emittanceForm"_h:
				{
					emittanceFormEDID = value;
					emittanceForm = RE::TESForm::LookupByEditorID(value);
				}
				break;
			case "flags"_h:
				rawFlags = value;
				break;
			case "condition"_h:
				rawConditions.push_back(value);
				break;
			default:
				break;
			}
		}
	}

	if (IsValid()) {
		ReadFlags();
		ReadConditions();
	}
}

void LightData::ReadFlags()
{
	// glaze doesn't reflect flag enums
	if (!rawFlags.empty()) {
		auto flagStrs = string::split(rawFlags, "|");
		for (const auto& flagStr : flagStrs) {
			switch (string::const_hash(flagStr)) {
			case "PortalStrict"_h:
				flags.set(LightFlags::PortalStrict);
				break;
			case "Shadow"_h:
				flags.set(LightFlags::Shadow);
				break;
			case "Simple"_h:
				flags.set(LightFlags::Simple);
				break;
			default:
				break;
			}
		}
	}
}

void LightData::ReadConditions()
{
	if (!rawConditions.empty()) {
		ConditionParser::GetSingleton()->BuildCondition(conditions, rawConditions);
	}
}

bool LightData::PostProcess()
{
	light = RE::TESForm::LookupByEditorID<RE::TESObjectLIGH>(lightEDID);

	if (!IsValid()) {
		return false;
	}

	emittanceForm = RE::TESForm::LookupByEditorID(emittanceFormEDID);

	ReadFlags();
	ReadConditions();

	return true;
}

bool LightData::IsValid() const
{
	return light != nullptr;
}

std::string LightData::GetName(std::uint32_t a_index) const
{
	return std::format("{}{}|{}|{} #{}", LP_ID, lightEDID, radius, fade, a_index);
}

RE::NiColor LightData::GetDiffuse() const
{
	auto diffuse = RE::NiColor(light->data.color);
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

RE::ShadowSceneNode::LIGHT_CREATE_PARAMS LightData::GetParams(RE::TESObjectREFR* a_ref) const
{
	RE::ShadowSceneNode::LIGHT_CREATE_PARAMS params{};
	params.dynamic = light->data.flags.any(RE::TES_LIGHT_FLAGS::kDynamic) || (a_ref && a_ref->GetBaseObject() ? a_ref->GetBaseObject()->IsInventoryObject() : false);
	params.shadowLight = false;
	params.portalStrict = light->data.flags.any(RE::TES_LIGHT_FLAGS::kPortalStrict);
	params.affectLand = a_ref ? (a_ref->GetFormFlags() & RE::TESObjectREFR::RecordFlags::kDoesntLightLandscape) == 0 : true;
	params.affectWater = a_ref ? (a_ref->GetFormFlags() & RE::TESObjectREFR::RecordFlags::kDoesntLightWater) == 0 : true;
	params.neverFades = a_ref ? !a_ref->IsHeadingMarker() : true;
	params.fov = 1.0f;
	params.falloff = light->data.fallofExponent;
	params.nearDistance = light->data.nearDistance;
	params.depthBias = 1.0;
	params.sceneGraphIndex = 0;
	params.restrictedNode = nullptr;
	params.lensFlareData = light->lensFlare;
	return params;
}

RE::NiNode* LightData::GetOrCreateNode(RE::NiNode* a_root, const std::string& a_nodeName, std::uint32_t a_index) const
{
	auto obj = a_root->GetObjectByName(a_nodeName);
	return obj ? GetOrCreateNode(a_root, obj, a_index) : nullptr;
}

RE::NiNode* LightData::GetOrCreateNode(RE::NiNode* a_root, RE::NiAVObject* a_obj, std::uint32_t a_index) const
{
	if (auto node = a_obj->AsNode()) {
		return node;
	}

	if (auto geometry = a_obj->AsGeometry()) {
		auto name = std::format("{} {}{}", a_obj->name.c_str(), LP_NODE, a_index);
		if (auto node = a_root->GetObjectByName(name); node && node->AsNode()) {
			return node->AsNode();
		}
		if (auto newNode = RE::NiNode::Create(0); newNode) {
			newNode->name = name;
			newNode->local.translate = geometry->modelBound.center;
			RE::AttachNode(a_root, newNode);
			return newNode;
		}
	}

	return nullptr;
}

RE::NiPointLight* LightData::SpawnLight(RE::TESObjectREFR* a_ref, RE::NiNode* a_node, const RE::NiPoint3& a_point, std::uint32_t a_index) const
{
	auto name = GetName(a_index);

	auto niLight = netimmerse_cast<RE::NiPointLight*>(a_node->GetObjectByName(name));
	if (!niLight) {
		niLight = RE::NiPointLight::Create();
		RE::NiPoint3 point = a_point;
		if (point == RE::NiPoint3::Zero()) {
			point += offset;
		}
		niLight->local.translate = point;
		niLight->name = name;
		RE::AttachNode(a_node, niLight);
	}

	if (niLight) {
		niLight->ambient = RE::NiColor();
		niLight->ambient.red = static_cast<float>(flags.underlying());

		niLight->diffuse = GetDiffuse();

		auto lightRadius = GetRadius();
		niLight->radius.x = lightRadius;
		niLight->radius.y = lightRadius;
		niLight->radius.z = lightRadius;

		auto* shadowSceneNode = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
		if (!shadowSceneNode->GetPointLight(niLight)) {
			shadowSceneNode->AddLight(niLight, GetParams(a_ref));
		}

		niLight->SetLightAttenuation(lightRadius);
		niLight->fade = GetFade();
	}

	return niLight;
}

void LightREFRData::UpdateConditions(const RE::TESObjectREFRPtr& a_ref) const
{
	if (ptLight && conditions) {
		ptLight->SetAppCulled(!conditions->IsTrue(a_ref.get(), a_ref.get()));
	}
}

void LightREFRData::UpdateFlickeringGame(const RE::TESObjectREFRPtr& a_ref) const
{
	if (ptLight && light) {
		if (ptLight->GetAppCulled()) {
			return;
		}
		auto originalFade = light->fade;
		light->fade = fade;
		UpdateLight_Game(light, ptLight, a_ref.get(), -1.0f);
		light->fade = originalFade;
	}
}

void LightREFRData::UpdateFlickering(const RE::TESObjectREFRPtr& a_ref) const
{
	if (ptLight && light) {
		if (ptLight->GetAppCulled()) {
			return;
		}
		UpdateLight();
	}
}

void LightREFRData::UpdateLight_Game(RE::TESObjectLIGH* a_light, const RE::NiPointer<RE::NiPointLight>& a_ptLight, RE::TESObjectREFR* a_ref, float a_wantDimmer)
{
	using func_t = decltype(&UpdateLight_Game);
	static REL::Relocation<func_t> func{ RELOCATION_ID(17212, 17614) };
	return func(a_light, a_ptLight, a_ref, a_wantDimmer);
}

void LightREFRData::UpdateLight() const
{
	if (light->data.flags.any(RE::TES_LIGHT_FLAGS::kFlicker, RE::TES_LIGHT_FLAGS::kFlickerSlow)) {
		const auto flickerDelta = RE::BSTimer::GetSingleton()->delta * light->data.flickerPeriodRecip;

		auto constAttenOffset = ptLight->constAttenuation + (clib_util::RNG().generate<float>(1.1f, 13.1f) * flickerDelta);
		auto linearAttenOffset = ptLight->linearAttenuation + (clib_util::RNG().generate<float>(1.2f, 13.2f) * flickerDelta);
		auto quadraticAttenOffset = ptLight->quadraticAttenuation + (clib_util::RNG().generate<float>(1.3f, 19.3f) * flickerDelta);

		constAttenOffset = std::fmod(constAttenOffset, RE::NI_TWO_PI);
		linearAttenOffset = std::fmod(linearAttenOffset, RE::NI_TWO_PI);
		quadraticAttenOffset = std::fmod(quadraticAttenOffset, RE::NI_TWO_PI);

		ptLight->constAttenuation = constAttenOffset;
		ptLight->linearAttenuation = linearAttenOffset;
		ptLight->quadraticAttenuation = quadraticAttenOffset;

		const auto constAttenSine = RE::NiSinQ(constAttenOffset + 1.7f);
		const auto linearAttenSine = RE::NiSinQ(linearAttenOffset + 0.5f);

		auto flickerMovementMult = ((light->data.flickerMovementAmplitude * constAttenSine) * linearAttenSine) * 0.5f;
		if ((flickerMovementMult + light->data.flickerMovementAmplitude) <= 0.0f) {
			flickerMovementMult = 0.0f;
		}

		ptLight->local.translate.x = flickerMovementMult * constAttenSine;
		ptLight->local.translate.y = flickerMovementMult * linearAttenSine;
		ptLight->local.translate.z = flickerMovementMult * RE::NiSinQ(quadraticAttenOffset + 0.3f);

		const auto halfIntensityAmplitude = light->data.flickerIntensityAmplitude * 0.5f;

		const auto flickerIntensity = std::clamp((RE::NiSinQImpl(linearAttenOffset * 1.3f * (512.0f / RE::NI_TWO_PI) + 52.966763f) + 1.0f) * 0.5f *
														 (RE::NiSinQImpl(constAttenOffset * 1.1f * (512.0f / RE::NI_TWO_PI) + 152.38132f) + 1.0f) * 0.5f * 0.33333331f +
													 RE::NiSinQImpl(quadraticAttenOffset * 3.0f * (512.0f / RE::NI_TWO_PI) + 73.3386f) * 0.2f,
			-1.0f, 1.0f);

		ptLight->fade = ((halfIntensityAmplitude * flickerIntensity) + (1.0f - halfIntensityAmplitude)) * fade;

	} else {
		if (light->data.flags.none(RE::TES_LIGHT_FLAGS::kPulse, RE::TES_LIGHT_FLAGS::kPulseSlow)) {
			return;
		}

		auto constAttenuation = std::fmod(ptLight->constAttenuation + (RE::BSTimer::GetSingleton()->delta * light->data.flickerPeriodRecip), RE::NI_TWO_PI);
		ptLight->constAttenuation = constAttenuation;

		auto constAttenCosine = RE::NiCosQ(constAttenuation);
		auto constAttenSine = RE::NiSinQ(constAttenuation);

		const auto movementAmplitude = light->data.flickerMovementAmplitude;
		const auto halfIntensityAmplitude = light->data.flickerIntensityAmplitude * 0.5f;

		ptLight->fade = ((constAttenCosine * halfIntensityAmplitude) + (1.0f - halfIntensityAmplitude)) * fade;

		ptLight->local.translate.x = movementAmplitude * constAttenCosine;
		ptLight->local.translate.y = movementAmplitude * constAttenSine;
		ptLight->local.translate.z = movementAmplitude * (constAttenSine * constAttenCosine);
	}

	if (RE::TaskQueueInterface::ShouldUseTaskQueue()) {
		RE::TaskQueueInterface::GetSingleton()->QueueUpdateNiObject(ptLight.get());
	} else {
		RE::NiUpdateData data;
		ptLight->Update(data);
	}
}

void LightREFRData::UpdateEmittance() const
{
	if (ptLight && emittance) {
		RE::NiColor emittanceColor(1.0, 1.0, 1.0);
		if (auto lightForm = emittance->As<RE::TESObjectLIGH>()) {
			emittanceColor = lightForm->emittanceColor;
		} else if (auto region = emittance->As<RE::TESRegion>()) {
			emittanceColor = region->emittanceColor;
		}
		ptLight->diffuse = diffuse * emittanceColor;
	}
}

void AttachLightVecPostProcess(AttachLightDataVec& a_attachLightDataVec)
{
	std::erase_if(a_attachLightDataVec, [](auto& attachLightData) {
		bool failedPostProcess = false;
		std::visit(overload{
					   [&](PointData& pointData) {
						   failedPostProcess = !pointData.data.PostProcess();
					   },
					   [&](NodeData& nodeData) {
						   failedPostProcess = !nodeData.data.PostProcess();
					   },
					   [&](FilteredData& filteredData) {
						   failedPostProcess = !filteredData.data.PostProcess();
					   } },
			attachLightData);
		return failedPostProcess;
	});
}

bool FilteredData::IsInvalid(const std::string& a_model) const
{
	return (!blackList.empty() && blackList.contains(a_model)) || (!whiteList.empty() && !whiteList.contains(a_model));
}
