#include "SourceData.h"

SourceData::SourceData(SOURCE_TYPE a_type, RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root, RE::TESBoundObject* a_object, RE::TESModel* a_model) :
	type(a_type),
	ref(a_ref),
	base(a_object),
	root(a_root->AsNode())
{
	RE::TESModel* model = a_model;
	if (!model) {
		model = a_object ? a_object->As<RE::TESModel>() : nullptr;
	}
	if (model) {
		modelPath = model->GetModel();
	}
}

SourceData::SourceData(SOURCE_TYPE a_type, RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_object, RE::TESModel* a_model) :
	SourceData(a_type, a_ref, a_ref->Get3D(), a_object, a_model)
{}

SourceData::SourceData(SOURCE_TYPE a_type, RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root, const RE::BIPOBJECT& a_bipObject) :
	SourceData(a_type, a_ref, a_root, a_bipObject.item->As<RE::TESBoundObject>(), a_bipObject.part)
{
	if (a_bipObject.addon) {
		miscID = a_bipObject.addon->GetFormID();
	}
}

bool SourceData::IsValid() const
{
	return !ref->IsDisabled() && !ref->IsDeleted() && root != nullptr;
}

RE::NiNode* SourceData::GetAttachNode() const
{
	if (type == SOURCE_TYPE::kActorWorn && base->Is(RE::FormType::Armor)) {
		return ref->Get3D()->AsNode();
	}
	return root;
}

std::string SourceData::GetWornItemNodeName() const
{
	if (type != SOURCE_TYPE::kActorWorn) {
		return {};
	}

	char nodeName[MAX_PATH]{ '\0' };
	if (auto armo = base->As<RE::TESObjectARMO>()) {
		if (auto arma = RE::TESForm::LookupByID<RE::TESObjectARMA>(miscID)) {
			arma->GetNodeName(nodeName, ref.get(), armo, -1);
		}
	} else if (const auto weap = base->As<RE::TESObjectWEAP>()) {
		weap->GetNodeName(nodeName);
	}

	return nodeName;
}

bool SourceAttachData::Initialize(const std::unique_ptr<SourceData>& a_srcData)
{
	if (!attachNode) {
		const auto& srcRef = a_srcData->ref;
		if (auto parentCell = srcRef ? srcRef->GetParentCell() : nullptr) {
			type = a_srcData->type;
			miscID = a_srcData->miscID;
			ref = srcRef;
			root = a_srcData->root;
			attachNode = a_srcData->GetAttachNode();
			scale = srcRef->GetScale();
			nodeName = std::move(a_srcData->GetWornItemNodeName());

			filterIDs.push_back(parentCell->GetFormID());

			filterIDs.push_back(srcRef->GetFormID());
			filterIDs.push_back(a_srcData->base->GetFormID());

			if (auto worldSpace = srcRef->GetWorldspace()) {
				filterIDs.push_back(worldSpace->GetFormID());
			}
			if (auto location = srcRef->GetCurrentLocation()) {
				filterIDs.push_back(location->GetFormID());
				for (auto it = location->parentLoc; it; it = it->parentLoc) {
					filterIDs.push_back(it->GetFormID());
				}
			}
		}
	}

	return IsValid();
}

bool SourceAttachData::IsValid() const
{
	return attachNode != nullptr;
}
