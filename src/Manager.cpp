#include "Manager.h"
#include "SourceData.h"

bool LightManager::ReadConfigs(bool a_reload)
{
	logger::info("{:*^50}", a_reload ? "RELOAD" : "CONFIG FILES");

	std::filesystem::path dir{ R"(Data\LightPlacer)" };

	if (std::error_code ec; !std::filesystem::exists(dir, ec)) {
		logger::info("Data\\LightPlacer folder not found ({})", ec.message());
		return false;
	}

	if (a_reload) {
		configs.clear();
	}

	for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(dir)) {
		if (dirEntry.is_directory() || dirEntry.path().extension() != ".json"sv) {
			continue;
		}
		logger::info("{} {}...", a_reload ? "Reloading" : "Reading", dirEntry.path().string());
		std::string                 buffer;
		std::vector<Config::Format> tmpConfig;
		auto                        err = glz::read_file_json(tmpConfig, dirEntry.path().string(), buffer);
		if (err) {
			logger::error("\terror:{}", glz::format_error(err, buffer));
		} else {
			logger::info("\t{} entries", tmpConfig.size());
			configs.append_range(std::move(tmpConfig));
		}
	}

	return !configs.empty();
}

void LightManager::OnDataLoad()
{
	if (configs.empty()) {
		return;
	}

	ProcessConfigs();

	logger::info("{:*^50}", "RESULTS");

	logger::info("Models : {} entries", gameModels.size());
	logger::info("VisualEffects : {} entries", gameVisualEffects.size());

	RE::PlayerCharacter::GetSingleton()->AddEventSink<RE::BGSActorCellEvent>(GetSingleton());
	RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESWaitStopEvent>(GetSingleton());
}

void LightManager::ReloadConfigs()
{
	ReadConfigs(true);

	if (configs.empty()) {
		return;
	}

	gameModels.clear();
	gameVisualEffects.clear();

	ProcessConfigs();
}

void LightManager::ProcessConfigs()
{
	for (auto& multiData : configs) {
		std::visit(overload{
					   [&](Config::MultiModelSet& models) {
						   PostProcess(models.lights);
						   for (auto& str : models.models) {
							   gameModels[str].append_range(models.lights);
						   }
					   },
					   [&](Config::MultiVisualEffectSet& visualEffects) {
						   PostProcess(visualEffects.lights);
						   for (auto& rawID : visualEffects.visualEffects) {
							   if (auto formID = RE::GetFormID(rawID); formID != 0) {
								   gameVisualEffects[formID].append_range(visualEffects.lights);
							   }
						   }
					   },
					   [&](const Config::MultiAddonSet&) {
					   } },
			multiData);
	}
}

std::vector<RE::TESObjectREFRPtr> LightManager::GetLightAttachedRefs()
{
	std::vector<RE::TESObjectREFRPtr> refs;

	gameRefLights.cvisit_all([&](auto& map) {
		RE::TESObjectREFRPtr ref{};
		RE::LookupReferenceByHandle(map.first, ref);
		if (ref) {
			refs.push_back(ref);
		}
	});

	gameHazardLights.cvisit_all([&](auto& map) {
		RE::TESObjectREFRPtr ref{};
		RE::LookupReferenceByHandle(map.first, ref);
		if (ref) {
			refs.push_back(ref);
		}
	});

	return refs;
}

void LightManager::AddLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base, RE::NiAVObject* a_root)
{
	if (!a_ref || !a_root || !a_base) {
		return;
	}

	auto srcData = std::make_unique<SourceData>(SOURCE_TYPE::kRef, a_ref, a_root, a_base);
	if (!srcData || !srcData->IsValid()) {
		return;
	}

	AttachLightsImpl(srcData);
}

void LightManager::ReattachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base)
{
	if (!a_ref || a_ref->IsDisabled() || a_ref->IsDeleted() || !a_base || a_base->Is(RE::FormType::Light)) {
		return;
	}

	auto handle = a_ref->CreateRefHandle().native_handle();

	gameRefLights.visit(handle, [&](auto& map) {
		map.second.ReattachLights(a_ref);
	});
}

