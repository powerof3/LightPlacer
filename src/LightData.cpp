#include "LightData.h"
#include "ConditionParser.h"
#include "Settings.h"
#include "SourceData.h"

const RE::NiPointer<RE::NiPointLight>& LightOutput::GetLight() const
{
	return niLight;
}

bool LightOutput::DimLight(const float a_dimmer) const
{
	if (a_dimmer <= 1.0f) {
		niLight->fade *= a_dimmer;
		return true;
	}

	return false;
}

void LightOutput::ReattachLight() const
{
	if (bsLight) {
		RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0]->AddLight(bsLight.get());
	}

	if (Settings::GetSingleton()->CanShowDebugMarkers()) {
		ShowDebugMarker();
	}
}

void LightOutput::RemoveLight(bool a_clearData) const
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

void LightOutput::ShowDebugMarker() const
{
	if (debugMarker) {
		debugMarker->SetAppCulled(false);
	}
}

void LightOutput::HideDebugMarker() const
{
	if (debugMarker) {
		debugMarker->SetAppCulled(true);
	}
}

void LightOutput::UpdateDebugMarkerState(bool a_culled) const
{
	constexpr auto COLOR_RED = RE::NiColorA(1.0f, 0.0f, 0.0f, 1.0f);
	constexpr auto COLOR_GREY = RE::NiColorA(0.682f, 0.682f, 0.682f, 1.0f);

	if (debugMarker) {
		const auto obj = debugMarker->GetObjectByName("MarkerGeo");
		const auto shape = obj ? obj->AsGeometry() : nullptr;

		if (!shape) {
			return;
		}

		const auto effectProp = netimmerse_cast<RE::BSEffectShaderProperty*>(shape->properties[RE::BSGeometry::States::kEffect].get());
		const auto effectMaterial = effectProp ? static_cast<RE::BSEffectShaderMaterial*>(effectProp->material) : nullptr;

		if (effectMaterial) {
			effectMaterial->baseColor = a_culled ? COLOR_RED : COLOR_GREY;
		}
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

std::string LightData::GetLightName(const std::unique_ptr<SourceAttachData>& a_srcData, std::string_view a_lightEDID, std::uint32_t a_index)
{
	if (a_srcData->effectID != std::numeric_limits<std::uint32_t>::max()) {
		return std::format("{}[{}|{}]#{}", LP_LIGHT, a_srcData->effectID, a_lightEDID, a_index);
	}

	return std::format("{}[{}]#{}", LP_LIGHT, a_lightEDID, a_index);
}

std::string LightData::GetNodeName(const RE::NiPoint3& a_point, std::uint32_t a_index)
{
	return std::format("{}[{},{},{}]#{}", LP_NODE, a_point.x, a_point.y, a_point.z, a_index);
}

std::string LightData::GetNodeName(RE::NiAVObject* a_obj, std::uint32_t a_index)
{
	return std::format("{}[{}]#{}", LP_NODE, a_obj->name.c_str(), a_index);
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
		if (const auto clonedModel = loadedModel->Clone()) {
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
	return lightFlags.get();
}

bool LightData::GetInverseSquare() const
{
	return flags.any(LIGHT_FLAGS::InverseSquare) || light->data.flags.any(static_cast<RE::TES_LIGHT_FLAGS>(TES_LIGHT_FLAGS_EXT::kInverseSquare));
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

LightOutput LightData::GenLight(RE::TESObjectREFR* a_ref, RE::NiNode* a_node, std::string_view a_lightName, float a_scale) const
{
	RE::BSLight*      bsLight = nullptr;
	RE::NiPointLight* niLight = nullptr;
	RE::NiAVObject*   debugMarker = nullptr;

	if (!a_node) {
		return { bsLight, niLight, debugMarker };
	}

	const auto debugMarkerName = GetDebugMarkerName(a_lightName);

	niLight = netimmerse_cast<RE::NiPointLight*>(a_node->GetObjectByName(a_lightName));
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
			debugMarker = a_node->GetObjectByName(debugMarkerName);
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

	if (const auto shape = a_obj->GetObjectByName(a_params.shapeName); shape && shape->AsGeometry()) {
		shape->name = "MarkerGeo"sv;

		// make material unique so each bulb can turn red independently
		if (const auto effectProp = netimmerse_cast<RE::BSEffectShaderProperty*>(shape->AsGeometry()->properties[RE::BSGeometry::States::kEffect].get())) {
			effectProp->SetFlags(RE::BSShaderProperty::EShaderPropertyFlag8::kVertexColors, false);

			if (const auto effectMaterial = static_cast<RE::BSEffectShaderMaterial*>(effectProp->material)) {
				if (const auto newMaterial = static_cast<RE::BSEffectShaderMaterial*>(effectMaterial->Create())) {
					newMaterial->CopyMembers(effectMaterial);

					effectProp->lastRenderPassState = std::numeric_limits<std::int32_t>::max();
					effectProp->SetMaterial(newMaterial, true);
					effectProp->SetupGeometry(shape->AsGeometry());
					effectProp->FinishSetupGeometry(shape->AsGeometry());

					newMaterial->~BSEffectShaderMaterial();
					RE::free(newMaterial);
				}
			}
		}
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

void LIGH::LightSourceData::ReadConditions()
{
	if (!conditions.empty()) {
		ConditionParser::BuildCondition(data.conditions, conditions);
	}
}

bool LIGH::LightSourceData::PostProcess()
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

	ReadConditions();

	return true;
}

bool LIGH::LightSourceData::IsStaticLight() const
{
	return data.offset == RE::NiPoint3::Zero() && data.rotation == RE::MATRIX_ZERO && positionController.empty() && rotationController.empty();
}

RE::NiNode* LIGH::LightSourceData::GetOrCreateNode(RE::NiNode* a_root, const RE::NiPoint3& a_point, std::uint32_t a_index) const
{
	if (!a_root) {
		return nullptr;
	}

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

	return node ? node->AsNode() : nullptr;
}

RE::NiNode* LIGH::LightSourceData::GetOrCreateNode(RE::NiNode* a_root, const std::string& a_nodeName, std::uint32_t a_index) const
{
	if (!a_root) {
		return nullptr;
	}

	const auto obj = a_root->GetObjectByName(a_nodeName);
	return GetOrCreateNode(a_root, obj, a_index);
}

RE::NiNode* LIGH::LightSourceData::GetOrCreateNode(RE::NiNode* a_root, RE::NiAVObject* a_obj, std::uint32_t a_index) const
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
		if (geometry->local == RE::NiTransform{}) {
			return geometry->parent;
		}
	}

	const auto name = LightData::GetNodeName(a_obj, a_index);
	if (const auto node = a_root->GetObjectByName(name)) {
		return node->AsNode();
	}

	RE::NiNode* newNode = nullptr;

	if (newNode = RE::NiNode::Create(1); newNode) {
		newNode->name = name;
		if (geometry) {
			newNode->local.translate = geometry->modelBound.center;
		}
		newNode->local.translate += data.offset;
		newNode->local.rotate = data.rotation;
		RE::AttachNode(geometry ? a_root :
								  a_obj->AsNode(),
			newNode);
	}

	return newNode;
}

void REFR_LIGH::NodeVisHelper::InsertConditionalNodes(const StringSet& a_nodes, bool a_isVisble)
{
	for (const auto& nodeName : a_nodes) {
		conditionalNodes.insert_or_assign(nodeName, a_isVisble);
	}
}

void REFR_LIGH::NodeVisHelper::UpdateNodeVisibility(const RE::TESObjectREFR* a_ref, std::string_view a_nodeName)
{
	if (canCullAddonNodes || canCullNodes) {
		RE::NiAVObject* node = nullptr;
		if (a_nodeName.empty()) {
			node = a_ref->Get3D();
		} else {
			node = a_ref->Get3D()->GetObjectByName(a_nodeName);
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

void REFR_LIGH::NodeVisHelper::Reset()
{
	isVisible = false;
	canCullAddonNodes = false;
	canCullNodes = false;
}

REFR_LIGH::REFR_LIGH(const LIGH::LightSourceData& a_lightSource, const LightOutput& a_lightOutput, const RE::TESObjectREFRPtr& a_ref, float a_scale) :
	data(a_lightSource.data),
	output(a_lightOutput),
	lightControllers(a_lightSource),
	scale(a_scale)
{
	if (!data.emittanceForm && data.flags.none(LIGHT_FLAGS::NoExternalEmittance)) {
		auto xData = a_ref->extraList.GetByType<RE::ExtraEmittanceSource>();
		data.emittanceForm = xData ? xData->source : nullptr;
	}
}

void REFR_LIGH::ReattachLight(RE::TESObjectREFR* a_ref)
{
	const auto& niLight = output.GetLight();

	if (!niLight || !niLight->parent) {
		return;
	}

	output = data.GenLight(a_ref, niLight->parent, niLight->name, scale);

	if (Settings::GetSingleton()->CanShowDebugMarkers()) {
		output.ShowDebugMarker();
	}
}

bool REFR_LIGH::ShouldUpdateConditions(const ConditionUpdateFlags a_flags) const
{
	if (!data.conditions || a_flags == ConditionUpdateFlags::Skip) {
		return false;
	}

	auto& niLight = output.GetLight();

	const REX::EnumSet<LIGHT_CULL_FLAGS, std::uint8_t> cullFlags{ LightData::GetCulledFlag(niLight.get()) };

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

void REFR_LIGH::UpdateAnimation(float a_delta, float a_scalingFactor)
{
	scale = data.flags.any(LIGHT_FLAGS::IgnoreScale) ? 1.0f : a_scalingFactor;
	lightControllers.UpdateAnimation(output.GetLight(), a_delta, scale);
}

void REFR_LIGH::UpdateConditions(RE::TESObjectREFR* a_ref, NodeVisHelper& a_nodeVisHelper, ConditionUpdateFlags a_flags)
{
	if (!ShouldUpdateConditions(a_flags)) {
		return;
	}

	if (a_flags != ConditionUpdateFlags::Normal) {
		lastVisibleState = std::nullopt;
	}

	const bool isVisible = data.conditions->IsTrue(a_ref, a_ref);
	if (lastVisibleState != isVisible) {
		lastVisibleState = isVisible;

		auto& niLight = output.GetLight();
		auto& debugMarker = output.debugMarker;

		LightData::CullLight(niLight.get(), debugMarker.get(), !isVisible, LIGHT_CULL_FLAGS::Conditions);

		a_nodeVisHelper.isVisible |= isVisible;
		a_nodeVisHelper.canCullAddonNodes |= data.flags.any(LIGHT_FLAGS::SyncAddonNodes);
		a_nodeVisHelper.canCullNodes |= !data.conditionalNodes.empty();

		if (!data.conditionalNodes.empty()) {
			a_nodeVisHelper.InsertConditionalNodes(data.conditionalNodes, isVisible);
		}
	}
}

void REFR_LIGH::UpdateEmittance() const
{
	auto& niLight = output.GetLight();

	if (niLight && data.emittanceForm) {
		auto emittanceColor = RE::COLOR_WHITE;
		if (const auto lightForm = data.emittanceForm->As<RE::TESObjectLIGH>()) {
			emittanceColor = lightForm->emittanceColor;
		} else if (const auto region = data.emittanceForm->As<RE::TESRegion>()) {
			emittanceColor = region->emittanceColor;
		}
		niLight->diffuse = data.GetDiffuse() * emittanceColor;
	}
}

void REFR_LIGH::UpdateVanillaFlickering() const
{
	auto& niLight = output.GetLight();

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

		if (!lightControllers.positionController) {
			const auto constAttenSine = RE::NiSinQ(constAttenOffset + 1.7f);
			const auto linearAttenSine = RE::NiSinQ(linearAttenOffset + 0.5f);

			auto flickerMovementMult = ((data.light->data.flickerMovementAmplitude * constAttenSine) * linearAttenSine) * 0.5f;
			if ((flickerMovementMult + data.light->data.flickerMovementAmplitude) <= 0.0f) {
				flickerMovementMult = 0.0f;
			}

			niLight->local.translate.x = flickerMovementMult * constAttenSine;
			niLight->local.translate.y = flickerMovementMult * linearAttenSine;
			niLight->local.translate.z = flickerMovementMult * RE::NiSinQ(quadraticAttenOffset + 0.3f);

			RE::UpdateNode(niLight.get());
		}

		if (!lightControllers.fadeController) {
			const auto halfIntensityAmplitude = data.light->data.flickerIntensityAmplitude * 0.5f;

			const auto flickerIntensity = std::clamp((RE::NiSinQImpl(linearAttenOffset * 1.3f * (512.0f / RE::NI_TWO_PI) + 52.966763f) + 1.0f) * 0.5f *
															 (RE::NiSinQImpl(constAttenOffset * 1.1f * (512.0f / RE::NI_TWO_PI) + 152.38132f) + 1.0f) * 0.5f * 0.33333331f +
														 RE::NiSinQImpl(quadraticAttenOffset * 3.0f * (512.0f / RE::NI_TWO_PI) + 73.3386f) * 0.2f,
				-1.0f, 1.0f);

			niLight->fade = ((halfIntensityAmplitude * flickerIntensity) + (1.0f - halfIntensityAmplitude)) * data.GetFade();
		}

	} else {
		if (data.light->data.flags.none(RE::TES_LIGHT_FLAGS::kPulse, RE::TES_LIGHT_FLAGS::kPulseSlow)) {
			return;
		}

		auto constAttenuation = std::fmod(niLight->constAttenuation + (RE::BSTimer::GetSingleton()->delta * data.light->data.flickerPeriodRecip), RE::NI_TWO_PI);
		niLight->constAttenuation = constAttenuation;

		auto constAttenCosine = RE::NiCosQ(constAttenuation);
		auto constAttenSine = RE::NiSinQ(constAttenuation);

		if (!lightControllers.fadeController) {
			const auto halfIntensityAmplitude = data.light->data.flickerIntensityAmplitude * 0.5f;
			niLight->fade = ((constAttenCosine * halfIntensityAmplitude) + (1.0f - halfIntensityAmplitude)) * data.GetFade();
		}

		if (!lightControllers.positionController) {
			const auto movementAmplitude = data.light->data.flickerMovementAmplitude;

			niLight->local.translate.x = movementAmplitude * constAttenCosine;
			niLight->local.translate.y = movementAmplitude * constAttenSine;
			niLight->local.translate.z = movementAmplitude * (constAttenSine * constAttenCosine);

			RE::UpdateNode(niLight.get());
		}
	}
}
