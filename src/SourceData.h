#pragma once

enum class SOURCE_TYPE
{
	kNone = 0,
	kRef,
	kActor,
	kEffect
};

struct SourceData
{
	SourceData() = default;
	SourceData(SOURCE_TYPE a_type, RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root, RE::TESBoundObject* a_object, RE::TESModel* a_model = nullptr);
	SourceData(SOURCE_TYPE a_type, RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_object, RE::TESModel* a_model = nullptr);
	SourceData(SOURCE_TYPE a_type, RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root, const RE::BIPOBJECT& a_bipObject);

	bool        IsValid() const;
	RE::NiNode* GetRootNode() const;
	void        GetWornItemNodeName(char* a_dstBuffer) const;

	// members
	SOURCE_TYPE         type{ SOURCE_TYPE::kNone };
	RE::TESObjectREFR*  ref{};
	RE::TESBoundObject* base{};
	RE::TESObjectARMA*  arma{};
	RE::NiNode*         root{};
	RE::RefHandle       handle{};
	std::string_view    modelPath;

	RE::FormID cellID{ 0 };
	RE::FormID worldSpaceID{ 0 };
	RE::FormID locationID{ 0 };

	std::uint32_t effectID{ std::numeric_limits<std::uint32_t>::max() };
};