void LightManager::DetachLights(RE::TESObjectREFR* a_ref, bool a_clearData)
{
	auto handle = a_ref->CreateRefHandle().native_handle();

	if (a_ref->IsActor()) {
		gameActorWornLights.erase_if(handle, [&](auto& map) {
			map.second.visit_all([&](auto& nodeMap) {
				nodeMap.second.RemoveLights(a_clearData);
			});
			return a_clearData;
		});
	} else {
		gameRefLights.erase_if(handle, [&](auto& map) {
			map.second.RemoveLights(a_clearData);
			return a_clearData;
		});
	}
}

void LightManager::DetachHazardLights(RE::Hazard* a_hazard)
{
	auto handle = a_hazard->CreateRefHandle().native_handle();

	gameHazardLights.erase_if(handle, [&](auto& map) {
		map.second.RemoveLights(true);
		return true;
	});
}

void LightManager::DetachExplosionLights(RE::Explosion* a_explosion)
{
	auto handle = a_explosion->CreateRefHandle().native_handle();

	gameExplosionLights.erase_if(handle, [&](auto& map) {
		map.second.RemoveLights(true);
		return true;
	});
}

void LightManager::AddWornLights(RE::TESObjectREFR* a_ref, const RE::BSTSmartPointer<RE::BipedAnim>& a_bipedAnim, std::int32_t a_slot, RE::NiAVObject* a_root)
{
	if (!a_ref || !a_root || a_slot == -1) {
		return;
	}

	auto bipedAnim = a_bipedAnim;
	if (!bipedAnim) {
		bipedAnim = a_ref->GetBiped();
	}
	if (!bipedAnim || a_ref->IsPlayerRef() && bipedAnim == a_ref->GetBiped(true)) {
		return;
	}

	const auto& bipObject = bipedAnim->objects[a_slot];
	if (!bipObject.item || bipObject.item->Is(RE::FormType::Light)) {
		return;
	}

	auto srcData = std::make_unique<SourceData>(SOURCE_TYPE::kActorWorn, a_ref, a_root, bipObject);
	if (!srcData || !srcData->IsValid()) {
		return;
	}

	AttachLightsImpl(srcData);
}

void LightManager::ReattachWornLights(const RE::ActorHandle& a_handle) const
{
	auto handle = a_handle.native_handle();

	gameActorWornLights.cvisit(handle, [&](auto& map) {
		map.second.cvisit_all([&](auto& nodeMap) {
			nodeMap.second.ReattachLights();
		});
	});
}

void LightManager::DetachWornLights(const RE::ActorHandle& a_handle, RE::NiAVObject* a_root)
{
	if (!a_root) {
		return;
	}

	auto handle = a_handle.native_handle();

	gameActorWornLights.visit(handle, [&](auto& map) {
		map.second.visit(a_root->name.c_str(), [&](auto& nodeMap) {
			nodeMap.second.RemoveLights(true);
		});
		map.second.erase(a_root->name.c_str());
	});
}

void LightManager::AddTempEffectLights(RE::ReferenceEffect* a_effect, RE::FormID a_effectFormID)
{
	if (!a_effect) {
		return;
	}

	const auto ref = a_effect->target.get();
	const auto root = RE::GetReferenceAttachRoot(a_effect);

	if (!ref || !root) {
		return;
	}

	const auto base = RE::GetReferenceEffectBase(ref, a_effect);
	if (!base) {
		return;
	}

	if (auto invMgr = RE::Inventory3DManager::GetSingleton(); invMgr && invMgr->tempRef == ref.get()) {
		return;
	}

	auto srcData = std::make_unique<SourceData>(SOURCE_TYPE::kTempEffect, ref.get(), root, base);
	if (!srcData || !srcData->IsValid()) {
		return;
	}
	srcData->miscID = a_effect->effectID;

	AttachLightsImpl(srcData, a_effectFormID);
}

