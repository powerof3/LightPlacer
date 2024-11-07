#pragma once

struct ObjectRefData
{
	ObjectRefData() = default;
	ObjectRefData(RE::TESObjectREFR* a_ref);
	ObjectRefData(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root);

	bool IsValid() const;

	// members
	RE::TESObjectREFR* ref{};
	RE::NiNode*        root{};
	RE::FormID         cellFormID{};
	RE::RefHandle      handle{};
};

// [light, flickers, emittance, conditions]
using SPAWN_LIGHT_PARAMS = std::tuple<RE::NiPointLight*, bool, RE::TESForm*, bool>;

struct LightData
{
	LightData() = default;
	LightData(const RE::NiStringsExtraData* a_data);

	// CS light flags
	enum class LightFlags : std::uint32_t
	{
		None = 0,
		PortalStrict = (1 << 0),
		Shadow = (1 << 1),
		Simple = (1 << 2),
		Particle = (1 << 3),
		Billboard = (1 << 4)
	};

	void ReadFlags();
	void ReadConditions();
	bool PostProcess();

	bool                                     IsValid() const;
	std::string                              GetName(std::uint32_t a_index) const;
	RE::NiColor                              GetDiffuse() const;
	float                                    GetRadius() const;
	float                                    GetFade() const;
	RE::ShadowSceneNode::LIGHT_CREATE_PARAMS GetParams(RE::TESObjectREFR* a_ref) const;

	RE::NiNode* GetOrCreateNode(RE::NiNode* a_root, const std::string& a_nodeName, std::uint32_t a_index) const;
	RE::NiNode* GetOrCreateNode(RE::NiNode* a_root, RE::NiAVObject* a_obj, std::uint32_t a_index) const;

	SPAWN_LIGHT_PARAMS SpawnLight(RE::TESObjectREFR* a_ref, RE::NiNode* a_node, const RE::NiPoint3& a_point, std::uint32_t a_index) const;
	// count
	std::uint32_t ReattachExistingLights(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_node) const;

	// members
	RE::TESObjectLIGH*                      light{ nullptr };
	std::string                             lightEDID{};
	float                                   radius{ 0.0f };
	float                                   fade{ 0.0f };
	RE::NiPoint3                            offset{};
	RE::TESForm*                            emittanceForm{ nullptr };
	std::string                             emittanceFormEDID{};
	REX::EnumSet<LightFlags, std::uint32_t> flags{ LightFlags::None };
	std::string                             rawFlags{};
	std::shared_ptr<RE::TESCondition>       conditions{};
	std::vector<std::string>                rawConditions{};
	float                                   chance{ 100.0 };

	constexpr static auto LP_ID = "LightPlacer|"sv;
	constexpr static auto LP_NODE = "LightPlacerNode #"sv;
};

template <>
struct glz::meta<LightData>
{
	using T = LightData;
	static constexpr auto value = object(
		"light", &T::lightEDID,
		"radius", &T::radius,
		"fade", &T::fade,
		"offset", &T::offset,
		"externalEmittance", &T::emittanceFormEDID,
		"flags", &T::rawFlags,
		"conditions", &T::rawConditions,
		"chance", &T::chance);
};

struct PointData
{
	std::vector<RE::NiPoint3> points{};
	LightData                 data{};
};

struct NodeData
{
	std::vector<std::string> nodes{};
	LightData                data{};
};

struct FilteredData
{
	bool IsInvalid(const std::string& a_model) const;

	StringSet whiteList;
	StringSet blackList;
	LightData data{};
};

using AttachLightData = std::variant<PointData, NodeData, FilteredData>;
using AttachLightDataVec = std::vector<AttachLightData>;

void AttachLightVecPostProcess(AttachLightDataVec& a_attachLightDataVec);
