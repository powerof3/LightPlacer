#include "Manager.h"

bool LightManager::ReadConfigs(bool a_reload)
{
	logger::info("{:*^50}", a_reload ? "RELOAD" : "CONFIG FILES");

	std::filesystem::path dir{ "Data\\LightPlacer" };

	std::error_code ec;
	if (!std::filesystem::exists(dir, ec)) {
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

	flickeringDistanceSq = std::powf(RE::GetINISetting("fFlickeringLightDistance:General")->GetFloat(), 2);

	logger::info("{:*^50}", "RESULTS");

	logger::info("Models : {} entries", gameModels.size());
	logger::info("VisualEffects : {} entries", gameVisualEffects.size());
	logger::info("AddonNodes : {} entries", gameAddonNodes.size());
}

void LightManager::ReloadConfigs()
{
	ReadConfigs(true);

	if (configs.empty()) {
		return;
	}

	gameModels.clear();
	gameAddonNodes.clear();
	gameVisualEffects.clear();

	ProcessConfigs();
}

std::vector<RE::TESObjectREFRPtr> LightManager::GetLightAttachedRefs()
{
	std::vector<RE::TESObjectREFRPtr> refs;

	gameRefLights.read_unsafe([&](auto& map) {
		for (auto& [handle, lightDataVec] : map) {
			RE::TESObjectREFRPtr ref{};
			RE::LookupReferenceByHandle(handle, ref);
			if (ref) {
				refs.push_back(ref);
			}
		}
	});

	return refs;
}

void LightManager::AddLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base, RE::NiAVObject* a_root)
{
	if (!a_ref || !a_root || !a_base) {
		return;
	}

	SourceData srcData(SOURCE_TYPE::kRef, a_ref, a_root, a_base);
	if (!srcData.IsValid()) {
		return;
	}

	AttachLightsImpl(srcData);
}

void LightManager::ReattachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base)
{
	if (!a_ref || !a_ref->Get3D() || !a_base || a_base->Is(RE::FormType::Light)) {
		return;
	}

	SourceData srcData(SOURCE_TYPE::kRef, a_ref, a_base);
	if (!srcData.IsValid()) {
		return;
	}

	ReattachLightsImpl(srcData);
}

void LightManager::DetachLights(RE::TESObjectREFR* a_ref, bool a_clearData)
{
	auto handle = a_ref->CreateRefHandle().native_handle();

	if (RE::IsActor(a_ref)) {
		gameActorWornLights.write([&](auto& map) {
			if (auto it = map.find(handle); it != map.end()) {
				it->second.write([&](auto& nodeMap) {
					for (auto& [node, lightRefrDataVec] : nodeMap) {
						for (auto& lightRefrData : lightRefrDataVec) {
							lightRefrData.RemoveLight(a_clearData);
						}
					}
				});
				if (a_clearData) {
					map.erase(it);
				}
			}
		});
	} else {
		gameRefLights.write([&](auto& map) {
			if (auto it = map.find(handle); it != map.end()) {
				for (const auto& lightRefrData : it->second) {
					lightRefrData.RemoveLight(a_clearData);
				}
				if (a_clearData) {
					map.erase(it);
				}
			}
		});
	}
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

	SourceData srcData(SOURCE_TYPE::kActorWorn, a_ref, a_root, bipObject);
	if (!srcData.IsValid()) {
		return;
	}

	AttachLightsImpl(srcData);
}

void LightManager::ReattachWornLights(const RE::ActorHandle& a_handle)
{
	auto handle = a_handle.native_handle();

	gameActorWornLights.read([&](auto& map) {
		if (auto it = map.find(handle); it != map.end()) {
			it->second.read([&](auto& nodeMap) {
				for (auto& [node, lightDataVec] : nodeMap) {
					for (auto& lightData : lightDataVec) {
						lightData.ReattachLight();
					}
				}
			});
		}
	});
}

