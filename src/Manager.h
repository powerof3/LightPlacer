#pragma once

#include "LightData.h"

struct Config
{
	struct FilteredData
	{
		bool IsInvalid(const std::string& a_model) const;

		FlatSet<std::string> whiteList;
		FlatSet<std::string> blackList;
		LightCreateParams    data{};
	};

	struct PointData
	{
		std::vector<RE::NiPoint3> points{};
		LightCreateParams         data{};
	};

	struct NodeData
	{
		std::vector<std::string> nodes{};
		LightCreateParams        data{};
	};

	using LightData = std::variant<PointData, NodeData, FilteredData>;
	using LightDataVec = std::vector<LightData>;

	struct MultiModelSet
	{
		FlatSet<std::string> models;
		LightDataVec         lightData;
	};

	struct MultiReferenceSet
	{
		FlatSet<std::string> references;
		LightDataVec         lightData;
	};

	struct MultiAddonSet
	{
		FlatSet<std::uint32_t> addonNodes;
		LightDataVec           lightData;
	};

	using Format = std::variant<MultiModelSet, MultiReferenceSet, MultiAddonSet>;
};

class LightManager : public ISingleton<LightManager>
{
public:
	bool ReadConfigs();
	void OnDataLoad();

	void AddLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base, RE::NiAVObject* a_root);
	void ReattachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base);
	void DetachLights(RE::TESObjectREFR* a_ref, bool a_clearData);

	void AddWornLights(RE::TESObjectREFR* a_ref, RE::BSTSmartPointer<RE::BipedAnim>& a_bipedAnim, std::int32_t a_slot, RE::NiAVObject* a_root);
	void ReattachWornLights(const RE::ActorHandle& a_handle);
	void DetachWornLights(const RE::ActorHandle& a_handle, RE::NiAVObject* a_root);

	void AddLightsToProcessQueue(RE::TESObjectCELL* a_cell, RE::TESObjectREFR* a_ref);
	void UpdateFlickeringAndConditions(RE::TESObjectCELL* a_cell);
	void UpdateEmittance(RE::TESObjectCELL* a_cell);
	void RemoveLightsFromProcessQueue(RE::TESObjectCELL* a_cell, const RE::ObjectRefHandle& a_handle);

	template <class F>
	void ForEachLight(RE::RefHandle a_handle, F&& func)
	{
		gameRefLights.read([&](const auto& map) {
			if (auto it = map.find(a_handle); it != map.end()) {
				for (auto& lightData : it->second) {
					func(lightData);
				}
			}
		});
	}

	template <class F>
	void ForEachWornLight(RE::RefHandle a_handle, F&& func)
	{
		gameActorLights.read([&](const auto& map) {
			if (auto it = map.find(a_handle); it != map.end()) {
				it->second.read([&](const auto& nodeMap) {
					for (auto& [node, lightDataVec] : nodeMap) {
						for (auto& lightData : lightDataVec) {
							func(lightData);
						}
					}
				});
			}
		});
	}

	template <class F>
	void ForEachLight(RE::TESObjectREFR* a_ref, RE::RefHandle a_handle, F&& func)
	{
		if (RE::IsActor(a_ref)) {
			ForEachWornLight(a_handle, func);
		} else {
			ForEachLight(a_handle, func);
		}
	}

private:
	struct ProcessedLights
	{
		ProcessedLights() = default;

		void emplace(const REFR_LIGH& a_data, RE::RefHandle a_handle);
		void erase(RE::RefHandle a_handle);

		std::vector<RE::RefHandle> flickeringLights;
		std::vector<RE::RefHandle> emittanceLights;
		float                      lastUpdateTime{ 0.0f };
		std::vector<RE::RefHandle> conditionalLights;
	};

	void PostProcessLightData(Config::LightDataVec& a_lightDataVec);

	void AttachLightsImpl(const ObjectREFRParams& a_refParams, RE::TESBoundObject* a_object, RE::TESModel* a_model);

	void AttachConfigLights(const ObjectREFRParams& a_refParams, const std::string& a_model, RE::FormID a_baseFormID);
	void AttachConfigLights(const ObjectREFRParams& a_refParams, const Config::LightData& a_lightData, std::uint32_t a_index);

	void AttachMeshLights(const ObjectREFRParams& a_refParams, const std::string& a_model);

	void AttachLight(const LightCreateParams& a_lightParams, const ObjectREFRParams& a_refParams, RE::NiNode* a_node, std::uint32_t a_index = 0, const RE::NiPoint3& a_point = { 0, 0, 0 });

	bool ReattachLightsImpl(const ObjectREFRParams& a_refParams);

	// members
	std::vector<Config::Format>                  config{};
	FlatMap<std::string, Config::LightDataVec>   gameModels{};
	FlatMap<RE::FormID, Config::LightDataVec>    gameReferences{};
	FlatMap<std::uint32_t, Config::LightDataVec> gameAddonNodes{};

	LockedMap<RE::RefHandle, std::vector<REFR_LIGH>>                         gameRefLights;
	LockedMap<RE::RefHandle, LockedMap<RE::NiNode*, std::vector<REFR_LIGH>>> gameActorLights;
	LockedMap<RE::FormID, MutexGuard<ProcessedLights>>                       processedGameLights;
};
