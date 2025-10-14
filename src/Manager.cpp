#include "Manager.h"

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

	clib_util::Timer timer;
	timer.start();

	for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(dir)) {
		if (dirEntry.is_directory() || dirEntry.path().extension() != ".json"sv) {
			continue;
		}

		std::string path = dirEntry.path().string();
		std::string truncPath = path.substr(strlen("Data\\LightPlacer\\"));
		truncPath.erase(truncPath.size() - strlen(".json"));

		logger::info("{} {}...", a_reload ? "Reloading" : "Reading", path);
		std::string buffer;
		auto        err = glz::read_file_json(configs[truncPath], path, buffer);
		if (err) {
			logger::error("\terror:{}", glz::format_error(err, buffer));
		} else {
			logger::info("\t{} entries", configs[truncPath].size());
		}
	}

	timer.stop();
	logger::info("Time taken: {}ms", timer.duration_ms());

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
	logger::info("FormIDs : {} entries", gameFormIDs.size());

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
	gameFormIDs.clear();

	ProcessConfigs();
}

void LightManager::ProcessConfigs()
{
	logger::info("{:*^50}", "PROCESSING");

	clib_util::Timer timer;
	timer.start();

	for (auto& [path, config] : configs) {
		for (auto& multiData : config) {
			std::visit(overload{
						   [&](Config::MultiModelSet& models) {
							   PostProcess(models.lights, path);
							   for (auto& str : models.models) {
								   gameModels[str].append_range(models.lights);
							   }
						   },
						   [&](Config::MultiFormIDSet& formIDs) {
							   PostProcess(formIDs.lights, path);
							   for (auto& rawID : formIDs.formIDs) {
								   if (auto formID = RE::GetFormID(rawID); formID != 0) {
									   gameFormIDs[formID].append_range(formIDs.lights);
								   }
							   }
						   },
						   [&](const Config::MultiAddonSet&) {
						   } },
				multiData);
		}
	}

	timer.stop();
	logger::info("Processing time taken: {}ms", timer.duration_ms());
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

	AttachLightsImpl(srcData, a_base->GetFormID());
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

	AttachLightsImpl(srcData, bipObject.item->GetFormID());
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

void LightManager::AddReferenceEffectLights(RE::ReferenceEffect* a_effect, RE::FormID a_effectFormID)
{
	if (!a_effect || a_effectFormID == 0) {
		return;
	}

	const auto ref = a_effect->target.get();
	if (!ref) {
		return;
	}

	auto root = RE::GetReferenceAttachRoot(a_effect);
	if (!root) {
		return;
	}

	if (ref->IsPlayerRef() && !RE::PlayerCharacter::GetSingleton()->Is3rdPersonVisible()) {
		auto thirdPersonRoot = ref->Get3D(false) ? ref->Get3D(false)->GetObjectByName(root->name) : nullptr;
		if (thirdPersonRoot) {
			root = thirdPersonRoot;
		}
	}

	const auto base = RE::GetReferenceEffectBase(ref, a_effect);
	if (!base) {
		return;
	}

	if (auto invMgr = RE::Inventory3DManager::GetSingleton(); invMgr && invMgr->tempRef == ref.get()) {
		return;
	}

	auto srcData = std::make_unique<SourceData>(SOURCE_TYPE::kReferenceEffect, ref.get(), root, base);
	if (!srcData || !srcData->IsValid()) {
		return;
	}
	srcData->miscID = a_effect->effectID;

	AttachLightsImpl(srcData, a_effectFormID);
}

void LightManager::ReattachReferenceEffectLights(RE::ReferenceEffect* a_effect) const
{
	gameReferenceEffectLights.cvisit(a_effect->effectID, [&](auto& map) {
		map.second.ReattachLights();
	});
}

void LightManager::DetachReferenceEffectLights(RE::ReferenceEffect* a_effect, bool a_clearData)
{
	gameReferenceEffectLights.erase_if(a_effect->effectID, [&](auto& map) {
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

void LightManager::AttachLightsImpl(const SourceDataPtr& a_srcData, RE::FormID a_formID)
{
	if (!a_srcData) {
		return;
	}

	auto srcAttachData = std::make_shared<SourceAttachData>();
	if (!srcAttachData) {
		return;
	}

	std::vector<Config::PointData> collectedPoints;
	std::vector<Config::NodeData>  collectedNodes;

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
		if (auto it = gameFormIDs.find(a_formID); it != gameFormIDs.end()) {
			if (srcAttachData->Initialize(a_srcData)) {
				for (const auto& data : it->second) {
					CollectValidLights(srcAttachData, data, collectedPoints, collectedNodes);
				}
			}
		}
	}

	if (collectedPoints.empty() && collectedNodes.empty()) {
		return;
	}

	SKSE::GetTaskInterface()->AddTask([points = std::move(collectedPoints), nodes = std::move(collectedNodes), srcAttachData, this]() mutable {
		if (!srcAttachData->root || !srcAttachData->attachNode) {
			return;
		}

		std::uint32_t LP_INDEX = 0;

		auto processLightGroup = [&](auto& groups) {
			for (auto& [entries, lightData, path] : groups) {
				if constexpr (std::is_same_v<std::decay_t<decltype(groups)>, std::vector<Config::PointData>>) {
					for (const auto& [i, point] : std::views::enumerate(entries)) {
						if (auto node = lightData.GetOrCreateNode(srcAttachData->attachNode, point, path, LP_INDEX)) {
							AttachLight(lightData, srcAttachData, node, path, LP_INDEX);
						}
						++LP_INDEX;
					}
				} else {
					std::vector<RE::NiAVObject*> nodeVec;
					if (srcAttachData->attachNode) {
						RE::BSVisit::TraverseScenegraphObjects(srcAttachData->attachNode.get(), [&](RE::NiAVObject* a_obj) {
							if (entries.contains(a_obj->name.c_str())) {
								nodeVec.push_back(a_obj);
							}
							return RE::BSVisit::BSVisitControl::kContinue;
						});
					}
					for (const auto& [i, node] : std::views::enumerate(nodeVec)) {
						if (auto lightNode = lightData.GetOrCreateNode(srcAttachData->attachNode, node, path, LP_INDEX)) {
							AttachLight(lightData, srcAttachData, lightNode, path, LP_INDEX);
						}
						++LP_INDEX;
					}
				}
			}
		};

		processLightGroup(points);
		processLightGroup(nodes);
	});
}

void LightManager::CollectValidLights(const SourceAttachDataPtr& a_srcData, const Config::LightSourceData& a_lightData, std::vector<Config::PointData>& a_collectedPoints, std::vector<Config::NodeData>& a_collectedNodes)
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

void LightManager::AttachLight(const LIGH::LightSourceData& a_lightSource, const SourceAttachDataPtr& a_srcData, RE::NiNode* a_node, const std::string& path, std::uint32_t a_index)
{
	if (!a_node) {
		return;
	}

	const auto name = a_lightSource.GetLightName(a_srcData, path, a_index);
	const auto ref = a_srcData->ref;
	const auto scale = a_srcData->scale;

	if (auto lightDataOutput = a_lightSource.data.GenLight(ref.get(), a_node, name, scale); lightDataOutput.bsLight && lightDataOutput.niLight) {
		auto handle = ref->CreateRefHandle().native_handle();
		auto cellFormID = a_srcData->filterIDs[0];

		switch (a_srcData->type) {
		case SOURCE_TYPE::kRef:
			{
				if (ref->Is(RE::FormType::PlacedHazard)) {
					gameHazardLights.try_emplace_or_visit(handle, ProcessedLights(a_lightSource, lightDataOutput, ref), [&](auto& container) {
						container.second.emplace_back(a_lightSource, lightDataOutput, ref);
					});
				} else if (ref->AsExplosion()) {
					gameExplosionLights.try_emplace_or_visit(handle, ProcessedLights(a_lightSource, lightDataOutput, ref), [&](auto& container) {
						container.second.emplace_back(a_lightSource, lightDataOutput, ref);
					});
				} else {
					gameRefLights.try_emplace_or_visit(handle, ProcessedLights(a_lightSource, lightDataOutput, ref), [&](auto& container) {
						container.second.emplace_back(a_lightSource, lightDataOutput, ref);
					});
					lightsToBeUpdated.try_emplace_or_visit(cellFormID, LightsToUpdate(a_lightSource.data, handle), [&](auto& lightsToUpdate) {
						lightsToUpdate.second.emplace(a_lightSource.data, handle);
					});
				}
			}
			break;
		case SOURCE_TYPE::kActorWorn:
			{
				auto updateFunc = [&](auto& map) {
					map.second.try_emplace_or_visit(a_srcData->nodeName, ProcessedLights(a_lightSource, lightDataOutput, ref), [&](auto& container) {
						container.second.emplace_back(a_lightSource, lightDataOutput, ref);
					});
					lightsToBeUpdated.try_emplace_or_visit(cellFormID, LightsToUpdate(handle), [&](auto& lightsToUpdate) {
						lightsToUpdate.second.emplace(handle);
					});
				};

				gameActorWornLights.try_emplace_and_visit(handle, updateFunc, updateFunc);
			}
			break;
		case SOURCE_TYPE::kActorMagic:
			{
				auto updateFunc = [&](auto& map) {
					map.second.try_emplace_or_visit(a_srcData->miscID, ProcessedLights(a_lightSource, lightDataOutput, ref), [&](auto& container) {
						container.second.emplace_back(a_lightSource, lightDataOutput, ref);
					});
				};

				gameActorMagicLights.try_emplace_and_visit(handle, updateFunc, updateFunc);
			}
			break;
		case SOURCE_TYPE::kReferenceEffect:
			{
				gameReferenceEffectLights.try_emplace_or_visit(a_srcData->miscID, ProcessedLights(a_lightSource, lightDataOutput, ref), [&](auto& map) {
					map.second.emplace_back(a_lightSource, lightDataOutput, ref);
				});
			}
			break;
		default:
			break;
		}
	}
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

void LightManager::UpdateEmittance(RE::TESObjectCELL* a_cell)
{
	lightsToBeUpdated.visit(a_cell->GetFormID(), [&](auto& map) {
		std::erase_if(map.second.emittanceLights, [&](const auto& handle) {
			RE::TESObjectREFRPtr ref{};
			RE::LookupReferenceByHandle(handle, ref);

			if (!ref) {
				return true;
			}

			gameRefLights.cvisit(handle, [&](auto& lightsMap) {
				lightsMap.second.UpdateEmittance(a_cell);
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

void LightManager::UpdateReferenceEffectLights(RE::ReferenceEffect* a_effect)
{
	gameReferenceEffectLights.visit(a_effect->effectID, [&](auto& map) {
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
