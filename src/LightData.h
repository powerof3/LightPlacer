#pragma once

struct ObjectRefData
{
	ObjectRefData() = default;
	ObjectRefData(RE::TESObjectREFR* a_ref);
	ObjectRefData(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root);

	bool IsValid();

	// members
	RE::TESObjectREFR* ref{};
	RE::NiNode*        root{};
	RE::FormID         cellFormID{};
	RE::RefHandle      handle{};
};

struct LightData
{
	LightData() = default;
	LightData(const RE::NiStringsExtraData* a_data);

	bool operator==(const LightData& a_rhs) const
	{
		return std::tie(light, radius, fade) == std::tie(a_rhs.light, a_rhs.radius, a_rhs.fade);
	}
	bool operator<(const LightData& a_rhs) const
	{
		return std::tie(light, radius, fade) < std::tie(a_rhs.light, a_rhs.radius, a_rhs.fade);
	}

	void LoadFormsFromConfig();

	std::string                              GetName(std::uint32_t a_index) const;
	RE::NiColor                              GetDiffuse() const;
	float                                    GetRadius() const;
	float                                    GetFade() const;
	RE::ShadowSceneNode::LIGHT_CREATE_PARAMS GetParams(RE::TESObjectREFR* a_ref) const;

	RE::NiNode* GetOrCreateNode(RE::NiNode* a_root, const std::string& a_nodeName, std::uint32_t a_index) const;
	RE::NiNode* GetOrCreateNode(RE::NiNode* a_root, RE::NiAVObject* a_obj, std::uint32_t a_index) const;

	// [light, flickers, emittance]
	std::tuple<RE::NiPointLight*, bool, RE::TESForm*> SpawnLight(RE::TESObjectREFR* a_ref, RE::NiNode* a_node, const RE::NiPoint3& a_point, std::uint32_t a_index) const;
	// count
	std::uint32_t ReattachExistingLights(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_node) const;

	// members
	RE::TESObjectLIGH* light{ nullptr };
	std::string        lightEDID{};
	float              radius{ 0.0f };
	float              fade{ 0.0f };
	RE::NiPoint3       offset{};
	RE::TESForm*       emittanceForm{ nullptr };
	std::string        emittanceFormEDID{};
	float              chance{ 100.0 };
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
		"chance", &T::chance);
};

struct PointData
{
	void LoadFormsFromConfig() { data.LoadFormsFromConfig(); }
	
	std::vector<RE::NiPoint3> points{};
	LightData                 data{};
};

struct NodeData
{
	void LoadFormsFromConfig() { data.LoadFormsFromConfig(); }
	
	std::vector<std::string> nodes{};
	LightData                data{};
};

using AttachLightData = std::variant<PointData, NodeData>;
using AttachLightDataVec = std::vector<AttachLightData>;

void LoadFormsFromAttachLightVec(AttachLightDataVec& a_attachLightDataVec);