void LightManager::ReattachTempEffectLights(RE::ReferenceEffect* a_effect) const
{
	gameVisualEffectLights.cvisit(a_effect->effectID, [&](auto& map) {
		map.second.ReattachLights();
	});
}

void LightManager::DetachTempEffectLights(RE::ReferenceEffect* a_effect, bool a_clearData)
{
	gameVisualEffectLights.erase_if(a_effect->effectID, [&](auto& map) {
		map.second.RemoveLights(a_clearData);
		return a_clearData;
	});
}

void LightManager::AddCastingLights(RE::ActorMagicCaster* a_actorMagicCaster)
{
	const auto& root = RE::GetCastingArtNode(a_actorMagicCaster);
	const auto  ref = a_actorMagicCaster->GetCasterAsActor();
	const auto  art = RE::GetCastingArt(a_actorMagicCaster);
	if (!root || !ref || !art) {
		return;
	}

	auto srcData = std::make_unique<SourceData>(SOURCE_TYPE::kActorMagic, ref, root, ref->GetActorBase(), art->GetAsModelTextureSwap());
	if (!srcData || !srcData->IsValid()) {
		return;
	}
	srcData->miscID = std::to_underlying(a_actorMagicCaster->castingSource);

	AttachLightsImpl(srcData, art->GetFormID());
}

void LightManager::DetachCastingLights(RE::ActorMagicCaster* a_actorMagicCaster)
{
	const auto& root = RE::GetCastingArtNode(a_actorMagicCaster);
	const auto  ref = a_actorMagicCaster->GetCasterAsActor();
	if (!root || !ref) {
		return;
	}

	auto handle = ref->CreateRefHandle().native_handle();
	auto castingSrc = static_cast<std::uint32_t>(a_actorMagicCaster->castingSource);

	gameActorMagicLights.visit(handle, [&](auto& map) {
		map.second.visit(castingSrc, [&](auto& srcMap) {
			srcMap.second.RemoveLights(true);
		});
		map.second.erase(castingSrc);
	});
}

