#pragma once

#include "LightControllers.h"

struct SourceAttachData;

enum class LIGHT_FLAGS
{
	// CS light flags
	None = 0,
	PortalStrict = (1 << 0),
	Shadow = (1 << 1),
	Simple = (1 << 2),

	// CS ISL flags
	Initialised = (1 << 8),
	Disabled = (1 << 9),
	InverseSquare = (1 << 10),

	// LP flags
	UpdateOnWaiting = (1 << 18),
	UpdateOnCellTransition = (1 << 19),
	NeedsUpdate = UpdateOnWaiting | UpdateOnCellTransition,
	SyncAddonNodes = (1 << 20),
	IgnoreScale = (1 << 21),
	RandomAnimStart = (1 << 22),
	NoExternalEmittance = (1 << 23)

	// LIGHT_CULL_FLAGS 24 - 31
};

enum class LIGHT_CULL_FLAGS
{
	None = 0,
	Conditions = (1 << 0),
	Game = (1 << 1),
	Script = (1 << 2),
};

enum class TES_LIGHT_FLAGS_EXT
{
	kInverseSquare = 1 << 14,
};

struct LightOutput
{
	LightOutput() = default;
	LightOutput(RE::BSLight* a_bsLight, RE::NiPointLight* a_niLight, RE::NiAVObject* a_debugMarker) :
		bsLight(a_bsLight),
		niLight(a_niLight),
		debugMarker(a_debugMarker)
	{}

	LightOutput& operator=(const LightOutput& rhs)
	{
		bsLight = rhs.bsLight;
		niLight = rhs.niLight;
		debugMarker = rhs.debugMarker;
		return *this;
	}

	bool operator==(const LightOutput& rhs) const
	{
		return niLight->name == rhs.niLight->name;
	}

	const RE::NiPointer<RE::NiPointLight>& GetLight() const;

	bool DimLight(float a_dimmer) const;
	void ReattachLight() const;
	void RemoveLight(bool a_clearData) const;
	void ShowDebugMarker() const;
	void HideDebugMarker() const;
	void UpdateDebugMarkerState(bool a_culled) const;

	RE::NiPointer<RE::BSLight>      bsLight{};
	RE::NiPointer<RE::NiPointLight> niLight{};
	RE::NiPointer<RE::NiAVObject>   debugMarker{};
};

struct LightData
{
	bool                                     GetCastsShadows() const;
	RE::NiColor                              GetDiffuse() const;
	float                                    GetRadius() const;
	float                                    GetFade() const;
	float                                    GetScaledRadius(float a_scale) const;
	float                                    GetScaledFade(float a_scale) const;
	float                                    GetScaledValue(float a_value, float a_scale) const;
	float                                    GetFOV() const;
	LIGHT_FLAGS                              GetLightFlags() const;
	bool                                     GetInverseSquare() const;
	float                                    GetCutoff() const;
	float                                    GetSize() const;
	float                                    GetScaledSize(float a_scale) const;
	float                                    GetFalloff() const;
	float                                    GetNearDistance() const;
	static std::string                       GetLightName(const std::unique_ptr<SourceAttachData>& a_srcData, std::string_view a_lightEDID, std::uint32_t a_index);
	static std::string                       GetNodeName(const RE::NiPoint3& a_point, std::uint32_t a_index);
	static std::string                       GetNodeName(RE::NiAVObject* a_obj, std::uint32_t a_index);
	RE::ShadowSceneNode::LIGHT_CREATE_PARAMS GetParams(const RE::TESObjectREFR* a_ref) const;
	bool                                     GetPortalStrict() const;
	bool                                     IsDynamicLight(const RE::TESObjectREFR* a_ref) const;
	bool                                     IsValid() const;

	LightOutput GenLight(RE::TESObjectREFR* a_ref, RE::NiNode* a_node, std::string_view a_lightName, float a_scale) const;  // [bsLight, niLight, debugMarker]

	static LIGHT_CULL_FLAGS GetCulledFlag(RE::NiPointLight* a_light);
	static void             CullLight(RE::NiPointLight* a_light, RE::NiAVObject* a_debugMarker, bool a_hide, LIGHT_CULL_FLAGS a_flags);
	static const char*      GetCulledStatus(RE::NiPointLight* a_light);

