#include "Manager.h"

bool LightManager::ReadConfigs(bool a_reload)
{
	logger::info("{:*^50}", a_reload ? "RELOAD" : "CONFIG FILES");

	std::filesystem::path dir{ R"(Data\LightPlacer)" };

	if (std::error_code ec; !std::filesystem::exists(dir, ec)) {
		logger::info("Data\\LightPlacer folder not found ({})", ec.message());
		return false;
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

		auto& config = configs[truncPath];
		
		logger::info("{} {}...", a_reload ? "Reloading" : "Reading", path);
		std::string buffer;
		auto        err = glz::read_file_json(config, path, buffer);
		if (err) {
			logger::error("\terror:{}", glz::format_error(err, buffer));
		} else {
			logger::info("\t{} entries", config.size());
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

	const auto count_lights = [](const auto& map) {
		std::size_t total = 0;
		for (const auto& [key, entries] : map) {
			total += entries.size();
		}
		return total;
	};

	logger::info("Models : {} ({} lights)", gameModels.size(), count_lights(gameModels));
	logger::info("FormIDs : {} ({} lights)", gameFormIDs.size(), count_lights(gameFormIDs));

	RE::PlayerCharacter::GetSingleton()->AddEventSink<RE::BGSActorCellEvent>(GetSingleton());
	RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESWaitStopEvent>(GetSingleton());
}

void LightManager::ReloadConfigs()
{
	if (!ReadConfigs(true)) {
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

	constexpr auto make_shared_lights = [](Config::LightEntries& a_lights) {
		Config::LightEntryGroup vec;
		vec.reserve(a_lights.size());
		for (auto& light : a_lights) {
			vec.emplace_back(std::make_shared<const Config::LightEntry>(std::move(light)));
		}
		a_lights.clear();
		return vec;
	};

	for (auto& [path, config] : configs) {
		const auto sharedPath = std::make_shared<const std::string>(path);

		for (auto& multiData : config) {
			std::visit(overload{
						   [&](Config::MultiModelSet& models) {
							   PostProcess(models.lights, sharedPath);
							   if (models.lights.empty()) {
								   return;
							   };
							   const auto shared = make_shared_lights(models.lights);
							   for (auto& str : models.models) {
								   gameModels[str].append_range(shared);
							   }
						   },
						   [&](Config::MultiFormIDSet& formIDs) {
							   PostProcess(formIDs.lights, sharedPath);
							   if (formIDs.lights.empty()) {
								   return;
							   };
							   const auto shared = make_shared_lights(formIDs.lights);
							   for (auto& rawID : formIDs.formIDs) {
								   if (auto formID = RE::GetFormID(rawID); formID != 0) {
									   gameFormIDs[formID].append_range(shared);
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
	configs.clear();
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

	auto srcData = SourceData(SOURCE_TYPE::kRef, a_ref, a_root, a_base);
	if (!srcData.IsValid()) {
		return;
	}

	AttachLightsImpl(srcData, a_base->GetFormID());
}

void LightManager::ReattachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base)
{
	if (!a_ref || a_ref->IsDisabled() || a_ref->IsDeleted() || !a_ref->GetParentCell() || !a_base) {
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

	auto srcData = SourceData(SOURCE_TYPE::kActorWorn, a_ref, a_root, bipObject);
	if (!srcData.IsValid()) {
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

	auto srcData = SourceData(SOURCE_TYPE::kReferenceEffect, ref.get(), root, base);
	if (!srcData.IsValid()) {
		return;
	}
	srcData.miscID = a_effect->effectID;

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

	auto srcData = SourceData(SOURCE_TYPE::kActorMagic, ref, root, ref->GetActorBase(), art->GetAsModelTextureSwap());
	if (!srcData.IsValid()) {
		return;
	}
	srcData.miscID = std::to_underlying(a_actorMagicCaster->castingSource);

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

void LightManager::AttachLightsImpl(const SourceData& a_srcData, RE::FormID a_formID)
{
	SourceAttachData srcAttachData;

	std::vector<Config::PointPlacementPtr> collectedPoints;
	std::vector<Config::NodePlacementPtr>  collectedNodes;

	if (!a_srcData.modelPath.empty()) {
		if (auto it = gameModels.find(a_srcData.modelPath); it != gameModels.end()) {
			if (srcAttachData.Initialize(a_srcData)) {
				for (const auto& entry : it->second) {
					CollectValidLights(srcAttachData, entry, collectedPoints, collectedNodes);
				}
			}
		}
	}

	if (a_formID != 0) {
		if (auto it = gameFormIDs.find(a_formID); it != gameFormIDs.end()) {
			if (srcAttachData.Initialize(a_srcData)) {
				for (const auto& entry : it->second) {
					CollectValidLights(srcAttachData, entry, collectedPoints, collectedNodes);
				}
			}
		}
	}

	ProcessCollectedLights(srcAttachData, collectedPoints, collectedNodes);
}

void LightManager::CollectValidLights(const SourceAttachData& a_srcData, const Config::LightEntryPtr& a_lightEntry, std::vector<Config::PointPlacementPtr>& a_collectedPoints, std::vector<Config::NodePlacementPtr>& a_collectedNodes)
{
	std::visit(overload{
				   [&](const Config::PointEntry& pointEntry) {
					   if (!pointEntry.filter.IsInvalid(a_srcData)) {
						   a_collectedPoints.emplace_back(a_lightEntry, &pointEntry.data);
					   }
				   },
				   [&](const Config::NodeEntry& nodeEntry) {
					   if (!nodeEntry.filter.IsInvalid(a_srcData)) {
						   a_collectedNodes.emplace_back(a_lightEntry, &nodeEntry.data);
					   }
				   } },
		*a_lightEntry);
}

void LightManager::ProcessCollectedLights(const SourceAttachData& a_srcAttachData, const std::vector<Config::PointPlacementPtr>& a_collectedPoints, const std::vector<Config::NodePlacementPtr>& a_collectedNodes)
{
	if (a_collectedPoints.empty() && a_collectedNodes.empty()) {
		return;
	}
	
	if (!a_srcAttachData.root || !a_srcAttachData.attachNode) {
		return;
	}

	std::uint32_t                           LP_INDEX = 0;
	StringMap<std::vector<RE::NiAVObject*>> foundNodes;

	if (!a_collectedNodes.empty()) {
		StringSet nodeNames;
		for (const auto& group : a_collectedNodes) {
			for (const auto& name : group->attacher) {
				nodeNames.emplace(name);
			}
		}
		if (!nodeNames.empty()) {
			RE::BSVisit::TraverseScenegraphObjects(a_srcAttachData.attachNode, [&](RE::NiAVObject* a_obj) {
				if (a_obj && nodeNames.contains(a_obj->name.c_str())) {
					foundNodes[a_obj->name.c_str()].push_back(a_obj);
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});
		}
	}

	auto processLightGroup = [&](auto& groups) {
		for (const auto& group : groups) {
			const auto& [entries, lightDef, path] = *group;

			const LIGH::LightDefinitionPtr lightDefPtr{ group, std::addressof(lightDef) };

			if constexpr (std::is_same_v<std::decay_t<decltype(groups)>, std::vector<Config::PointPlacementPtr>>) {
				for (const auto& [i, point] : std::views::enumerate(entries)) {
					if (auto node = lightDef.GetOrCreateNode(a_srcAttachData.attachNode, point, *path, LP_INDEX)) {
						AttachLight(lightDefPtr, a_srcAttachData, node, *path, LP_INDEX);
					}
					++LP_INDEX;
				}
			} else {
				std::vector<RE::NiAVObject*> nodeVec;
				for (const auto& name : entries) {
					if (auto it = foundNodes.find(name); it != foundNodes.end()) {
						nodeVec.insert(nodeVec.end(), it->second.begin(), it->second.end());
					}
				}
				for (const auto& [i, node] : std::views::enumerate(nodeVec)) {
					if (auto lightNode = lightDef.GetOrCreateNode(a_srcAttachData.attachNode, node, *path, LP_INDEX)) {
						AttachLight(lightDefPtr, a_srcAttachData, lightNode, *path, LP_INDEX);
					}
					++LP_INDEX;
				}
			}
		}
	};

	processLightGroup(a_collectedPoints);
	processLightGroup(a_collectedNodes);
}

void LightManager::AttachLight(const LIGH::LightDefinitionPtr& a_lightDef, const SourceAttachData& a_srcData, RE::NiNode* a_node, const std::string& path, std::uint32_t a_index)
{
	if (!a_node) {
		return;
	}

	const auto name = a_lightDef->GetLightName(a_srcData, path, a_index);
	const auto ref = a_srcData.ref;
	const auto scale = a_srcData.scale;

	auto lightInstance = a_lightDef->data.GenLight(ref.get(), a_node, name, scale);
	if (!lightInstance.bsLight || !lightInstance.niLight) {
		return;
	}

	auto handle = ref->CreateRefHandle().native_handle();
	auto cellFormID = a_srcData.filterIDs[0];

	switch (a_srcData.type) {
	case SOURCE_TYPE::kRef:
		{
			if (ref->Is(RE::FormType::PlacedHazard)) {
				EmplaceLightImpl(gameHazardLights, handle, a_lightDef, lightInstance, ref);
			} else if (ref->AsExplosion()) {
				EmplaceLightImpl(gameExplosionLights, handle, a_lightDef, lightInstance, ref);
			} else {
				EmplaceLightImpl(gameRefLights, handle, a_lightDef, lightInstance, ref);
				lightsToBeUpdated.try_emplace_or_visit(cellFormID, LightsToUpdate(a_lightDef->data, handle), [&](auto& lightsToUpdate) {
					lightsToUpdate.second.emplace(a_lightDef->data, handle);
				});
			}
		}
		break;
	case SOURCE_TYPE::kActorWorn:
		{
			auto updateFunc = [&](auto& map) {
				EmplaceLightImpl(map.second, a_srcData.nodeName, a_lightDef, lightInstance, ref);

				lightsToBeUpdated.try_emplace_or_visit(cellFormID, LightsToUpdate(handle),
					[&](auto& lightsToUpdate) {
						lightsToUpdate.second.emplace(handle);
					});
			};

			gameActorWornLights.try_emplace_and_visit(handle, updateFunc, updateFunc);
		}
		break;
	case SOURCE_TYPE::kActorMagic:
		{
			auto updateFunc = [&](auto& map) {
				EmplaceLightImpl(map.second, a_srcData.miscID, a_lightDef, lightInstance, ref);
			};

			gameActorMagicLights.try_emplace_and_visit(handle, updateFunc, updateFunc);
		}
		break;
	case SOURCE_TYPE::kReferenceEffect:
		EmplaceLightImpl(gameReferenceEffectLights, a_srcData.miscID, a_lightDef, lightInstance, ref);
		break;
	default:
		break;
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
		ForEachValidLight([&](const auto& ref, const auto& nodeName, auto& placedLights) {
			placedLights.UpdateConditions(ref, nodeName, ConditionUpdateFlags::CellTransition);
		});
	}
	lastCellWasInterior = currentCellIsInterior;

	ForEachFXLight([&](auto& placedLights) {
		placedLights.ReattachLights();
	});

	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl LightManager::ProcessEvent(const RE::TESWaitStopEvent* a_event, RE::BSTEventSource<RE::TESWaitStopEvent>*)
{
	if (a_event) {
		ForEachValidLight([&](const auto& ref, const auto& nodeName, auto& placedLights) {
			placedLights.UpdateConditions(ref, nodeName, ConditionUpdateFlags::Waiting);
		});
	}

	return RE::BSEventNotifyControl::kContinue;
}

void LightManager::UpdateLights(const RE::TESObjectCELL* a_cell)
{
	std::vector<std::pair<RE::RefHandle, RE::TESObjectREFRPtr>> refrsToUpdate;

	lightsToBeUpdated.visit(a_cell->GetFormID(), [&](auto& entry) {
		auto& [id, data] = entry;

		erase_if(data.updatingLights, [&](const auto& handle) {
			RE::TESObjectREFRPtr ref;
			if (!RE::LookupReferenceByHandle(handle, ref) || !ref) {
				return true;
			}
			refrsToUpdate.push_back(std::make_pair(handle, std::move(ref)));
			return false;
		});
	});

	PlacedLights::UpdateParams params;
	params.pcPos = RE::PlayerCharacter::GetSingleton()->GetPosition();
	params.delta = RE::BSTimer::GetSingleton()->delta;

	for (const auto& [handle, ref] : refrsToUpdate) {
		if (!ref) {
			continue;
		}

		params.ref = ref.get();

		ForEachLightMutable(ref.get(), handle, [&](const auto& a_nodeName, auto& placedLight) {
			params.nodeName = a_nodeName;
			placedLight.UpdateLightsAndRef(params);
			return true;
		});
	}
}

void LightManager::UpdateEmittance(RE::TESObjectCELL* a_cell)
{
	std::vector<RE::RefHandle> handlesToUpdate;

	lightsToBeUpdated.visit(a_cell->GetFormID(), [&](auto& entry) {
		auto& lights = entry.second.emittanceLights;
		erase_if(lights, [&](const auto& handle) {
			RE::TESObjectREFRPtr ref{};
			if (!RE::LookupReferenceByHandle(handle, ref) || !ref) {
				return true;
			}
			handlesToUpdate.push_back(handle);
			return false;
		});
	});

	for (const auto& handle : handlesToUpdate) {
		gameRefLights.cvisit(handle, [&](const auto& entry) {
			entry.second.UpdateEmittance(a_cell);
		});
	}
}

void LightManager::RemoveLightsFromUpdateQueue(const RE::TESObjectCELL* a_cell, const RE::ObjectRefHandle& a_handle)
{
	if (a_handle.native_handle() == 0) {
		return;
	}

	lightsToBeUpdated.erase_if(a_cell->GetFormID(), [&](auto& map) {
		map.second.erase(a_handle.native_handle());
		return map.second.updatingLights.empty() && map.second.emittanceLights.empty();
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
		                               std::clamp((a_effect->lifetime + MAX_WAIT_TIME - a_effect->age) / MAX_WAIT_TIME, 0.0f, 1.0f) :
		                               std::numeric_limits<float>::max();

		PlacedLights::UpdateParams params;
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
		PlacedLights::UpdateParams params;
		params.ref = actor;
		params.pcPos = RE::PlayerCharacter::GetSingleton()->GetPosition();
		params.delta = a_delta;

		map.second.visit(castingSrc, [&](auto& placedLights) {
			placedLights.second.UpdateLightsAndRef(params);
		});
	});
}

void LightManager::UpdateHazardLights(RE::Hazard* a_hazard)
{
	auto handle = a_hazard->CreateRefHandle().native_handle();

	gameHazardLights.visit(handle, [&](auto& map) {
		PlacedLights::UpdateParams params;
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
		PlacedLights::UpdateParams params;
		params.ref = a_explosion;
		params.pcPos = RE::PlayerCharacter::GetSingleton()->GetPosition();
		params.delta = RE::BSTimer::GetSingleton()->delta;
		map.second.UpdateLightsAndRef(params);
	});
}
