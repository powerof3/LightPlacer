#pragma once

#include "LightData.h"

class LightManager : public ISingleton<LightManager>
{
public:
	bool ReadConfigs();
	void OnDataLoad();

	void TryAttachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base);
	void TryAttachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base, RE::NiAVObject* a_root);
	void TryAttachLights(RE::TESObjectREFR* a_ref, RE::BSTSmartPointer<RE::BipedAnim>& a_bipedAnim, std::int32_t a_slot, RE::NiAVObject* a_root);
	void DetachLights(RE::TESObjectREFR* a_ref, bool a_clearData);

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
	struct Config
	{
		struct MultiModelSet
		{
			FlatSet<std::string> models;
			AttachLightDataVec   lightData;
		};

		struct MultiReferenceSet
		{
			FlatSet<std::string> references;
			AttachLightDataVec   lightData;
		};

		struct MultiAddonSet
		{
			FlatSet<std::uint32_t> addonNodes;
			AttachLightDataVec     lightData;
		};

		using Format = std::variant<MultiModelSet, MultiReferenceSet, MultiAddonSet>;
	};

	struct ProcessedLights
	{
		ProcessedLights() = default;

		void emplace(const LightREFRData& a_data, RE::RefHandle a_handle)
		{
			constexpr auto emplace = [](auto& container, RE::RefHandle handle) {
				if (auto it = std::ranges::find(container, handle); it == container.end()) {
					container.push_back(handle);
				}
			};

			if (!a_data.light->GetNoFlicker()) {
				emplace(flickeringLights, a_handle);
			}
			if (a_data.emittance) {
				emplace(emittanceLights, a_handle);
			}
			if (a_data.conditions) {
				emplace(conditionalLights, a_handle);
			}
		}

		void erase(RE::RefHandle a_handle)
		{
			constexpr auto erase = [](auto& container, RE::RefHandle handle) {
				if (auto it = std::ranges::find(container, handle); it != container.end()) {
					container.erase(it);
				}
			};

			erase(flickeringLights, a_handle);
			erase(conditionalLights, a_handle);
			erase(emittanceLights, a_handle);
			erase(emittanceLights, a_handle);
		}

		std::vector<RE::RefHandle> flickeringLights;
		std::vector<RE::RefHandle> emittanceLights;
		float                      lastUpdateTime{ 0.0f };
		std::vector<RE::RefHandle> conditionalLights;
	};

	void TryAttachLightsImpl(const ObjectRefData& a_refData, RE::TESBoundObject* a_object, RE::TESModel* a_model);

	void AttachConfigLights(const ObjectRefData& a_refData, const std::string& a_model, RE::FormID a_baseFormID);
	void AttachConfigLights(const ObjectRefData& a_refData, const AttachLightData& a_attachData, std::uint32_t a_index);

	void AttachMeshLights(const ObjectRefData& a_refData, const std::string& a_model);

	void AttachLight(const LightData& a_lightData, const ObjectRefData& a_refData, RE::NiNode* a_node, std::uint32_t a_index = 0, const RE::NiPoint3& a_point = RE::NiPoint3::Zero());

	// members
	std::vector<Config::Format>                config{};
	FlatMap<std::string, AttachLightDataVec>   gameModels{};
	FlatMap<RE::FormID, AttachLightDataVec>    gameReferences{};
	FlatMap<std::uint32_t, AttachLightDataVec> gameAddonNodes{};

	LockedMap<RE::RefHandle, NodeSet<LightREFRData>>                         gameRefLights;
	LockedMap<RE::RefHandle, LockedMap<RE::NiNode*, NodeSet<LightREFRData>>> gameActorLights;
	LockedMap<RE::FormID, MutexGuard<ProcessedLights>>                       processedGameLights;
};
