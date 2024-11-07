#include "LightData.h"

ObjectRefData::ObjectRefData(RE::TESObjectREFR* a_ref) :
	ObjectRefData(a_ref, a_ref->Get3D())
{}

ObjectRefData::ObjectRefData(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root) :
	ref(a_ref),
	root(a_root ? a_root->AsNode() : nullptr),
	cellFormID(a_ref->GetSaveParentCell() ? a_ref->GetSaveParentCell()->GetFormID() : 0),
	handle(a_ref->CreateRefHandle().native_handle())
{}

bool ObjectRefData::IsValid() const
{
	if (ref->IsDisabled() || ref->IsDeleted() || !root || cellFormID == 0) {
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
			case "chance"_h:
				chance = string::to_num<float>(value);
				break;
			default:
				break;
			}
		}
	}
}

void LightData::LoadFormsFromConfig()
{
	light = RE::TESForm::LookupByEditorID<RE::TESObjectLIGH>(lightEDID);
	emittanceForm = RE::TESForm::LookupByEditorID(emittanceFormEDID);
}

bool LightData::IsValid() const
{
	return light != nullptr;
}

std::string LightData::GetName(std::uint32_t a_index) const
{
	return std::format("{}{} PtLight #{}", LP_ID, lightEDID, a_index);
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
	params.dynamic = light->data.flags.any(RE::TES_LIGHT_FLAGS::kDynamic);
	params.shadowLight = false;
	params.portalStrict = light->data.flags.any(RE::TES_LIGHT_FLAGS::kPortalStrict);
	params.affectLand = a_ref ? (a_ref->GetFormFlags() & RE::TESObjectREFR::RecordFlags::kDoesntLightLandscape) == 0 : true;
	params.affectWater = a_ref ? (a_ref->GetFormFlags() & RE::TESObjectREFR::RecordFlags::kDoesntLightWater) == 0 : true;
	params.neverFades = a_ref ? !a_ref->IsHeadingMarker() : true;
	params.fov = 1.0f;
	params.falloff = light->data.fallofExponent;
	params.nearDistance = light->data.nearDistance;
	params.depthBias = 0;
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

std::tuple<RE::NiPointLight*, bool, RE::TESForm*> LightData::SpawnLight(RE::TESObjectREFR* a_ref, RE::NiNode* a_node, const RE::NiPoint3& a_point, std::uint32_t a_index) const
{
	if (chance < 100.0f) {
		if (const auto rngValue = clib_util::RNG().generate<float>(0.0f, 100.0f); rngValue > chance) {
			return { nullptr, false, nullptr };
		}
	}

	auto niLight = RE::NiPointLight::Create();
	if (!niLight) {
		return { nullptr, false, nullptr };
	}

	RE::NiPoint3 point = a_point;
	if (point == RE::NiPoint3::Zero()) {
		point += offset;
	}
	niLight->local.translate = point;
	RE::AttachNode(a_node, niLight);

	niLight->name = GetName(a_index);

	niLight->ambient = RE::NiColor();
	niLight->diffuse = GetDiffuse();

	auto lightRadius = GetRadius();
	niLight->radius.x = lightRadius;
	niLight->radius.y = lightRadius;
	niLight->radius.z = lightRadius;

	RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0]->AddLight(niLight, GetParams(a_ref));

	niLight->SetLightAttenuation(lightRadius);
	niLight->fade = GetFade();

	RE::TESForm* emittanceSrc = emittanceForm;
	if (!emittanceSrc) {
		auto xData = a_ref->extraList.GetByType<RE::ExtraEmittanceSource>();
		emittanceSrc = xData ? xData->source : nullptr;
	}

	return { niLight, !light->GetNoFlicker(), emittanceSrc };
}

std::uint32_t LightData::ReattachExistingLights(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_node) const
{
	auto* shadowSceneNode = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
	auto  lightParams = GetParams(a_ref);

	std::uint32_t lightCount = 0;
	RE::BSVisit::TraverseScenegraphLights(a_node, [shadowSceneNode, lightParams, &lightCount](RE::NiPointLight* ptLight) {
		if (ptLight->name.contains(LP_ID)) {
			lightCount++;
			if (auto bsLight = shadowSceneNode->GetPointLight(ptLight); !bsLight) {
				shadowSceneNode->AddLight(ptLight, lightParams);
			}
		}
		return RE::BSVisit::BSVisitControl::kContinue;
	});
	return lightCount;
}

void LoadFormsFromAttachLightVec(AttachLightDataVec& a_attachLightDataVec)
{
	for (auto& attachLightData : a_attachLightDataVec) {
		std::visit(overload{
					   [&](PointData& pointData) {
						   pointData.data.LoadFormsFromConfig();
					   },
					   [&](NodeData& nodeData) {
						   nodeData.data.LoadFormsFromConfig();
					   },
					   [&](FilteredData& filteredData) {
						   filteredData.data.LoadFormsFromConfig();
					   } },
			attachLightData);
	}
}

bool FilteredData::IsInvalid(const std::string& a_model) const
{
	return (!blackList.empty() && blackList.contains(a_model)) || (!whiteList.empty() && !whiteList.contains(a_model));
}