	// members
	RE::TESObjectLIGH*                       light{ nullptr };
	RE::NiColor                              color{ RE::COLOR_BLACK };
	float                                    radius{ 0.0f };
	float                                    fade{ 0.0f };
	float                                    fov{ 0.0f };
	float                                    cutoff{ 0.0f };
	float                                    size{ 0.0f };
	float                                    shadowDepthBias{ 1.0f };
	RE::NiPoint3                             offset;
	RE::NiMatrix3                            rotation;
	REX::EnumSet<LIGHT_FLAGS, std::uint32_t> flags{ LIGHT_FLAGS::None };
	RE::TESForm*                             emittanceForm{ nullptr };
	std::shared_ptr<RE::TESCondition>        conditions;
	StringSet                                conditionalNodes;

	constexpr static auto LP_LIGHT = "LP_Light"sv;
	constexpr static auto LP_NODE = "LP_Node"sv;
	constexpr static auto LP_DEBUG = "LP_DebugMarker"sv;

private:
	struct MARKER_CREATE_PARAMS
	{
		const char*  modelName;
		const char*  shapeName;
		float        scale;
		RE::NiPoint3 rotation;
	};

	static std::string   GetDebugMarkerName(std::string_view a_lightName);
	RE::NiAVObject*      AttachDebugMarker(RE::NiNode* a_node, std::string_view a_debugMarkerName) const;
	static void          PostProcessDebugMarker(RE::NiAVObject* a_obj, const MARKER_CREATE_PARAMS& a_params, std::string_view a_debugMarkerName);
	MARKER_CREATE_PARAMS GetDebugMarkerParams() const;
};

namespace LIGH
{
	struct LightSourceData
	{
		LightSourceData() = default;

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
		std::vector<std::string> conditions;
		ColorKeyframeSequence    colorController;
		FloatKeyframeSequence    radiusController;
		FloatKeyframeSequence    fadeController;
		PositionKeyframeSequence positionController;
		RotationKeyframeSequence rotationController;
		AIOKeyframeSequence      aioController;
	};
}

template <>
struct glz::meta<LIGH::LightSourceData>
{
	using T = LIGH::LightSourceData;

	static constexpr auto read_flags = [](T& s, const std::string& input) {
		if (!input.empty()) {
			const auto flagStrs = string::split(input, "|");
			for (const auto& flagStr : flagStrs) {
				switch (string::const_hash(flagStr)) {
				case "PortalStrict"_h:
					s.data.flags.set(LIGHT_FLAGS::PortalStrict);
					break;
				case "Shadow"_h:
					s.data.flags.set(LIGHT_FLAGS::Shadow);
					break;
				case "Simple"_h:
					s.data.flags.set(LIGHT_FLAGS::Simple);
					break;
				case "InverseSquare"_h:
					s.data.flags.set(LIGHT_FLAGS::InverseSquare);
					break;

				case "UpdateOnWaiting"_h:
					s.data.flags.set(LIGHT_FLAGS::UpdateOnWaiting);
					break;
				case "UpdateOnCellTransition"_h:
					s.data.flags.set(LIGHT_FLAGS::UpdateOnCellTransition);
					break;
				case "SyncAddonNodes"_h:
					s.data.flags.set(LIGHT_FLAGS::SyncAddonNodes);
					break;
				case "IgnoreScale"_h:
					s.data.flags.set(LIGHT_FLAGS::IgnoreScale);
					break;
				case "RandomAnimStart"_h:
					s.data.flags.set(LIGHT_FLAGS::RandomAnimStart);
					break;
				case "NoExternalEmittance"_h:
					s.data.flags.set(LIGHT_FLAGS::NoExternalEmittance);
					break;
				default:
					break;
				}
			}
		}
	};
	static constexpr auto write_flags = [](auto&) -> auto& { return ""; };

