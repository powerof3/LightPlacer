#pragma once

#include "LightData.h"

struct PlacedLight
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
		void InsertConditionalNodes(const std::vector<std::string>& a_nodes, bool a_isVisble);
		void UpdateNodeVisibility(const RE::TESObjectREFR* a_ref, std::string_view a_nodeName);
		void Reset();

		// members
		bool            isVisible{ false };
		bool            canCullAddonNodes{ false };
		bool            canCullNodes{ false };
		StringMap<bool> conditionalNodes{};
	};

	PlacedLight(const LIGH::LightDefinitionPtr& a_lightDef, const LightInstance& a_lightInstance, const RE::TESObjectREFRPtr& a_ref);

	bool operator==(const PlacedLight& rhs) const
	{
		return instance == rhs.instance;
	}

	bool operator==(const LightInstance& rhs) const
	{
		return instance == rhs;
	}

	const LightData&                       GetData() const { return definition->data; }
	const RE::NiPointer<RE::NiPointLight>& GetLight() const { return instance.GetLight(); }

	void ReattachLight(RE::TESObjectREFR* a_ref);
	bool ShouldUpdateConditions(ConditionUpdateFlags a_flags) const;
	void UpdateAnimation(float a_delta, float a_scalingFactor);
	void UpdateConditions(RE::TESObjectREFR* a_ref, NodeVisHelper& a_nodeVisHelper, ConditionUpdateFlags a_flags);
	void UpdateEmittance(RE::TESObjectCELL* a_cell) const;
	void UpdateVanillaFlickering() const;

	// members
	LIGH::LightDefinitionPtr          definition{};
	LightInstance                     instance{};
	std::unique_ptr<LightControllers> lightControllers{};
	RE::TESForm*                      emittanceForm{ nullptr };
	std::optional<bool>               lastVisibleState{};
};

using ConditionUpdateFlags = PlacedLight::ConditionUpdateFlags;

struct PlacedLights
{
	PlacedLights() = default;
	PlacedLights(const LIGH::LightDefinitionPtr& a_lightDef, const LightInstance& a_lightInstance, const RE::TESObjectREFRPtr& a_ref);

	struct UpdateParams
	{
		RE::TESObjectREFR* ref;
		RE::NiPoint3       pcPos;
		float              delta;
		std::string_view   nodeName{ ""sv };
		float              dimFactor{ RE::NI_INFINITY };
	};

	std::size_t size() const { return lights.size(); }

	bool emplace_back(const LIGH::LightDefinitionPtr& a_lightDef, const LightInstance& a_lightInstance, const RE::TESObjectREFRPtr& a_ref);

	void ShowDebugMarkers(bool a_show) const;

	void ToggleLights(bool a_toggle, LIGHT_CULL_FLAGS a_flags) const;
	bool GetLightsToggled(LIGHT_CULL_FLAGS a_flags) const;

	void ReattachLights(RE::TESObjectREFR* a_ref);
	void ReattachLights() const;
	void RemoveLights(bool a_clearData) const;

	bool UpdateTimer(float a_delta, float a_interval);
	void UpdateConditions(RE::TESObjectREFR* a_ref, std::string_view a_nodeName, ConditionUpdateFlags a_flags);
	void UpdateLightsAndRef(const UpdateParams& a_params);
	void UpdateEmittance(RE::TESObjectCELL* a_cell) const;

	// members
	float                      lastUpdateTime{ std::numeric_limits<float>::max() };
	std::vector<PlacedLight>   lights;
	PlacedLight::NodeVisHelper nodeVisHelper{};
};

struct LightsToUpdate
{
	LightsToUpdate() = default;
	LightsToUpdate(RE::RefHandle a_handle);
	LightsToUpdate(const LightData& a_lightData, RE::RefHandle a_handle);

	void emplace(const LightData& a_lightData, RE::RefHandle a_handle);
	void emplace(RE::RefHandle a_handle);

	void erase(RE::RefHandle a_handle);

	// members
	std::vector<RE::RefHandle> updatingLights;
	std::vector<RE::RefHandle> emittanceLights;
};
