#pragma once

#include "LightData.h"

class LightManager : public ISingleton<LightManager>
{
public:
	bool ReadConfigs();
	void OnDataLoad();

	void TryAttachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base);
	void TryAttachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base, RE::NiAVObject* a_root);

	void DetachLights(RE::TESObjectREFR* a_ref, bool a_clearData);

	void AddLightsToProcessQueue(RE::TESObjectCELL* a_cell, RE::TESObjectREFR* a_ref);

	void UpdateFlickeringAndConditions(RE::TESObjectCELL* a_cell);
	void UpdateEmittance(RE::TESObjectCELL* a_cell);

	void RemoveLightsFromProcessQueue(RE::TESObjectCELL* a_cell, const RE::ObjectRefHandle& a_handle);

	void ToggleGameFlicker();

	template <class F>
	void ForEachLight(RE::RefHandle handle, F&& func)
	{
		gameLightsData.read([&](auto& map) {
			if (auto it = map.find(handle); it != map.end()) {
				for (auto& lightData : it->second) {
					func(lightData);
				}
			}
		});
	}

private:
	struct Config
	{
		struct MultiModelSet
		{
			Set<std::string>   models;
			AttachLightDataVec lightData;
		};

		struct MultiReferenceSet
		{
			Set<std::string>   references;
			AttachLightDataVec lightData;
		};

		struct MultiAddonSet
		{
			Set<std::uint32_t> addonNodes;
			AttachLightDataVec lightData;
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
			
			std::scoped_lock locker(*_lock);
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

			std::scoped_lock locker(*_lock);
			erase(flickeringLights, a_handle);
			erase(conditionalLights, a_handle);
			erase(emittanceLights, a_handle);
		}

		std::vector<RE::RefHandle> flickeringLights;
		std::vector<RE::RefHandle> emittanceLights;
		float                      lastUpdateTime{ 0.0f };
		std::vector<RE::RefHandle> conditionalLights;

		mutable std::unique_ptr<std::recursive_mutex> _lock{ std::make_unique<std::recursive_mutex>() };
	};

	void TryAttachLightsImpl(const ObjectRefData& a_refData, RE::TESBoundObject* a_object);

	void AttachConfigLights(const ObjectRefData& a_refData, const std::string& a_model, RE::FormID a_baseFormID);
	void AttachConfigLights(const ObjectRefData& a_refData, const AttachLightData& a_attachData, std::uint32_t a_index);

	void AttachMeshLights(const ObjectRefData& a_refData, const std::string& a_model);

	void AttachLight(const LightData& a_lightData, const ObjectRefData& a_refData, RE::NiNode* a_node, std::uint32_t a_index = 0, const RE::NiPoint3& a_point = RE::NiPoint3::Zero());

	// members
	std::vector<Config::Format>            config{};
	Map<std::string, AttachLightDataVec>   gameModels{};
	Map<RE::FormID, AttachLightDataVec>    gameReferences{};
	Map<std::uint32_t, AttachLightDataVec> gameAddonNodes{};

	LockedMap<RE::RefHandle, Set<LightREFRData, boost::hash<LightREFRData>>> gameLightsData;
	Map<RE::FormID, ProcessedLights>                                         processedGameLights;

	bool useGameFlicker{ false };
};
