#pragma once

#include "ConfigData.h"
#include "LightData.h"
#include "ProcessedLights.h"

struct SourceData;

class LightManager :
	public REX::Singleton<LightManager>,
	public RE::BSTEventSink<RE::BGSActorCellEvent>,
	public RE::BSTEventSink<RE::TESWaitStopEvent>
{
public:
	bool ReadConfigs(bool a_reload = false);
	void OnDataLoad();
	void ReloadConfigs();

	std::vector<RE::TESObjectREFRPtr> GetLightAttachedRefs();

	void AddLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base, RE::NiAVObject* a_root);
	void ReattachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base);
	void DetachLights(RE::TESObjectREFR* a_ref, bool a_clearData);

	void AddWornLights(RE::TESObjectREFR* a_ref, const RE::BSTSmartPointer<RE::BipedAnim>& a_bipedAnim, std::int32_t a_slot, RE::NiAVObject* a_root);
	void ReattachWornLights(const RE::ActorHandle& a_handle) const;
	void DetachWornLights(const RE::ActorHandle& a_handle, RE::NiAVObject* a_root);

	void AddTempEffectLights(RE::ReferenceEffect* a_effect, RE::FormID a_effectFormID);
	void ReattachTempEffectLights(RE::ReferenceEffect* a_effect) const;
	void DetachTempEffectLights(RE::ReferenceEffect* a_effect, bool a_clearData);

	void AddCastingLights(RE::ActorMagicCaster* a_actorMagicCaster);
	void DetachCastingLights(RE::RefAttachTechniqueInput& a_refAttachInput);

	void AddLightsToUpdateQueue(const RE::TESObjectCELL* a_cell, RE::TESObjectREFR* a_ref);
	void UpdateLights(const RE::TESObjectCELL* a_cell);
	void UpdateEmittance(const RE::TESObjectCELL* a_cell);
	void RemoveLightsFromUpdateQueue(const RE::TESObjectCELL* a_cell, const RE::ObjectRefHandle& a_handle);

	void UpdateTempEffectLights(RE::ReferenceEffect* a_effect);
	void UpdateCastingLights(RE::ActorMagicCaster* a_actorMagicCaster, float a_delta);

	template <class F>
	void ForAllLights(F&& func)
	{
		gameRefLights.read_unsafe([&](auto& map) {
			for (auto& [handle, processedLight] : map) {
				func(processedLight);
			}
		});
		gameActorWornLights.read_unsafe([&](auto& map) {
			for (auto& [handle, nodes] : map) {
				nodes.read_unsafe([&](auto& nodeMap) {
					for (auto& [node, processedLight] : nodeMap) {
						func(processedLight);
					}
				});
			}
		});
		gameActorMagicLights.read_unsafe([&](auto& map) {
			for (auto& [node, processedLights] : map) {
				func(processedLights);
			}
		});
		gameVisualEffectLights.read_unsafe([&](auto& map) {
			for (auto& [effectID, processedLights] : map) {
				func(processedLights);
			}
		});
	}

	template <class F>
	void ForEachLight(RE::TESObjectREFR* a_ref, RE::RefHandle a_handle, F&& func)
	{
		if (a_ref->IsActor()) {
			gameActorWornLights.read_unsafe([&](auto& map) {
				if (auto it = map.find(a_handle); it != map.end()) {
					it->second.read_unsafe([&](auto& nodeMap) {
						for (auto& [nodeName, processedLights] : nodeMap) {
							if (func(nodeName, processedLights)) {
								return;
							}
						}
					});
				}
			});
		} else {
			gameRefLights.read_unsafe([&](auto& map) {
				if (auto it = map.find(a_handle); it != map.end()) {
					if (func(""sv, it->second)) {
						return;
					}
				}
			});
		}
	}

	template <class F>
	void ForEachValidLight(F&& func)
	{
		gameRefLights.read_unsafe([&](auto& map) {
			for (auto& [handle, processedLights] : map) {
				RE::TESObjectREFRPtr ref{};
				RE::LookupReferenceByHandle(handle, ref);
				if (!ref) {
					continue;
				}
				func(ref.get(), ""sv, processedLights);
			}
		});

		gameActorWornLights.read_unsafe([&](auto& map) {
			for (auto& [handle, nodes] : map) {
				RE::TESObjectREFRPtr ref{};
				RE::LookupReferenceByHandle(handle, ref);
				if (!ref) {
					continue;
				}
				nodes.read_unsafe([&](auto& nodeMap) {
					for (auto& [nodeName, processedLights] : nodeMap) {
						func(ref.get(), nodeName, processedLights);
					}
				});
			}
		});
	}

	template <class F>
	void ForEachFXLight(F&& func)
	{
		gameVisualEffectLights.read_unsafe([&](auto& map) {
			for (auto& [effectID, processedLights] : map) {
				func(processedLights);
			}
		});
	}

private:
	void ProcessConfigs();

	RE::BSEventNotifyControl ProcessEvent(const RE::BGSActorCellEvent* a_event, RE::BSTEventSource<RE::BGSActorCellEvent>*) override;
	RE::BSEventNotifyControl ProcessEvent(const RE::TESWaitStopEvent* a_event, RE::BSTEventSource<RE::TESWaitStopEvent>*) override;

	void AttachLightsImpl(const std::unique_ptr<SourceData>& a_srcData, RE::FormID a_formID = 0);
	void CollectValidLights(const std::unique_ptr<SourceAttachData>& a_srcData, const Config::LightSourceData& a_lightData, std::vector<Config::PointData>& a_collectedPoints, std::vector<Config::NodeData>& a_collectedNodes);

	void AttachLight(const LIGH::LightSourceData& a_lightSource, const std::unique_ptr<SourceAttachData>& a_srcData, RE::NiNode* a_node, std::uint32_t a_index = 0);

	// members
	std::vector<Config::Format>                 configs;
	StringMap<Config::LightSourceVec>           gameModels;
	FlatMap<RE::FormID, Config::LightSourceVec> gameVisualEffects;

	LockedMap<RE::RefHandle, ProcessedLights>                         gameRefLights;
	LockedMap<RE::RefHandle, LockedMap<std::string, ProcessedLights>> gameActorWornLights;     // nodeName (armor node on attach isn't same ptr on detach)
	LockedNiPtrMap<RE::NiNode, ProcessedLights>                       gameActorMagicLights;    // castingArt3D
	LockedMap<std::uint32_t, ProcessedLights>                         gameVisualEffectLights;  // effectID

	LockedMap<RE::FormID, MutexGuard<LightsToUpdate>> lightsToBeUpdated;
	std::optional<bool>                               lastCellWasInterior;
};