void LightManager::DetachWornLights(const RE::ActorHandle& a_handle, RE::NiAVObject* a_root)
{
	if (!a_root) {
		return;
	}

	auto handle = a_handle.native_handle();

	gameActorWornLights.write([&](auto& map) {
		if (auto it = map.find(handle); it != map.end()) {
			it->second.write([&](auto& nodeMap) {
				if (auto nIt = nodeMap.find(a_root->name.c_str()); nIt != nodeMap.end()) {
					for (auto& lightData : nIt->second) {
						lightData.RemoveLight(true);
					}
					nodeMap.erase(nIt);
				}
			});
		}
	});
}

void LightManager::AddTempEffectLights(RE::ReferenceEffect* a_effect, RE::FormID a_effectFormID)
{
	if (!a_effect || a_effectFormID == 0) {
		return;
	}

	const auto ref = a_effect->target.get();
	const auto root = a_effect->GetAttachRoot();

	if (!ref || !root) {
		return;
	}

	if (auto invMgr = RE::Inventory3DManager::GetSingleton(); invMgr && invMgr->tempRef == ref.get()) {
		return;
	}

	const auto base = RE::GetReferenceEffectBase(ref, a_effect);
	if (!base) {
		return;
	}

	SourceData srcData(SOURCE_TYPE::kTempEffect, ref.get(), root, base);
	if (!srcData.IsValid()) {
		return;
	}
	srcData.effectID = a_effect->effectID;

	if (auto it = gameVisualEffects.find(a_effectFormID); it != gameVisualEffects.end()) {
		for (const auto [index, data] : std::views::enumerate(it->second)) {
			AttachConfigLights(srcData, data, static_cast<std::uint32_t>(index));
		}
	}

	if (base->Is(RE::FormType::ArtObject)) {
		if (auto it = gameModels.find(srcData.modelPath); it != gameModels.end()) {
			for (const auto [index, data] : std::views::enumerate(it->second)) {
				AttachConfigLights(srcData, data, static_cast<std::uint32_t>(index));
			}
		}
	}
}

void LightManager::ReattachTempEffectLights(RE::ReferenceEffect* a_effect)
{
	gameVisualEffectLights.read([&](auto& map) {
		if (auto it = map.find(a_effect->effectID); it != map.end()) {
			for (auto& lightData : it->second.lights) {
				lightData.ReattachLight();
			}
		}
	});
}

void LightManager::DetachTempEffectLights(RE::ReferenceEffect* a_effect, bool a_clear)
{
	gameVisualEffectLights.write([&](auto& map) {
		if (auto it = map.find(a_effect->effectID); it != map.end()) {
			for (auto& lightData : it->second.lights) {
				lightData.RemoveLight(a_clear);
			}
			if (a_clear) {
				map.erase(it);
			}
		}
	});
}

void LightManager::AddCastingLights(RE::ActorMagicCaster* a_actorMagicCaster)
{
	const auto& root = a_actorMagicCaster->castingArtData.attachedArt;

	if (!root) {
		return;
	}

	if (gameActorMagicLights.read([&](const auto& map) { return map.contains(root); })) {
		return;
	}

	const auto ref = a_actorMagicCaster->GetCasterAsActor();
	const auto art = RE::GetCastingArt(a_actorMagicCaster);
	if (!ref || !art) {
		return;
	}

	SourceData srcData(SOURCE_TYPE::kActorMagic, ref, root.get(), ref->GetActorBase(), art->GetAsModelTextureSwap());
	if (!srcData.IsValid()) {
		return;
	}

	if (auto it = gameVisualEffects.find(art->GetFormID()); it != gameVisualEffects.end()) {
		for (const auto [index, data] : std::views::enumerate(it->second)) {
			AttachConfigLights(srcData, data, static_cast<std::uint32_t>(index));
		}
	}

	if (auto it = gameModels.find(srcData.modelPath); it != gameModels.end()) {
		for (const auto [index, data] : std::views::enumerate(it->second)) {
			AttachConfigLights(srcData, data, static_cast<std::uint32_t>(index));
		}
	}
}

