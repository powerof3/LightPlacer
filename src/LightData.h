#pragma once

#include "LightControllers.h"

struct Timer
{
	Timer() = default;

	bool UpdateTimer(float a_interval, float a_delta);
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

struct LightDataBase
{
	// CS light flags
	enum class LightFlags
	{
		None = 0,
		PortalStrict = (1 << 0),
		Shadow = (1 << 1),
		Simple = (1 << 2)
	};

	float                                    GetRadius() const;
	float                                    GetFade() const;
	std::string                              GetName(std::uint32_t a_index) const;
	static std::string                       GetNodeName(std::uint32_t a_index);
	static std::string                       GetNodeName(RE::NiAVObject* a_obj, std::uint32_t a_index);
	RE::ShadowSceneNode::LIGHT_CREATE_PARAMS GetParams(RE::TESObjectREFR* a_ref) const;
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

struct LightCreateParams : LightDataBase
{
	LightCreateParams() = default;
	LightCreateParams(const RE::NiStringsExtraData* a_data);

	void ReadFlags();
	void ReadConditions();
	bool PostProcess();

	// members
	std::string                        lightEDID;
	std::string                        emittanceFormEDID;
	std::string                        rawFlags;
	std::vector<std::string>           rawConditions;
	std::vector<Keyframe<RE::NiColor>> rawColorController;
};

template <>
struct glz::meta<LightCreateParams>
{
	using T = LightCreateParams;
	static constexpr auto value = object(
		"light", &T::lightEDID,
		"color", &T::color,
		"radius", &T::radius,
		"fade", &T::fade,
		"offset", &T::offset,
		"externalEmittance", &T::emittanceFormEDID,
		"flags", &T::rawFlags,
		"conditions", &T::rawConditions,
		"colorController", &T::rawColorController);
};

struct REFR_LIGH : LightDataBase
{
	REFR_LIGH() = default;
	REFR_LIGH(const LightCreateParams& a_lightParams, RE::BSLight* a_bsLight, RE::NiPointLight* a_niLight, RE::TESObjectREFR* a_ref, RE::NiNode* a_node, const RE::NiPoint3& a_point, std::uint32_t a_index) :
		LightDataBase(a_lightParams),
		bsLight(a_bsLight),
		niLight(a_niLight),
		parentNode(a_node),
		point(a_point),
		index(a_index),
		isReference(!RE::IsActor(a_ref))
	{
		if (!emittanceForm) {
			auto xData = a_ref->extraList.GetByType<RE::ExtraEmittanceSource>();
			emittanceForm = xData ? xData->source : nullptr;
		}

		if (!a_lightParams.rawColorController.empty()) {
			colorController = LightColorController(a_lightParams.rawColorController);
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

	RE::NiPointer<RE::BSLight>          bsLight;
	RE::NiPointer<RE::NiPointLight>     niLight;
	RE::NiPointer<RE::NiNode>           parentNode;
	std::optional<LightColorController> colorController;
	RE::NiPoint3                        point;
	std::uint32_t                       index{ 0 };
	bool                                isReference{};

private:
	void UpdateLight() const;
};

struct ProcessedREFRLights : Timer
{
	ProcessedREFRLights() = default;

	void emplace(const REFR_LIGH& a_data, RE::RefHandle a_handle);
	void erase(RE::RefHandle a_handle);

	// members
	std::vector<RE::RefHandle> animatedLights; // color/flickering
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
