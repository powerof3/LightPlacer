#pragma once

#include "LightControllers.h"

struct SourceData;

struct LightData
{
	// CS light flags
	enum class Flags
	{
		None = 0,
		PortalStrict = (1 << 0),
		Shadow = (1 << 1),
		Simple = (1 << 2),

		SyncAddonNodes = (1 << 27),
		IgnoreScale = (1 << 28),
		RandomAnimStart = (1 << 29),
		NoExternalEmittance = (1 << 30)
	};

	void AttachDebugMarker(RE::NiNode* a_node) const;

	bool        GetCastsShadows() const;
	RE::NiColor GetDiffuse() const;

	float GetRadius() const;
	float GetFade() const;

	float GetScaledRadius(float a_radius, float a_scale) const;
	float GetScaledFade(float a_fade, float a_scale) const;
	float GetScaledRadius(float a_scale) const;
	float GetScaledFade(float a_scale) const;

	float                                    GetFOV() const;
	float                                    GetFalloff() const;
	float                                    GetNearDistance() const;
	std::string                              GetName(const SourceData& a_srcData, std::uint32_t a_index) const;
	static std::string                       GetNodeName(const RE::NiPoint3& a_point, std::uint32_t a_index);
	static std::string                       GetNodeName(RE::NiAVObject* a_obj, std::uint32_t a_index);
	RE::ShadowSceneNode::LIGHT_CREATE_PARAMS GetParams(RE::TESObjectREFR* a_ref) const;
	bool                                     GetPortalStrict() const;
	bool                                     IsDynamicLight(RE::TESObjectREFR* a_ref) const;
	bool                                     IsValid() const;

	std::pair<RE::BSLight*, RE::NiPointLight*> GenLight(RE::TESObjectREFR* a_ref, RE::NiNode* a_node, std::string_view a_lightName, float a_scale) const;

	// members
	RE::TESObjectLIGH* light{ nullptr };
	RE::NiColor        color{ RE::COLOR_BLACK };
	float              radius{ 0.0f };
	float              fade{ 0.0f };
	float              fov{ 0.0f };
	float              shadowDepthBias{ 1.0f };
	RE::NiPoint3       offset;
	RE::NiMatrix3      rotation;
#if defined(SKYRIMVR)
	stl::enumeration<Flags, std::uint32_t> flags{ Flags::None };
#else
	REX::EnumSet<Flags, std::uint32_t> flags{ Flags::None };
#endif
	RE::TESForm*                      emittanceForm{ nullptr };
	std::shared_ptr<RE::TESCondition> conditions;
	StringSet                         conditionalNodes;

	constexpr static auto LP_LIGHT = "LP_Light"sv;
	constexpr static auto LP_NODE = "LP_Node"sv;
	constexpr static auto LP_DEBUG = "LP_DebugMarker"sv;
};

struct LightSourceData
{
	LightSourceData() = default;
	LightSourceData(const RE::NiStringsExtraData* a_data);

	void read_color(RE::NiColor a_value)
	{
		for (std::size_t i = 0; i < RE::NiColor::kTotal; ++i) {
			if (a_value[i] >= 0.0f && a_value[i] <= 1.0f) {
				continue;
			}
			a_value[i] = a_value[i] / 255;
		}
		data.color = a_value;
	}

	RE::NiColor write_color() const
	{
		return data.color;
	}

	void ReadFlags();
	void ReadConditions();
	bool PostProcess();

	bool IsStaticLight() const;

	RE::NiNode* GetOrCreateNode(RE::NiNode* a_root, const RE::NiPoint3& a_point, std::uint32_t a_index) const;
	RE::NiNode* GetOrCreateNode(RE::NiNode* a_root, const std::string& a_nodeName, std::uint32_t a_index) const;
	RE::NiNode* GetOrCreateNode(RE::NiNode* a_root, RE::NiAVObject* a_obj, std::uint32_t a_index) const;

	// members
	LightData                data;
	std::string              lightEDID;
	std::string              emittanceFormEDID;
	std::string              flags;
	std::vector<std::string> conditions;
	ColorKeyframeSequence    colorController;
	FloatKeyframeSequence    radiusController;
	FloatKeyframeSequence    fadeController;
	PosKeyframeSequence      positionController;
	RotKeyframeSequence      rotationController;
};

template <>
struct glz::meta<LightSourceData>
{
	using T = LightSourceData;
	static constexpr auto value = object(
		"light", &T::lightEDID,
		"color", "color", custom<&T::read_color, &T::write_color>,
		"radius", [](auto&& self) -> auto& { return self.data.radius; },
		"fade", [](auto&& self) -> auto& { return self.data.fade; },
		"fov", [](auto&& self) -> auto& { return self.data.fov; },
		"shadowDepthBias", [](auto&& self) -> auto& { return self.data.shadowDepthBias; },
		"offset", [](auto&& self) -> auto& { return self.data.offset; },
		"rotation", [](auto&& self) -> auto& { return self.data.rotation; },
		"externalEmittance", &T::emittanceFormEDID,
		"flags", &T::flags,
		"conditions", &T::conditions,
		"conditionalNodes", [](auto&& self) -> auto& { return self.data.conditionalNodes; },
		"colorController", &T::colorController,
		"radiusController", &T::radiusController,
		"fadeController", &T::fadeController,
		"positionController", &T::positionController,
		"rotationController", &T::rotationController);
};

struct REFR_LIGH
{
	// cull nodes based on condition state
	struct NodeVisHelper
	{
		void InsertConditionalNodes(const StringSet& a_nodes, bool a_isVisble);
		void UpdateNodeVisibility(const RE::TESObjectREFR* a_ref, std::string_view a_nodeName);
		void Reset();

		// members
		bool            isVisible{ false };
		bool            canCullAddonNodes{ false };
		bool            canCullNodes{ false };
		StringMap<bool> conditionalNodes;
	};

	REFR_LIGH() = default;
	REFR_LIGH(const LightSourceData& a_lightSource, RE::BSLight* a_bsLight, RE::NiPointLight* a_niLight, RE::TESObjectREFR* a_ref, float a_scale);

	bool operator==(const REFR_LIGH& rhs) const
	{
		return niLight->name == rhs.niLight->name;
	}

	bool operator==(const RE::NiPointLight* rhs) const
	{
		return niLight->name == rhs->name;
	}

	bool IsAnimated() const;

	void DimLight(float a_dimmer) const;
	void ReattachLight(RE::TESObjectREFR* a_ref);
	void ReattachLight() const;
	void RemoveLight(bool a_clearData) const;
	void ShowDebugMarker(bool a_show) const;
	void UpdateAnimation(bool a_withinRange, float a_scalingFactor);
	void UpdateConditions(RE::TESObjectREFR* a_ref, NodeVisHelper& a_nodeVisHelper);
	void UpdateFlickering() const;
	void UpdateEmittance() const;

	LightData                       data;
	RE::NiPointer<RE::BSLight>      bsLight;
	RE::NiPointer<RE::NiPointLight> niLight;
	RE::NiPointer<RE::NiAVObject>   debugMarker;
	std::optional<ColorController>  colorController;
	std::optional<FloatController>  radiusController;
	std::optional<FloatController>  fadeController;
	std::optional<PosController>    positionController;
	std::optional<RotController>    rotationController;
	float                           scale{ 1.0f };
	std::optional<bool>             lastVisibleState;

private:
	void UpdateLight() const;
};