void LightManager::DetachCastingLights(RE::RefAttachTechniqueInput& a_refAttachInput)
{
	if (!a_refAttachInput.attachedArt) {
		return;
	}

	gameActorMagicLights.write([&](auto& map) {
		if (auto it = map.find(a_refAttachInput.attachedArt); it != map.end()) {
			for (auto& lightData : it->second.lights) {
				lightData.RemoveLight(true);
			}
			map.erase(it);
		}
	});
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
					   [&](Config::MultiAddonSet& addonNodes) {
						   PostProcess(addonNodes.lights);
						   for (auto& idx : addonNodes.addonNodes) {
							   gameAddonNodes[idx].append_range(addonNodes.lights);
						   }
					   } },
			multiData);
	}
}

void LightManager::AttachLightsImpl(const SourceData& a_srcData)
{
	std::int32_t LP_INDEX = 0;

	if (auto mIt = gameModels.find(a_srcData.modelPath); mIt != gameModels.end()) {
		for (const auto [index, data] : std::views::enumerate(mIt->second)) {
			AttachConfigLights(a_srcData, data, static_cast<std::uint32_t>(index));
			LP_INDEX++;
		}
	}

	RE::BSVisit::TraverseScenegraphObjects(a_srcData.root, [&](RE::NiAVObject* a_obj) {
		if (auto addonNode = netimmerse_cast<RE::BSValueNode*>(a_obj)) {
			if (auto it = gameAddonNodes.find(addonNode->value); it != gameAddonNodes.end()) {
				for (const auto& [filter, lightData] : it->second) {
					if (!filter.IsInvalid(a_srcData)) {
						if (auto lightPlacerNode = lightData.GetOrCreateNode(a_srcData.root, addonNode, LP_INDEX)) {
							AttachLight(lightData, a_srcData, lightPlacerNode, LP_INDEX);
						}
						LP_INDEX++;
					}
				}
			}
		} /* else if (auto xData = a_obj->GetExtraData<RE::NiStringsExtraData>("LIGHT_PLACER"); xData && xData->value && xData->size > 0) {
			if (auto lightSource = LightSourceData(xData); lightSource.data.IsValid()) {
				if (auto node = LightData::GetOrCreateNode(a_srcData.root->AsNode(), a_obj, LP_INDEX)) {
					AttachLight(lightSource, a_srcData, node, a_type, LP_INDEX);
				}
				LP_INDEX++;
			}
		}*/
		return RE::BSVisit::BSVisitControl::kContinue;
	});
}

void LightManager::AttachConfigLights(const SourceData& a_srcData, const Config::LightSourceData& a_lightData, std::uint32_t a_index)
{
	RE::NiAVObject* lightPlacerNode = nullptr;
	const auto&     rootNode = a_srcData.GetRootNode();

	std::visit(overload{
				   [&](const Config::PointData& pointData) {
					   auto& [filter, points, lightData] = pointData;
					   if (!filter.IsInvalid(a_srcData)) {
						   for (const auto [pointIdx, point] : std::views::enumerate(points)) {
							   lightPlacerNode = lightData.GetOrCreateNode(rootNode, point, static_cast<std::uint32_t>(pointIdx));
							   if (lightPlacerNode) {
								   AttachLight(lightData, a_srcData, lightPlacerNode->AsNode(), static_cast<std::uint32_t>(pointIdx));
							   }
						   }
					   }
				   },
				   [&](const Config::NodeData& nodeData) {
					   auto& [filter, nodes, lightData] = nodeData;
					   if (!filter.IsInvalid(a_srcData)) {
						   for (const auto& nodeName : nodes) {
							   lightPlacerNode = lightData.GetOrCreateNode(rootNode, nodeName, a_index);
							   if (lightPlacerNode) {
								   AttachLight(lightData, a_srcData, lightPlacerNode->AsNode());
							   }
						   }
					   }
				   } },
		a_lightData);
}

