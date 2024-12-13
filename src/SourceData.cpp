#include "SourceData.h"

SourceData::SourceData(SOURCE_TYPE a_type, RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root, RE::TESBoundObject* a_object, RE::TESModel* a_model) :
	type(a_type),
	ref(a_ref),
	base(a_object),
	root(a_root->AsNode()),
	handle(a_ref->CreateRefHandle().native_handle())
{
	RE::TESModel* model = a_model;
	if (!model) {
		model = a_object ? a_object->As<RE::TESModel>() : nullptr;
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

SourceData::SourceData(SOURCE_TYPE a_type, RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_object, RE::TESModel* a_model) :
	SourceData(a_type, a_ref, a_ref->Get3D(), a_object, a_model)
{}

SourceData::SourceData(SOURCE_TYPE a_type, RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root, const RE::BIPOBJECT& a_bipObject) :
	SourceData(a_type, a_ref, a_root, a_bipObject.item->As<RE::TESBoundObject>(), a_bipObject.part)
{
	arma = a_bipObject.addon;
}

bool SourceData::IsValid() const
{
	if (type == SOURCE_TYPE::kRef || type == SOURCE_TYPE::kActorWorn) {
		return !ref->IsDisabled() && !ref->IsDeleted() && root && cellID != 0 && !modelPath.empty();
	}
	return !ref->IsDisabled() && !ref->IsDeleted() && root && cellID != 0;
}

RE::NiNode* SourceData::GetRootNode() const
{
	if (type == SOURCE_TYPE::kActorWorn && base->Is(RE::FormType::Armor)) {
		return ref->Get3D()->AsNode();
	}
	if (type == SOURCE_TYPE::kActorMagic && ref->IsPlayerRef()) { // light doesn't get show in first person (possibly culled when switching nodes)?
		if (ref->Get3D(true)) {
			const auto tpRoot = ref->Get3D(false);
			if (const auto node = tpRoot->GetObjectByName(root->parent->name)) {
				return node->AsNode();
			}
		}
	}
	return root;
}

void SourceData::GetWornItemNodeName(char* a_dstBuffer) const
{
	if (auto armo = base->As<RE::TESObjectARMO>()) {
		arma->GetNodeName(a_dstBuffer, ref, armo, -1);
	} else if (const auto weap = base->As<RE::TESObjectWEAP>()) {
		weap->GetNodeName(a_dstBuffer);
	}
}