	static constexpr auto read_aioController = [](auto& s) -> bool {
		if (!s.aioController.empty()) {
			s.colorController.clear();
			s.radiusController.clear();
			s.fadeController.clear();
			s.positionController.clear();
			s.rotationController.clear();

			s.colorController.interpolation = s.aioController.interpolation;
			s.radiusController.interpolation = s.aioController.interpolation;
			s.fadeController.interpolation = s.aioController.interpolation;
			s.positionController.interpolation = s.aioController.interpolation;
			s.rotationController.interpolation = s.aioController.interpolation;

			for (auto& key : s.aioController.keys) {
				if (key.value.GetValidColor()) {
					s.colorController.keys.push_back(ColorKeyframe(key.time, key.value.color, key.forward.color, key.backward.color));
				}
				if (key.value.GetValidRadius()) {
					s.radiusController.keys.push_back(FloatKeyframe(key.time, key.value.radius, key.forward.radius, key.backward.radius));
				}
				if (key.value.GetValidFade()) {
					s.fadeController.keys.push_back(FloatKeyframe(key.time, key.value.fade, key.forward.fade, key.backward.fade));
				}
				if (key.value.GetValidTranslation()) {
					s.positionController.keys.push_back(PositionKeyframe(key.time, key.value.translation, key.forward.translation, key.backward.translation));
				}
				if (key.value.GetValidRotation()) {
					s.rotationController.keys.push_back(RotationKeyframe(key.time, key.value.rotation, key.forward.rotation, key.backward.rotation));
				}
			}
		}
		return true;
	};
	static constexpr auto write_aioController = [](auto&) -> bool { return true; };

	static constexpr auto value = object(
		"light", &T::lightEDID,
		"color", [](auto&& self) -> auto& { return self.data.color; },
		"radius", [](auto&& self) -> auto& { return self.data.radius; },
		"fade", [](auto&& self) -> auto& { return self.data.fade; },
		"fov", [](auto&& self) -> auto& { return self.data.fov; },
		"cutoff", [](auto&& self) -> auto& { return self.data.cutoff; },
		"size", [](auto&& self) -> auto& { return self.data.size; },
		"shadowDepthBias", [](auto&& self) -> auto& { return self.data.shadowDepthBias; },
		"offset", [](auto&& self) -> auto& { return self.data.offset; },
		"rotation", [](auto&& self) -> auto& { return self.data.rotation; },
		"externalEmittance", &T::emittanceFormEDID,
		"flags", glz::custom<read_flags, write_flags>,
		"conditions", &T::conditions,
		"conditionalNodes", [](auto&& self) -> auto& { return self.data.conditionalNodes; },
		"colorController", &T::colorController,
		"radiusController", &T::radiusController,
		"fadeController", &T::fadeController,
		"positionController", &T::positionController,
		"rotationController", &T::rotationController,
		"lightController", glz::manage<&T::aioController, read_aioController, write_aioController>);
};

struct REFR_LIGH
{
	struct Condition
	{
		enum UpdateFlags : std::uint8_t
		{
			Skip = 0,
			Normal = (1 << 0),
			Forced = (1 << 1),
			CellTransition = (1 << 2),
			Waiting = (1 << 3),

			UpdateRequired = CellTransition | Waiting
		};
	};
	using ConditionUpdateFlags = Condition::UpdateFlags;

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
	REFR_LIGH(const LIGH::LightSourceData& a_lightSource, const LightOutput& a_lightOutput, const RE::TESObjectREFRPtr& a_ref, float a_scale);

	bool operator==(const REFR_LIGH& rhs) const
	{
		return output == rhs.output;
	}

	bool operator==(const LightOutput& rhs) const
	{
		return output == rhs;
	}

	const RE::NiPointer<RE::NiPointLight>& GetLight() const { return output.GetLight(); }

	void ReattachLight(RE::TESObjectREFR* a_ref);
	bool ShouldUpdateConditions(ConditionUpdateFlags a_flags) const;
	void UpdateAnimation(float a_delta, float a_scalingFactor);
	void UpdateConditions(RE::TESObjectREFR* a_ref, NodeVisHelper& a_nodeVisHelper, ConditionUpdateFlags a_flags);
	void UpdateEmittance() const;
	void UpdateVanillaFlickering() const;

	LightData           data;
	LightOutput         output;
	LightControllers    lightControllers;
	float               scale{ 1.0f };
	std::optional<bool> lastVisibleState;
};

using ConditionUpdateFlags = REFR_LIGH::ConditionUpdateFlags;