void LightManager::AttachLight(const LightSourceData& a_lightSource, const SourceData& a_srcData, RE::NiNode* a_node, std::uint32_t a_index)
{
	const auto name = a_lightSource.data.GetName(a_srcData, a_index);

	if (auto [bsLight, niLight] = a_lightSource.data.GenLight(a_srcData.ref, a_node, name); bsLight && niLight) {
		switch (a_srcData.type) {
		case SOURCE_TYPE::kRef:
			{
				gameRefLights.write([&](auto& map) {
					auto& lightDataVec = map[a_srcData.handle];

					if (std::find(lightDataVec.begin(), lightDataVec.end(), niLight) == lightDataVec.end()) {
						lightDataVec.emplace_back(a_lightSource, bsLight, niLight, a_srcData.ref);
					}
				});
			}
			break;
		case SOURCE_TYPE::kActorWorn:
			{
				gameActorWornLights.write([&](auto& map) {
					map[a_srcData.handle].write([&](auto& nodeNameMap) {
						char nodeName[MAX_PATH]{ '\0' };
						a_srcData.GetWornItemNodeName(nodeName);

						auto& lightDataVec = nodeNameMap[nodeName];

						if (std::find(lightDataVec.begin(), lightDataVec.end(), niLight) == lightDataVec.end()) {
							REFR_LIGH lightData(a_lightSource, bsLight, niLight, a_srcData.ref);
							lightDataVec.push_back(lightData);
							processedGameRefLights.write([&](auto& cellMap) {
								cellMap[a_srcData.cellID].write([&](auto& innerMap) {
									innerMap.emplace(lightData, a_srcData.handle);
								});
							});
						}
					});
				});
			}
			break;
		case SOURCE_TYPE::kActorMagic:
			{
				gameActorMagicLights.write([&](auto& map) {
					auto& effectLights = map[a_srcData.root];

					if (std::find(effectLights.lights.begin(), effectLights.lights.end(), niLight) == effectLights.lights.end()) {
						effectLights.lights.emplace_back(a_lightSource, bsLight, niLight, a_srcData.ref);
					}
				});
			}
			break;
		case SOURCE_TYPE::kTempEffect:
			{
				gameVisualEffectLights.write([&](auto& map) {
					auto& effectLights = map[a_srcData.effectID];

					if (std::find(effectLights.lights.begin(), effectLights.lights.end(), niLight) == effectLights.lights.end()) {
						effectLights.lights.emplace_back(a_lightSource, bsLight, niLight, a_srcData.ref);
					}
				});
			}
			break;
		default:
			break;
		}
	}
}

bool LightManager::ReattachLightsImpl(const SourceData& a_srcData)
{
	if (!gameRefLights.read([&](auto& map) {
			return map.contains(a_srcData.handle);
		})) {
		return false;
	}

	gameRefLights.write([&](auto& map) {
		if (auto it = map.find(a_srcData.handle); it != map.end()) {
			for (auto& lightData : it->second) {
				lightData.ReattachLight(a_srcData.ref);
			}
		}
	});

	return true;
}

void LightManager::AddLightsToProcessQueue(const RE::TESObjectCELL* a_cell, RE::TESObjectREFR* a_ref)
{
	auto cellFormID = a_cell->GetFormID();
	auto handle = a_ref->CreateRefHandle().native_handle();

	ForEachLight(a_ref, handle, [&](const auto& lightREFRData) {
		processedGameRefLights.write([&](auto& map) {
			map[cellFormID].write([&](auto& innerMap) {
				innerMap.emplace(lightREFRData, handle);
			});
		});
	});
}

void LightManager::UpdateFlickeringAndConditions(const RE::TESObjectCELL* a_cell)
{
	processedGameRefLights.read_unsafe([&](auto& map) {
		if (auto it = map.find(a_cell->GetFormID()); it != map.end()) {
			const auto pcPos = RE::PlayerCharacter::GetSingleton()->GetPosition();

			it->second.write([&](auto& innerMap) {
				const bool updateConditions = innerMap.UpdateTimer(0.25f);

				std::erase_if(innerMap.animatedLights, [&](auto& handle) {
					RE::TESObjectREFRPtr ref{};
					RE::LookupReferenceByHandle(handle, ref);

					if (!ref) {
						return true;
					}

					const bool withinFlickerDistance = ref->GetPosition().GetSquaredDistance(pcPos) < flickeringDistanceSq;

					ForEachLight(ref.get(), handle, [=](auto& lightREFRData) {
						if (updateConditions) {
							lightREFRData.UpdateConditions(ref.get());
						}
						lightREFRData.UpdateAnimation(withinFlickerDistance);
						if (withinFlickerDistance) {
							lightREFRData.UpdateFlickering();
						}
					});

					return false;
				});
			});
		}
	});
}