void LightManager::AttachLightsImpl(const std::unique_ptr<SourceData>& a_srcData, RE::FormID a_formID)
{
	std::uint32_t LP_INDEX{ 0 };

	auto srcAttachData = std::make_unique<SourceAttachData>();

	std::vector<Config::PointData> collectedPoints{};
	std::vector<Config::NodeData>  collectedNodes{};

	if (!a_srcData->modelPath.empty()) {
		if (auto it = gameModels.find(a_srcData->modelPath); it != gameModels.end()) {
			if (srcAttachData->Initialize(a_srcData)) {
				for (const auto& data : it->second) {
					CollectValidLights(srcAttachData, data, collectedPoints, collectedNodes);
				}
			}
		}
	}

	if (a_formID != 0) {
		if (auto it = gameVisualEffects.find(a_formID); it != gameVisualEffects.end()) {
			if (srcAttachData->Initialize(a_srcData)) {
				for (const auto& data : it->second) {
					CollectValidLights(srcAttachData, data, collectedPoints, collectedNodes);
				}
			}
		}
	}

	if (!srcAttachData->IsValid()) {
		return;
	}

	for (auto& [points, lightData] : collectedPoints) {
		for (const auto [pointIdx, point] : std::views::enumerate(points)) {
			auto lightPlacerNode = lightData.GetOrCreateNode(srcAttachData->attachNode, point, LP_INDEX);
			if (lightPlacerNode) {
				AttachLight(lightData, srcAttachData, lightPlacerNode, LP_INDEX);
			}
			LP_INDEX++;
		}
	}

	for (auto& [nodes, lightData] : collectedNodes) {
		std::vector<RE::NiAVObject*> nodeVec;
		if (srcAttachData->attachNode) {
			RE::BSVisit::TraverseScenegraphObjects(srcAttachData->attachNode, [&](RE::NiAVObject* a_obj) {
				if (nodes.contains(a_obj->name.c_str())) {
					nodeVec.push_back(a_obj);
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});
		}
		for (const auto [nodeIdx, node] : std::views::enumerate(nodeVec)) {
			auto lightPlacerNode = lightData.GetOrCreateNode(srcAttachData->attachNode, node, LP_INDEX);
			if (lightPlacerNode) {
				AttachLight(lightData, srcAttachData, lightPlacerNode, LP_INDEX);
			}
			LP_INDEX++;
		}
	}
}

void LightManager::CollectValidLights(const std::unique_ptr<SourceAttachData>& a_srcData, const Config::LightSourceData& a_lightData, std::vector<Config::PointData>& a_collectedPoints, std::vector<Config::NodeData>& a_collectedNodes)
{
	std::visit(overload{
				   [&](const Config::FilteredPointData& pointData) {
					   auto& [filter, data] = pointData;
					   if (!filter.IsInvalid(a_srcData)) {
						   a_collectedPoints.push_back(data);
					   }
				   },
				   [&](const Config::FilteredNodeData& nodeData) {
					   auto& [filter, data] = nodeData;
					   if (!filter.IsInvalid(a_srcData)) {
						   a_collectedNodes.push_back(data);
					   }
				   } },
		a_lightData);
}

void LightManager::AttachLight(const LIGH::LightSourceData& a_lightSource, const std::unique_ptr<SourceAttachData>& a_srcData, RE::NiNode* a_node, std::uint32_t a_index)
{
	if (!a_node) {
		return;
	}

	const auto name = LightData::GetLightName(a_srcData, a_lightSource.lightEDID, a_index);
	const auto ref = a_srcData->ref;
	const auto scale = a_srcData->scale;

	if (auto lightDataOutput = a_lightSource.data.GenLight(ref.get(), a_node, name, scale); lightDataOutput.bsLight && lightDataOutput.niLight) {
		auto handle = ref->CreateRefHandle().native_handle();

		switch (a_srcData->type) {
		case SOURCE_TYPE::kRef:
			{
				if (ref->Is(RE::FormType::PlacedHazard)) {
					gameHazardLights.try_emplace_or_visit(handle, ProcessedLights(a_lightSource, lightDataOutput, ref, scale), [&](auto& container) {
						container.second.emplace_back(a_lightSource, lightDataOutput, ref, scale);
					});
				} else if (ref->Is(RE::FormType::Explosion)) {
					gameExplosionLights.try_emplace_or_visit(handle, ProcessedLights(a_lightSource, lightDataOutput, ref, scale), [&](auto& container) {
						container.second.emplace_back(a_lightSource, lightDataOutput, ref, scale);
					});
				} else {
					gameRefLights.try_emplace_or_visit(handle, ProcessedLights(a_lightSource, lightDataOutput, ref, scale), [&](auto& container) {
						container.second.emplace_back(a_lightSource, lightDataOutput, ref, scale);
					});
				}
			}
			break;
		case SOURCE_TYPE::kActorWorn:
			{
				auto updateFunc = [&](auto& map) {
					map.second.try_emplace_or_visit(a_srcData->nodeName, ProcessedLights(a_lightSource, lightDataOutput, ref, scale), [&](auto& container) {
						container.second.emplace_back(a_lightSource, lightDataOutput, ref, scale);
					});
					lightsToBeUpdated.try_emplace_or_visit(a_srcData->filterIDs[0], LightsToUpdate(handle), [&](auto& lightsToUpdate) {
						lightsToUpdate.second.emplace(handle);
					});
				};

				gameActorWornLights.try_emplace_and_visit(handle, updateFunc, updateFunc);
			}
			break;
		case SOURCE_TYPE::kActorMagic:
			{
				auto updateFunc = [&](auto& map) {
					map.second.try_emplace_or_visit(a_srcData->miscID, ProcessedLights(a_lightSource, lightDataOutput, ref, scale), [&](auto& container) {
						container.second.emplace_back(a_lightSource, lightDataOutput, ref, scale);
					});
				};

				gameActorMagicLights.try_emplace_and_visit(handle, updateFunc, updateFunc);
			}
			break;
		case SOURCE_TYPE::kTempEffect:
			{
				gameVisualEffectLights.try_emplace_or_visit(a_srcData->miscID, ProcessedLights(a_lightSource, lightDataOutput, ref, scale), [&](auto& map) {
					map.second.emplace_back(a_lightSource, lightDataOutput, ref, scale);
				});
			}
			break;
		default:
			break;
		}
	}
}

void LightManager::AddLightsToUpdateQueue(const RE::TESObjectCELL* a_cell, RE::TESObjectREFR* a_ref)
{
	auto cellFormID = a_cell->GetFormID();
	auto handle = a_ref->CreateRefHandle().native_handle();
	auto isObject = a_ref->IsNot(RE::FormType::ActorCharacter);

	ForEachLight(a_ref, handle, [&](const auto&, const auto& processedLight) {
		lightsToBeUpdated.try_emplace_or_visit(cellFormID, LightsToUpdate(processedLight, handle, isObject), [&](auto& map) {
			map.second.emplace(processedLight, handle, isObject);
		});
		return true;
	});
}

RE::BSEventNotifyControl LightManager::ProcessEvent(const RE::BGSActorCellEvent* a_event, RE::BSTEventSource<RE::BGSActorCellEvent>*)
{
	if (!a_event || a_event->flags == RE::BGSActorCellEvent::CellFlag::kLeave) {
		return RE::BSEventNotifyControl::kContinue;
	}

	auto cell = RE::TESForm::LookupByID<RE::TESObjectCELL>(a_event->cellID);
	if (!cell) {
		return RE::BSEventNotifyControl::kContinue;
	}

	const bool currentCellIsInterior = cell->IsInteriorCell();
	if (lastCellWasInterior != currentCellIsInterior) {
		ForEachValidLight([&](const auto& ref, const auto& nodeName, auto& processedLights) {
			processedLights.UpdateConditions(ref, nodeName, ConditionUpdateFlags::CellTransition);
		});
	}
	lastCellWasInterior = currentCellIsInterior;

	ForEachFXLight([&](auto& processedLights) {
		processedLights.ReattachLights();
	});

	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl LightManager::ProcessEvent(const RE::TESWaitStopEvent* a_event, RE::BSTEventSource<RE::TESWaitStopEvent>*)
{
	if (a_event) {
		ForEachValidLight([&](const auto& ref, const auto& nodeName, auto& processedLights) {
			processedLights.UpdateConditions(ref, nodeName, ConditionUpdateFlags::Waiting);
		});
	}

	return RE::BSEventNotifyControl::kContinue;
}

void LightManager::UpdateLights(const RE::TESObjectCELL* a_cell)
{
	lightsToBeUpdated.visit(a_cell->GetFormID(), [&](auto& map) {
		const auto pc = RE::PlayerCharacter::GetSingleton();

		ProcessedLights::UpdateParams params;
		params.pcPos = pc->GetPosition();
		params.delta = RE::BSTimer::GetSingleton()->delta;

		std::erase_if(map.second.updatingLights, [&](auto& handle) {
			RE::TESObjectREFRPtr ref{};
			RE::LookupReferenceByHandle(handle, ref);

			if (!ref) {
				return true;
			}

			params.ref = ref.get();

			ForEachLightMutable(ref.get(), handle, [&](const auto& a_nodeName, auto& processedLight) {
				params.nodeName = a_nodeName;
				processedLight.UpdateLightsAndRef(params);
				return true;
			});

			return false;
		});
	});
}

void LightManager::UpdateEmittance(const RE::TESObjectCELL* a_cell)
{
	lightsToBeUpdated.visit(a_cell->GetFormID(), [&](auto& map) {
		std::erase_if(map.second.emittanceLights, [&](const auto& handle) {
			RE::TESObjectREFRPtr ref{};
			RE::LookupReferenceByHandle(handle, ref);

			if (!ref) {
				return true;
			}

			gameRefLights.cvisit(handle, [&](auto& lightsMap) {
				lightsMap.second.UpdateEmittance();
			});

			return false;
		});
	});
}

void LightManager::RemoveLightsFromUpdateQueue(const RE::TESObjectCELL* a_cell, const RE::ObjectRefHandle& a_handle)
{
	lightsToBeUpdated.visit(a_cell->GetFormID(), [&](auto& map) {
		map.second.erase(a_handle.native_handle());
	});
}

void LightManager::UpdateTempEffectLights(RE::ReferenceEffect* a_effect)
{
	gameVisualEffectLights.visit(a_effect->effectID, [&](auto& map) {
		const auto ref = a_effect->target.get();
		if (!ref) {
			return;
		}

		bool singleSequence = false;

		if (auto modelEffect = a_effect->As<RE::ModelReferenceEffect>()) {
			const auto artObj = modelEffect->artObject3D;
			const auto controllers = artObj ? artObj->GetControllers() : nullptr;
			const auto manager = controllers ? controllers->AsNiControllerManager() : nullptr;

			singleSequence = manager && manager->sequenceArray.size() == 1;
		}

		constexpr auto MAX_WAIT_TIME = 3.0f;
		const float    dimFactor = !singleSequence && a_effect->finished ?
		                               (a_effect->lifetime + MAX_WAIT_TIME - a_effect->age) / MAX_WAIT_TIME :
		                               std::numeric_limits<float>::max();

		ProcessedLights::UpdateParams params;
		params.ref = ref.get();
		params.pcPos = RE::PlayerCharacter::GetSingleton()->GetPosition();
		params.delta = RE::BSTimer::GetSingleton()->delta;
		params.dimFactor = dimFactor;

		map.second.UpdateLightsAndRef(params);
	});
}

void LightManager::UpdateCastingLights(RE::ActorMagicCaster* a_actorMagicCaster, float a_delta)
{
	if (a_actorMagicCaster->flags.none(RE::ActorMagicCaster::Flags::kCastingArtAttached)) {
		return;
	}

	const auto& root = RE::GetCastingArtNode(a_actorMagicCaster);
	if (!root) {
		return;
	}

	auto actor = a_actorMagicCaster->GetCasterAsActor();
	if (!actor) {
		return;
	}

	auto handle = actor->CreateRefHandle().native_handle();
	auto castingSrc = std::to_underlying(a_actorMagicCaster->castingSource);

	gameActorMagicLights.visit(handle, [&](auto& map) {
		ProcessedLights::UpdateParams params;
		params.ref = actor;
		params.pcPos = RE::PlayerCharacter::GetSingleton()->GetPosition();
		params.delta = a_delta;

		map.second.visit(castingSrc, [&](auto& processedLights) {
			processedLights.second.UpdateLightsAndRef(params);
		});
	});
}

void LightManager::UpdateHazardLights(RE::Hazard* a_hazard)
{
	auto handle = a_hazard->CreateRefHandle().native_handle();

	gameHazardLights.visit(handle, [&](auto& map) {
		ProcessedLights::UpdateParams params;
		params.ref = a_hazard;
		params.pcPos = RE::PlayerCharacter::GetSingleton()->GetPosition();
		params.delta = RE::BSTimer::GetSingleton()->delta;

		constexpr auto MAX_WAIT_TIME = 3.0f;
		const float    dimFactor = a_hazard->flags.any(RE::Hazard::Flags::kShuttingDown) ?
		                               (a_hazard->lifetime + MAX_WAIT_TIME - a_hazard->age) / MAX_WAIT_TIME :
		                               std::numeric_limits<float>::max();
		params.dimFactor = dimFactor;

		map.second.UpdateLightsAndRef(params);
	});
}

void LightManager::UpdateExplosionLights(RE::Explosion* a_explosion)
{
	auto handle = a_explosion->CreateRefHandle().native_handle();

	gameExplosionLights.visit(handle, [&](auto& map) {
		ProcessedLights::UpdateParams params;
		params.ref = a_explosion;
		params.pcPos = RE::PlayerCharacter::GetSingleton()->GetPosition();
		params.delta = RE::BSTimer::GetSingleton()->delta;
		map.second.UpdateLightsAndRef(params);
	});
}
