#pragma once

#include "LightControllers.h"

struct Timer
{
	Timer() = default;

	bool UpdateTimer(float a_interval);

	// members
	float lastUpdateTime{ 0.0f };
};

struct ObjectREFRParams
{
	ObjectREFRParams() = default;
	ObjectREFRParams(RE::TESObjectREFR* a_ref);
	ObjectREFRParams(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root);

	bool IsValid() const;

	// members
	RE::TESObjectREFR*   ref{};
	RE::ReferenceEffect* effect{};
	RE::NiNode*          root{};
	RE::RefHandle        handle{};
};

struct LightData
{
	// CS light flags
	enum class LightFlags
	{
		None = 0,
		PortalStrict = (1 << 0),
		Shadow = (1 << 1),
		Simple = (1 << 2)
	};

	bool                                     GetCastsShadows() const;
	RE::NiColor                              GetDiffuse() const;
	float                                    GetRadius() const;
	float                                    GetFade() const;
	float                                    GetFOV() const;
	float                                    GetFalloff() const;
	float                                    GetNearDistance() const;
	std::string                              GetName(std::uint32_t a_index) const;
	static std::string                       GetNodeName(std::uint32_t a_index);
	static std::string                       GetNodeName(RE::NiAVObject* a_obj, std::uint32_t a_index);
	RE::ShadowSceneNode::LIGHT_CREATE_PARAMS GetParams(RE::TESObjectREFR* a_ref) const;
	bool                                     IsDynamicLight(RE::TESObjectREFR* a_ref) const;
	bool                                     IsValid() const;

	static RE::NiNode* GetOrCreateNode(RE::NiNode* a_root, const std::string& a_nodeName, std::uint32_t a_index);
	static RE::NiNode* GetOrCreateNode(RE::NiNode* a_root, RE::NiAVObject* a_obj, std::uint32_t a_index);

	std::pair<RE::BSLight*, RE::NiPointLight*> GenLight(RE::TESObjectREFR* a_ref, RE::NiNode* a_node, const RE::NiPoint3& a_point = { 0, 0, 0 }, std::uint32_t a_index = 0) const;

	// members
	RE::TESObjectLIGH*                      light{ nullptr };
	RE::NiColor                             color{ RE::COLOR_BLACK };
	float                                   radius{ 0.0f };
	float                                   fade{ 0.0f };
	RE::NiPoint3                            offset;
	REX::EnumSet<LightFlags, std::uint32_t> flags{ LightFlags::None };
	RE::TESForm*                            emittanceForm{ nullptr };
	std::shared_ptr<RE::TESCondition>       conditions;

	constexpr static auto LP_ID = "LightPlacer"sv;
	constexpr static auto LP_NODE = "LightPlacerNode"sv;
};

struct LightCreateParams
{
	LightCreateParams() = default;
	LightCreateParams(const RE::NiStringsExtraData* a_data);

	void ReadFlags();
	void ReadConditions();
	bool PostProcess();

	// members
	LightData                data;
	std::string              lightEDID;
	std::string              emittanceFormEDID;
	std::string              flags;
	std::vector<std::string> conditions;
	ColorKeyframeSequence    colorController;
	FloatKeyframeSequence    radiusController;
	FloatKeyframeSequence    fadeController;
};

template <>
struct glz::meta<LightCreateParams>
{
	using T = LightCreateParams;
	static constexpr auto value = object(
		"light", &T::lightEDID,
		"color", [](auto&& self) -> auto& { return self.data.color; },
		"radius", [](auto&& self) -> auto& { return self.data.radius; },
		"fade", [](auto&& self) -> auto& { return self.data.fade; },
		"offset", [](auto&& self) -> auto& { return self.data.offset; },
		"externalEmittance", &T::emittanceFormEDID,
		"flags", &T::flags,
		"conditions", &T::conditions,
		"colorController", &T::colorController,
		"radiusController", &T::radiusController,
		"fadeController", &T::fadeController);
};

struct REFR_LIGH
{
	REFR_LIGH() = default;

	REFR_LIGH(const LightCreateParams& a_lightParams, RE::BSLight* a_bsLight, RE::NiPointLight* a_niLight, RE::TESObjectREFR* a_ref, RE::NiNode* a_node, const RE::NiPoint3& a_point, std::uint32_t a_index) :
		data(a_lightParams.data),
		bsLight(a_bsLight),
		niLight(a_niLight),
		parentNode(a_node),
		point(a_point),
		index(a_index),
		isReference(!RE::IsActor(a_ref))
	{
		if (!data.emittanceForm) {
			auto xData = a_ref->extraList.GetByType<RE::ExtraEmittanceSource>();
			data.emittanceForm = xData ? xData->source : nullptr;
		}

		if (!a_lightParams.colorController.empty()) {
			colorController = Animation::LightController(a_lightParams.colorController);
		}
		if (!a_lightParams.radiusController.empty()) {
			radiusController = Animation::LightController(a_lightParams.radiusController);
		}
		if (!a_lightParams.fadeController.empty()) {
			fadeController = Animation::LightController(a_lightParams.fadeController);
		}
	}

	bool operator==(const REFR_LIGH& rhs) const
	{
		return niLight->name == rhs.niLight->name;
	}

	bool operator==(const RE::NiPointLight* rhs) const
	{
		return niLight->name == rhs->name;
	}

	void ReattachLight(RE::TESObjectREFR* a_ref);

	void UpdateAnimation();
	void UpdateConditions(RE::TESObjectREFR* a_ref) const;
	void UpdateFlickering() const;
	void UpdateEmittance() const;
	void ReattachLight() const;
	void RemoveLight() const;

	LightData                       data;
	RE::NiPointer<RE::BSLight>      bsLight;
	RE::NiPointer<RE::NiPointLight> niLight;
	RE::NiPointer<RE::NiNode>       parentNode;
	std::optional<ColorController>  colorController;
	std::optional<FloatController>  radiusController;
	std::optional<FloatController>  fadeController;
	RE::NiPoint3                    point;
	std::uint32_t                   index{ 0 };
	bool                            isReference{};

private:
	void UpdateLight() const;
};

struct ProcessedREFRLights : Timer
{
	ProcessedREFRLights() = default;

	void emplace(const REFR_LIGH& a_lightData, RE::RefHandle a_handle);
	void erase(RE::RefHandle a_handle);

	// members
	std::vector<RE::RefHandle> animatedLights;  // color/flickering
	std::vector<RE::RefHandle> emittanceLights;
};

struct ProcessedEffectLights
{
	ProcessedEffectLights() = default;

	// members
	Timer                  flickerTimer;
	Timer                  conditionalTimer;
	std::vector<REFR_LIGH> lights;
};
