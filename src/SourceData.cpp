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
	if (type == SOURCE_TYPE::kActorMagic && ref->IsPlayerRef()) {  // light doesn't get show in first person (possibly culled when switching nodes)?
		if (ref->Get3D(true)) {
			const auto tpRoot = ref->Get3D(false);
			if (const auto node = tpRoot->GetObjectByName(root->parent->name)) {
				return node->AsNode();
			}
		}
	}
	return root;
}

std::string SourceData::GetWornItemNodeName() const
{
	if (type != SOURCE_TYPE::kActorWorn) {
		return std::string();
	}

	char nodeName[MAX_PATH]{ '\0' };
	if (auto armo = base->As<RE::TESObjectARMO>()) {
		if (auto arma = RE::TESForm::LookupByID<RE::TESObjectARMA>(miscID)) {
			arma->GetNodeName(nodeName, ref, armo, -1);
		}
	} else if (const auto weap = base->As<RE::TESObjectWEAP>()) {
		weap->GetNodeName(nodeName);
	}

	return nodeName;
}

bool SourceAttachData::Initialize(const SourceData& a_srcData)
{
	if (!attachNode) {
		if (auto parentCell = a_srcData.ref->GetParentCell()) {
			type = a_srcData.type;
			effectID = a_srcData.miscID;
			ref = a_srcData.ref;
			root = a_srcData.root;
			attachNode = a_srcData.GetAttachNode();
			handle = ref->CreateRefHandle().native_handle();
			scale = ref->GetScale();
			nodeName = std::move(a_srcData.GetWornItemNodeName());

			filterIDs.push_back(parentCell->GetFormID());

			filterIDs.push_back(ref->GetFormID());
			filterIDs.push_back(a_srcData.base->GetFormID());

			if (auto worldSpace = ref->GetWorldspace()) {
				filterIDs.push_back(worldSpace->GetFormID());
			}
			if (auto location = ref->GetCurrentLocation()) {
				filterIDs.push_back(location->GetFormID());
				for (auto it = location->parentLoc; it; it = it->parentLoc) {
					filterIDs.push_back(it->GetFormID());
				}
			}
		}
	}

	return attachNode != nullptr;
}
