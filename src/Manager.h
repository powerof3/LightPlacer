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
	void DetachHazardLights(RE::Hazard* a_hazard);
	void DetachExplosionLights(RE::Explosion* a_explosion);

	void AddWornLights(RE::TESObjectREFR* a_ref, const RE::BSTSmartPointer<RE::BipedAnim>& a_bipedAnim, std::int32_t a_slot, RE::NiAVObject* a_root);
	void ReattachWornLights(const RE::ActorHandle& a_handle) const;
	void DetachWornLights(const RE::ActorHandle& a_handle, RE::NiAVObject* a_root);

	void AddTempEffectLights(RE::ReferenceEffect* a_effect, RE::FormID a_effectFormID);
	void ReattachTempEffectLights(RE::ReferenceEffect* a_effect) const;
	void DetachTempEffectLights(RE::ReferenceEffect* a_effect, bool a_clearData);

	void AddCastingLights(RE::ActorMagicCaster* a_actorMagicCaster);
	void DetachCastingLights(RE::ActorMagicCaster* a_actorMagicCaster);

	void AddLightsToUpdateQueue(const RE::TESObjectCELL* a_cell, RE::TESObjectREFR* a_ref);
	void UpdateLights(const RE::TESObjectCELL* a_cell);
	void UpdateEmittance(const RE::TESObjectCELL* a_cell);
	void RemoveLightsFromUpdateQueue(const RE::TESObjectCELL* a_cell, const RE::ObjectRefHandle& a_handle);

	void UpdateTempEffectLights(RE::ReferenceEffect* a_effect);
	void UpdateCastingLights(RE::ActorMagicCaster* a_actorMagicCaster, float a_delta);
	void UpdateHazardLights(RE::Hazard* a_hazard);
	void UpdateExplosionLights(RE::Explosion* a_explosion);

	template <class F>
	void ForAllLights(F&& func)
	{
		gameRefLights.cvisit_all([&](auto& map) {
			func(map.second);
		});
		gameHazardLights.cvisit_all([&](auto& map) {
			func(map.second);
		});
		gameExplosionLights.cvisit_all([&](auto& map) {
			func(map.second);
		});
		gameActorWornLights.cvisit_all([&](auto& map) {
			map.second.cvisit_all([&](auto& nodeMap) {
				func(nodeMap.second);
			});
		});
		gameActorMagicLights.cvisit_all([&](auto& map) {
			map.second.cvisit_all([&](auto& srcMap) {
				func(srcMap.second);
			});
		});
		gameVisualEffectLights.cvisit_all([&](auto& map) {
			func(map.second);
		});
	}

	template <class F>
	void ForEachLight(RE::TESObjectREFR* a_ref, RE::RefHandle a_handle, F&& func)
	{
		if (a_ref->IsActor()) {
			gameActorWornLights.cvisit(a_handle, [&](auto& map) {
				map.second.cvisit_while([&](auto& nodeMap) {
					return func(nodeMap.first, nodeMap.second);
				});
			});
		} else {
			gameRefLights.cvisit(a_handle, [&](auto& map) {
				return func(""sv, map.second);
			});
		}
	}

	template <class F>
	void ForEachLightMutable(RE::TESObjectREFR* a_ref, RE::RefHandle a_handle, F&& func)
	{
		if (a_ref->IsActor()) {
			gameActorWornLights.visit(a_handle, [&](auto& map) {
				map.second.visit_while([&](auto& nodeMap) {
					return func(nodeMap.first, nodeMap.second);
				});
			});
		} else {
			gameRefLights.visit(a_handle, [&](auto& map) {
				return func(""sv, map.second);
			});
		}
	}

	template <class F>
	void ForEachValidLight(F&& func)
	{
		gameRefLights.visit_all([&](auto& map) {
			RE::TESObjectREFRPtr ref{};
			RE::LookupReferenceByHandle(map.first, ref);
			if (ref) {
				func(ref.get(), ""sv, map.second);
			}
		});

		gameActorWornLights.visit_all([&](auto& map) {
			RE::TESObjectREFRPtr ref{};
			RE::LookupReferenceByHandle(map.first, ref);
			if (ref) {
				map.second.visit_all([&](auto& nodeMap) {
					func(ref.get(), nodeMap.first, nodeMap.second);
				});
			}
		});
	}

	template <class F>
	void ForEachFXLight(F&& func)
	{
		gameVisualEffectLights.cvisit_all([&](auto& map) {
			func(map.second);
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

	LockedMap<RE::RefHandle, ProcessedLights>                           gameRefLights;
	LockedMap<RE::RefHandle, LockedMap<std::string, ProcessedLights>>   gameActorWornLights;     // nodeName (armor node on attach isn't same ptr on detach)
	LockedMap<RE::RefHandle, LockedMap<std::uint32_t, ProcessedLights>> gameActorMagicLights;    // magicNodeName
	LockedMap<std::uint32_t, ProcessedLights>                           gameVisualEffectLights;  // effectID
	LockedMap<RE::RefHandle, ProcessedLights>                           gameHazardLights;
	LockedMap<RE::RefHandle, ProcessedLights>                           gameExplosionLights;

	LockedMap<RE::FormID, LightsToUpdate> lightsToBeUpdated;
	std::optional<bool>                   lastCellWasInterior;
};
