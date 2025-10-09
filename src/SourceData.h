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

	bool          IsValid() const;
	RE::NiNodePtr GetAttachNode() const;
	std::string   GetWornItemNodeName() const;

	// members
	SOURCE_TYPE          type{ SOURCE_TYPE::kNone };
	std::uint32_t        miscID{ std::numeric_limits<std::uint32_t>::max() };
	RE::TESObjectREFRPtr ref{};
	RE::TESBoundObject*  base{};
	RE::NiNode*          root{};
	std::string_view     modelPath;
};

using SourceDataPtr = std::unique_ptr<SourceData>;

struct SourceAttachData
{
	SourceAttachData() = default;

	bool Initialize(const SourceDataPtr& a_srcData);

	// members
	SOURCE_TYPE             type{ SOURCE_TYPE::kNone };
	std::uint32_t           miscID{ std::numeric_limits<std::uint32_t>::max() };
	RE::TESObjectREFRPtr    ref{};
	RE::NiNodePtr           root{};
	RE::NiNodePtr           attachNode{};
	float                   scale{};
	std::string             nodeName{};
	std::vector<RE::FormID> filterIDs{};
};

using SourceAttachDataPtr = std::shared_ptr<SourceAttachData>;