void LightManager::UpdateEmittance(const RE::TESObjectCELL* a_cell)
{
	processedGameRefLights.read_unsafe([&](auto& map) {
		if (auto it = map.find(a_cell->GetFormID()); it != map.end()) {
			it->second.write([&](auto& innerMap) {
				std::erase_if(innerMap.emittanceLights, [&](const auto& handle) {
					RE::TESObjectREFRPtr ref{};
					RE::LookupReferenceByHandle(handle, ref);

					if (!ref) {
						return true;
					}

					ForEachLight(handle, [](const auto& lightREFRData) {
						lightREFRData.UpdateEmittance();
					});

					return false;
				});
			});
		}
	});
}

void LightManager::RemoveLightsFromProcessQueue(const RE::TESObjectCELL* a_cell, const RE::ObjectRefHandle& a_handle)
{
	processedGameRefLights.read_unsafe([&](auto& map) {
		if (auto it = map.find(a_cell->GetFormID()); it != map.end()) {
			it->second.write([&](auto& innerMap) {
				innerMap.erase(a_handle.native_handle());
			});
		}
	});
}

void LightManager::UpdateTempEffectLights(RE::ReferenceEffect* a_effect)
{
	gameVisualEffectLights.read_unsafe([&](auto& map) {
		if (auto it = map.find(a_effect->effectID); it != map.end()) {
			const auto ref = a_effect->target.get();

			constexpr auto MAX_WAIT_TIME = 3.0f;
			const float    dimFactor = a_effect->finished ?
			                               (a_effect->lifetime + MAX_WAIT_TIME - a_effect->age) / MAX_WAIT_TIME :
			                               std::numeric_limits<float>::max();

			auto& [flickerTimer, conditionTimer, lightDataVec] = it->second;

			const bool updateFlicker = flickerTimer.UpdateTimer(RE::BSTimer::GetSingleton()->delta * 2.0f);
			const bool updateConditions = conditionTimer.UpdateTimer(0.25f);
			const bool withinFlickerDistance = ref->GetPosition().GetSquaredDistance(RE::PlayerCharacter::GetSingleton()->GetPosition()) < flickeringDistanceSq;

			for (auto& lightData : lightDataVec) {
				if (lightData.DimLight(dimFactor)) {
					continue;
				}
				if (updateConditions) {
					lightData.UpdateConditions(ref.get());
				}
				lightData.UpdateAnimation(withinFlickerDistance);
				if (updateFlicker) {
					if (withinFlickerDistance) {
						lightData.UpdateFlickering();
					}
				}
			}
		}
	});
}

void LightManager::UpdateCastingLights(RE::ActorMagicCaster* a_actorMagicCaster, float a_delta)
{
	if (a_actorMagicCaster->flags.none(RE::ActorMagicCaster::Flags::kCastingArtAttached)) {
		return;
	}

	if (!a_actorMagicCaster->castingArtData.attachedArt) {
		return;
	}

	auto actor = a_actorMagicCaster->GetCasterAsActor();
	if (!actor) {
		return;
	}

	gameActorMagicLights.read_unsafe([&](auto& map) {
		if (auto it = map.find(a_actorMagicCaster->castingArtData.attachedArt); it != map.end()) {
			auto& [flickerTimer, conditionTimer, lightDataVec] = it->second;

			const bool updateConditions = conditionTimer.UpdateTimer(a_delta, 0.25f);
			const bool withinFlickerDistance = actor->IsPlayerRef() || actor->GetPosition().GetSquaredDistance(RE::PlayerCharacter::GetSingleton()->GetPosition()) < flickeringDistanceSq;

			for (auto& lightData : lightDataVec) {
				if (updateConditions) {
					lightData.UpdateConditions(actor);
				}
				lightData.UpdateAnimation(withinFlickerDistance);
				if (withinFlickerDistance) {
					lightData.UpdateFlickering();
				}
			}
		}
	});
}
