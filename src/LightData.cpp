#include "LightData.h"
#include "ConditionParser.h"
#include "Settings.h"
#include "SourceData.h"

const RE::NiPointer<RE::NiPointLight>& LightInstance::GetLight() const
{
	return niLight;
}

bool LightInstance::DimLight(const float a_dimmer) const
{
	if (a_dimmer < 1.0f) {
		niLight->fade *= a_dimmer;
		return true;
	}

	return false;
}

void LightInstance::ReattachLight() const
{
	if (bsLight) {
		RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0]->AddLight(bsLight.get());
	}

	if (Settings::GetSingleton()->CanShowDebugMarkers()) {
		ShowDebugMarker();
	}
}

void LightInstance::RemoveLight(bool a_clearData) const
{
	if (Settings::GetSingleton()->CanShowDebugMarkers()) {
		HideDebugMarker();
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

void LightInstance::ShowDebugMarker() const
{
	if (debugMarker) {
		debugMarker->SetAppCulled(false);
	}
}

void LightInstance::HideDebugMarker() const
{
	if (debugMarker) {
		debugMarker->SetAppCulled(true);
	}
}

bool LightData::IsValid() const
{
	return light != nullptr;
}

std::string LightData::GetDebugMarkerName(std::string_view a_lightName)
{
	return std::format("{}[{}]", LP_DEBUG, a_lightName);
}

std::string LightData::GetNodeName(const RE::NiPoint3& a_point, const std::string& path, std::uint32_t a_index) const
{
	return std::format("{}[{}|{},{},{}]#{}", LP_NODE, path, a_point.x + offset.x, a_point.y + offset.y, a_point.z + offset.z, a_index);
}

std::string LightData::GetNodeName(RE::NiAVObject* a_obj, const std::string& path, std::uint32_t a_index) const
{
	const auto& pos = a_obj->local.translate;
	return std::format("{}[{}|{}({},{},{})]#{}", LP_NODE, path, a_obj->name.c_str(), pos.x + offset.x, pos.y + offset.y, pos.z + offset.z, a_index);
}

bool LightData::IsDynamicLight(const RE::TESObjectREFR* a_ref) const
{
	if (light->data.flags.any(RE::TES_LIGHT_FLAGS::kDynamic) || GetCastsShadows()) {
		return true;
	}

	if (a_ref) {
		if (a_ref->IsActor()) {
			return true;
		}
		if (const auto baseObject = a_ref->GetBaseObject(); baseObject && baseObject->IsInventoryObject()) {
			return true;
		}
	}

	return false;
}

RE::NiAVObject* LightData::AttachDebugMarker(RE::NiNode* a_node, std::string_view a_debugMarkerName) const
{
	if (!Settings::GetSingleton()->LoadDebugMarkers()) {
		return nullptr;
	}

	RE::NiNodePtr                               loadedModel;
	constexpr RE::BSModelDB::DBTraits::ArgsType args{};

	const auto create_params = GetDebugMarkerParams();

	if (const auto error = Demand(create_params.modelName, loadedModel, args); error == RE::BSResource::ErrorCode::kNone) {
		if (const auto clonedModel = netimmerse_cast<RE::NiAVObject*>(loadedModel->Clone())) {
			loadedModel.reset();
			PostProcessDebugMarker(clonedModel, create_params, a_debugMarkerName);
			RE::AttachNode(a_node, clonedModel);
			return clonedModel;
		}
	}

	return nullptr;
}

bool LightData::GetCastsShadows() const
{
	return flags.any(LIGHT_FLAGS::Shadow) /*|| light->data.flags.any(RE::TES_LIGHT_FLAGS::kOmniShadow, RE::TES_LIGHT_FLAGS::kHemiShadow, RE::TES_LIGHT_FLAGS::kSpotShadow)*/;
}

RE::NiColor LightData::GetDiffuse() const
{
	auto diffuse = (color == RE::COLOR_BLACK) ? RE::NiColor(light->data.color) : color;
	return light->data.flags.any(RE::TES_LIGHT_FLAGS::kNegative) ? -diffuse : diffuse;
}

float LightData::GetRadius() const
{
	return (radius > 0.0f ? radius : static_cast<float>(light->data.radius)) * Settings::GetSingleton()->GetGlobalLightRadius();
}

float LightData::GetFade() const
{
	return (fade > 0.0f ? fade : light->fade) * Settings::GetSingleton()->GetGlobalLightFade();
}

float LightData::GetScaledValue(float a_value, float a_scale) const
{
	return flags.any(LIGHT_FLAGS::IgnoreScale) ?
	           a_value :
	           a_value * a_scale;
}

float LightData::GetScaledRadius(float a_scale) const
{
	return GetScaledValue(GetRadius(), a_scale);
}

float LightData::GetScaledFade(float a_scale) const
{
	return GetScaledValue(GetFade(), a_scale);
}

float LightData::GetFOV() const
{
	if (!GetCastsShadows()) {
		return 1.0;
	}
	if (light->data.flags.any(RE::TES_LIGHT_FLAGS::kHemiShadow)) {
		return RE::NI_PI;
	}
	if (light->data.flags.any(RE::TES_LIGHT_FLAGS::kSpotShadow)) {
		return RE::deg_to_rad(fov > 0.0f ? fov : light->data.fov);
	}
	return RE::NI_TWO_PI;
}

LIGHT_FLAGS LightData::GetLightFlags() const
{
	auto lightFlags = LIGHT_FLAGS::Initialised | flags;
	if (GetInverseSquare()) {
		lightFlags |= LIGHT_FLAGS::InverseSquare;
	}
	if (GetLinear()) {
		lightFlags |= LIGHT_FLAGS::Linear;
	}
	return lightFlags.get();
}

bool LightData::GetInverseSquare() const
{
	return flags.any(LIGHT_FLAGS::InverseSquare) || light->data.flags.any(static_cast<RE::TES_LIGHT_FLAGS>(TES_LIGHT_FLAGS_EXT::kInverseSquare));
}

bool LightData::GetLinear() const
{
	return flags.any(LIGHT_FLAGS::Linear) || light->data.flags.any(static_cast<RE::TES_LIGHT_FLAGS>(TES_LIGHT_FLAGS_EXT::kLinear));
}

float LightData::GetCutoff() const
{
	const float lightCutoff = cutoff > 0.0f ? cutoff : light->data.fallofExponent;
	return std::clamp(lightCutoff, 0.01f, 1.0f);
}

float LightData::GetSize() const
{
	float lightSize = size > 0.0f ? size : light->data.fov;
	lightSize = lightSize >= 50.0f ? 1.414f : lightSize;
	return std::clamp(lightSize, 0.01f, 50.0f);
}

float LightData::GetScaledSize(float a_scale) const
{
	return GetScaledValue(GetSize(), a_scale);
}

float LightData::GetFalloff() const
{
	return GetCastsShadows() ? light->data.fallofExponent : 1.0f;
}

float LightData::GetNearDistance() const
{
	return GetCastsShadows() ? light->data.nearDistance : 5.0f;
}

RE::ShadowSceneNode::LIGHT_CREATE_PARAMS LightData::GetParams(const RE::TESObjectREFR* a_ref) const
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
	return flags.any(LIGHT_FLAGS::PortalStrict) || light->data.flags.any(RE::TES_LIGHT_FLAGS::kPortalStrict);
}

LightInstance LightData::GenLight(RE::TESObjectREFR* a_ref, RE::NiNode* a_node, std::string_view a_lightName, float a_scale) const
{
	RE::BSLight*      bsLight = nullptr;
	RE::NiPointLight* niLight = nullptr;
	RE::NiAVObject*   debugMarker = nullptr;

	if (!a_node) {
		return { bsLight, niLight, debugMarker };
	}

	const auto debugMarkerName = GetDebugMarkerName(a_lightName);

	niLight = netimmerse_cast<RE::NiPointLight*>(RE::GetChildByName(a_node, a_lightName));
	if (!niLight) {
		niLight = RE::NiPointLight::Create();
		niLight->name = a_lightName;
		RE::AttachNode(a_node, niLight);
		debugMarker = AttachDebugMarker(a_node, debugMarkerName);
	}

	if (niLight) {
		niLight->ambient = RE::NiColor();
		niLight->ambient.red = std::bit_cast<float>(GetLightFlags());
		niLight->ambient.green = GetCutoff();
		niLight->ambient.blue = std::bit_cast<float>(light->formID);

		niLight->diffuse = GetDiffuse();

		const auto lightRadius = GetScaledRadius(a_scale);
		niLight->radius.x = lightRadius;
		niLight->radius.y = lightRadius;
		niLight->radius.z = GetScaledSize(a_scale);

		niLight->SetLightAttenuation(lightRadius);
		niLight->fade = GetScaledFade(a_scale);

		auto* shadowSceneNode = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
		if (bsLight = shadowSceneNode->GetPointLight(niLight); !bsLight) {
			bsLight = shadowSceneNode->AddLight(niLight, GetParams(a_ref));
		}

		if (!debugMarker) {
			debugMarker = RE::GetObjectByName(a_node, debugMarkerName);
		}

		// immediately update state on attach. waiting for cell update is too slow
		if (conditions && !conditions->IsTrue(a_ref, a_ref)) {
			CullLight(niLight, debugMarker, true, LIGHT_CULL_FLAGS::Conditions);
		}
	}

	return { bsLight, niLight, debugMarker };
};

LIGHT_CULL_FLAGS LightData::GetCulledFlag(RE::NiPointLight* a_light)
{
	return static_cast<LIGHT_CULL_FLAGS>(std::bit_cast<uint32_t>(a_light->ambient.red) >> 24);
}

void LightData::CullLight(RE::NiPointLight* a_light, RE::NiAVObject* a_debugMarker, bool a_hide, LIGHT_CULL_FLAGS a_flags)
{
	a_light->SetAppCulled(a_hide);

	std::uint32_t bits = std::bit_cast<std::uint32_t>(a_light->ambient.red);

	if (a_hide) {
		bits = (bits & 0x00FFFFFF) | (static_cast<std::uint32_t>(std::to_underlying(a_flags)) << 24);
	} else {
		bits &= ~(static_cast<uint32_t>(std::to_underlying(a_flags)) << 24);
	}

	a_light->ambient.red = std::bit_cast<float>(bits);

	if (Settings::GetSingleton()->CanShowDebugMarkers() && a_debugMarker) {
		a_debugMarker->SetAppCulled(a_hide);
	}
}

const char* LightData::GetCulledStatus(RE::NiPointLight* a_light)
{
	if (!a_light->GetAppCulled()) {
		return "visible";
	}

	const REX::EnumSet<LIGHT_CULL_FLAGS, std::uint8_t> flags(static_cast<LIGHT_CULL_FLAGS>(std::bit_cast<uint32_t>(a_light->ambient.red) >> 24));

	// script > game > conditions

	if (flags.any(LIGHT_CULL_FLAGS::Script)) {
		return "hidden [script]";
	}
	if (flags.any(LIGHT_CULL_FLAGS::Game)) {
		return "hidden [game]";
	}
	if (flags.any(LIGHT_CULL_FLAGS::Conditions)) {
		return "hidden [conditions]";
	}

	return "hidden";
}

void LightData::PostProcessDebugMarker(RE::NiAVObject* a_obj, const MARKER_CREATE_PARAMS& a_params, std::string_view a_debugMarkerName)
{
	if (!Settings::GetSingleton()->CanShowDebugMarkers()) {
		a_obj->SetAppCulled(true);
	}

	a_obj->name = a_debugMarkerName;
	a_obj->local.scale = a_params.scale;
	if (a_params.rotation != RE::NiPoint3::Zero()) {
		a_obj->local.rotate.SetEulerAnglesXYZ(a_params.rotation.x, a_params.rotation.y, a_params.rotation.z);
	}
}

LightData::MARKER_CREATE_PARAMS LightData::GetDebugMarkerParams() const
{
	if (GetCastsShadows()) {
		if (light->data.flags.any(RE::TES_LIGHT_FLAGS::kHemiShadow)) {
			return { "marker_halfomni.nif", "marker_halfomni:0", 0.25f, RE::NiPoint3(0, -1.5708f, 0) };
		}
		if (light->data.flags.any(RE::TES_LIGHT_FLAGS::kSpotShadow)) {
			return { "marker_spotlight.nif", "marker_spotlight:0", 1.0f, RE::NiPoint3(-RE::NI_PI, 0, -RE::NI_PI) };
		}
		return { "marker_lightshadow.nif", "marker_lightshadow:0", 0.25f, RE::NiPoint3() };
	}
	return { "marker_light.nif", "marker_light:0", 0.25f, RE::NiPoint3() };
}

void LIGH::LightDefinition::ReadConditions()
{
	if (!conditions.empty()) {
		ConditionParser::BuildCondition(data.conditions, conditions);
	}
}

bool LIGH::LightDefinition::PostProcess()
{
	if (!lightEDID.contains("|")) {
		data.light = RE::TESForm::LookupByEditorID<RE::TESObjectLIGH>(lightEDID);
	} else {
		auto edids = string::split(lightEDID, "|");
		for (const auto& edid : edids) {
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
	emittanceFormEDID = {};

	ReadConditions();
	conditions = {};

	return true;
}

bool LIGH::LightDefinition::IsStaticLight() const
{
	return data.offset == RE::NiPoint3::Zero() && data.rotation == RE::NiPoint3::Zero() && !positionController && !rotationController;
}

bool LIGH::LightDefinition::HasControllers() const
{
	return positionController || rotationController || colorController || radiusController || fadeController;
}

RE::NiNode* LIGH::LightDefinition::GetOrCreateNode(RE::NiNode* a_root, const RE::NiPoint3& a_point, const std::string& path, std::uint32_t a_index) const
{
	if (a_root) {
		if (a_point == RE::NiPoint3::Zero() && IsStaticLight()) {
			return a_root;
		}

		auto name = data.GetNodeName(a_point, path, a_index);

		auto node = RE::GetChildByName(a_root, name);
		if (!node) {
			if (node = RE::NiNode::Create(1); node) {
				node->name = name;
				node->local.translate = a_point + data.offset;
				if (data.rotation != RE::NiPoint3::Zero()) {
					node->local.rotate = RE::ToMatrix(data.rotation);
				}
				RE::AttachNode(a_root, node);
			}
		}

		return node ? node->AsNode() : nullptr;
	}

	return nullptr;
}

RE::NiNode* LIGH::LightDefinition::GetOrCreateNode(RE::NiNode* a_root, const std::string& a_nodeName, const std::string& path, std::uint32_t a_index) const
{
	if (!a_root) {
		return nullptr;
	}

	const auto obj = RE::GetObjectByName(a_root, a_nodeName);
	return GetOrCreateNode(a_root, obj, path, a_index);
}

RE::NiNode* LIGH::LightDefinition::GetOrCreateNode(RE::NiNode* a_root, RE::NiAVObject* a_obj, const std::string& path, std::uint32_t a_index) const
{
	if (!a_root || !a_obj) {
		return nullptr;
	}

	if (const auto node = a_obj->AsNode()) {
		if (IsStaticLight()) {
			return node;
		}
	}

	auto geometry = a_obj->AsGeometry();
	if (geometry && geometry->parent && !geometry->parent->AsFadeNode()) {  // not top level BSFadeNode
		if (geometry->local == RE::NiTransform{} && data.offset == RE::NiPoint3{}) {
			return geometry->parent;
		}
	}

	const auto name = data.GetNodeName(a_obj, path, a_index);
	if (const auto node = RE::GetObjectByName(a_root, name)) {
		return node->AsNode();
	}

	RE::NiNode* newNode = nullptr;

	if (newNode = RE::NiNode::Create(1); newNode) {
		newNode->name = name;

		const auto getGeometryAttachNode = [&](const RE::BSGeometry* geom) {
			for (auto* parent = geom->parent; parent; parent = parent->parent) {
				if (parent->AsSwitchNode()) {
					return geom->parent;
				}
			}
			return a_root;
		};

		auto attachNode = geometry ? getGeometryAttachNode(geometry) : a_obj->AsNode();
		if (geometry) {
			newNode->local.translate = attachNode == a_root ? geometry->modelBound.center : geometry->local.translate;
		}
		newNode->local.translate += data.offset;
		if (data.rotation != RE::NiPoint3::Zero()) {
			newNode->local.rotate = RE::ToMatrix(data.rotation);
		}

		RE::AttachNode(attachNode, newNode);
	}

	return newNode;
}

std::string LIGH::LightDefinition::GetLightName(const SourceAttachData& a_srcData, const std::string& path, std::uint32_t a_index) const
{
	if (a_srcData.miscID != std::numeric_limits<std::uint32_t>::max()) {
		return std::format("{}[{}|{}]({})#{}", LightData::LP_LIGHT, path, lightEDID, a_srcData.miscID, a_index);
	}

	return std::format("{}[{}|{}]#{}", LightData::LP_LIGHT, path, lightEDID, a_index);
}
