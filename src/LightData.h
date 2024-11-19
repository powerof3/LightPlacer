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
	RE::RefHandle      handle{};
};

struct LightREFRData;

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
		Simple = (1 << 2)
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

	RE::NiPointLight* SpawnLight(RE::TESObjectREFR* a_ref, RE::NiNode* a_node, const RE::NiPoint3& a_point, std::uint32_t a_index) const;

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

	constexpr static auto LP_ID = "LightPlacer|"sv;
	constexpr static auto LP_NODE = "LightPlacerNode #"sv;
};

struct LightREFRData
{
	LightREFRData(RE::NiPointLight* a_ptLight, RE::TESObjectREFR* a_ref, const LightData& a_lightData) :
		ptLight(a_ptLight),
		light(a_lightData.light),
		fade(a_lightData.GetFade()),
		diffuse(a_lightData.GetDiffuse()),
		conditions(a_lightData.conditions)
	{
		emittance = a_lightData.emittanceForm;
		if (!emittance) {
			auto xData = a_ref->extraList.GetByType<RE::ExtraEmittanceSource>();
			emittance = xData ? xData->source : nullptr;
		}
	}

	bool operator==(const LightREFRData& rhs) const
	{
		return std::tie(ptLight, light, fade, diffuse, emittance, conditions) ==
		       std::tie(rhs.ptLight, rhs.light, rhs.fade, rhs.diffuse, rhs.emittance, rhs.conditions);
	}

	void UpdateConditions(const RE::TESObjectREFRPtr& a_ref) const;
	void UpdateFlickering(const RE::TESObjectREFRPtr& a_ref) const;
	void UpdateEmittance(const RE::TESObjectREFRPtr& a_ref) const;

	RE::NiPointer<RE::NiPointLight>   ptLight;
	RE::TESObjectLIGH*                light;
	float                             fade;
	RE::NiColor                       diffuse;
	RE::TESForm*                      emittance;
	std::shared_ptr<RE::TESCondition> conditions;

private:
	static void UpdateLight_Game(RE::TESObjectLIGH* a_light, const RE::NiPointer<RE::NiPointLight>& a_ptLight, RE::TESObjectREFR* a_ref, float a_wantDimmer);
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
		"conditions", &T::rawConditions);
};

namespace boost
{
	template <>
	struct hash<LightREFRData>
	{
		std::size_t operator()(const LightREFRData& data) const
		{
			return boost::hash<std::string>()(data.ptLight->name.c_str());
		}
	};
}

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

	Set<std::string> whiteList;
	Set<std::string> blackList;
	LightData        data{};
};

using AttachLightData = std::variant<PointData, NodeData, FilteredData>;
using AttachLightDataVec = std::vector<AttachLightData>;

void AttachLightVecPostProcess(AttachLightDataVec& a_attachLightDataVec);
