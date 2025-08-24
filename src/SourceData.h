#pragma once

enum class SOURCE_TYPE
{
	kNone = 0,
	kRef,
	kActorWorn,
	kActorMagic,
	kTempEffect
};

struct SourceData
{
	SourceData() = default;
	SourceData(SOURCE_TYPE a_type, RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root, RE::TESBoundObject* a_object, RE::TESModel* a_model = nullptr);
	SourceData(SOURCE_TYPE a_type, RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_object, RE::TESModel* a_model = nullptr);
	SourceData(SOURCE_TYPE a_type, RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root, const RE::BIPOBJECT& a_bipObject);

	bool        IsValid() const;
	RE::NiNode* GetAttachNode() const;
	std::string GetWornItemNodeName() const;

	// members
	SOURCE_TYPE          type{ SOURCE_TYPE::kNone };
	std::uint32_t        miscID{ std::numeric_limits<std::uint32_t>::max() };
	RE::TESObjectREFRPtr ref{};
	RE::TESBoundObject*  base{};
	RE::NiNode*          root{};
	std::string_view     modelPath;
};

struct SourceAttachData
{
	SourceAttachData() = default;

	bool Initialize(const std::unique_ptr<SourceData>& a_srcData);
	bool IsValid() const;

	// members
	SOURCE_TYPE             type{ SOURCE_TYPE::kNone };
	std::uint32_t           miscID{ std::numeric_limits<std::uint32_t>::max() };
	RE::TESObjectREFRPtr    ref{};
	RE::NiNode*             root{};
	RE::NiNode*             attachNode{};
	float                   scale{};
	std::string             nodeName{};
	std::vector<RE::FormID> filterIDs{};
};
