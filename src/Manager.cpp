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

	ObjectREFRParams refParams(a_ref, a_root, a_base);
	if (!refParams.IsValid()) {
		return;
	}

	AttachLightsImpl(refParams, TYPE::kRef);
}

void LightManager::ReattachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base)
{
	if (!a_ref || !a_ref->Get3D() || !a_base || a_base->Is(RE::FormType::Light)) {
		return;
	}

	ObjectREFRParams refParams(a_ref, a_base);
	if (!refParams.IsValid()) {
		return;
	}

	ReattachLightsImpl(refParams);
}

void LightManager::DetachLights(RE::TESObjectREFR* a_ref, bool a_clearData)
{
	auto handle = a_ref->CreateRefHandle().native_handle();

	if (RE::IsActor(a_ref)) {
		gameActorLights.write([&](auto& map) {
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

	RE::BSTSmartPointer<RE::BipedAnim> bipedAnim = a_bipedAnim;
	if (!bipedAnim) {
		bipedAnim = a_ref->GetBiped();
	}
	if (!bipedAnim || a_ref->IsPlayerRef() && bipedAnim == a_ref->GetBiped(true)) {
		return;
	}

	const auto& bipObject = a_bipedAnim->objects[a_slot];
	if (!bipObject.item || bipObject.item->Is(RE::FormType::Light)) {
		return;
	}

	ObjectREFRParams refParams(a_ref, a_root, bipObject.item->As<RE::TESBoundObject>(), bipObject.part);
	if (!refParams.IsValid()) {
		return;
	}

	AttachLightsImpl(refParams, TYPE::kActor);
}

void LightManager::ReattachWornLights(const RE::ActorHandle& a_handle)
{
	auto handle = a_handle.native_handle();

	gameActorLights.read([&](auto& map) {
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

	gameActorLights.write([&](auto& map) {
		if (auto it = map.find(handle); it != map.end()) {
			it->second.write([&](auto& nodeMap) {
				if (auto nIt = nodeMap.find(a_root->AsNode()); nIt != nodeMap.end()) {
					for (auto& lightData : nIt->second) {
						lightData.RemoveLight(true);
					}
					nodeMap.erase(nIt);
				}
			});
		}
	});
}

void LightManager::AddTempEffectLights(RE::ReferenceEffect* a_effect, RE::FormID a_effectID)
{
	if (!a_effect || a_effectID == 0) {
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

	const auto base = RE::GetReferenceEffectBase(a_effect);
	if (!base) {
		return;
	}

	ObjectREFRParams refParams(ref.get(), root, base);
	if (!refParams.IsValid()) {
		return;
	}
	refParams.effect = a_effect;

	if (auto it = gameVisualEffects.find(a_effectID); it != gameVisualEffects.end()) {
		for (const auto [index, data] : std::views::enumerate(it->second)) {
			AttachConfigLights(refParams, data, static_cast<std::uint32_t>(index), TYPE::kEffect);
		}
	}

	if (base->Is(RE::FormType::ArtObject)) {
		if (auto it = gameModels.find(refParams.modelPath); it != gameModels.end()) {
			for (const auto [index, data] : std::views::enumerate(it->second)) {
				AttachConfigLights(refParams, data, static_cast<std::uint32_t>(index), TYPE::kEffect);
			}
		}
	}
}

void LightManager::ReattachTempEffectLights(RE::ReferenceEffect* a_effect)
{
	gameVisualEffectLights.read([&](auto& map) {
		if (auto it = map.find(a_effect); it != map.end()) {
			for (auto& lightData : it->second.lights) {
				lightData.ReattachLight();
			}
		}
	});
}

void LightManager::DetachTempEffectLights(RE::ReferenceEffect* a_effect, bool a_clear)
{
	gameVisualEffectLights.write([&](auto& map) {
		if (auto it = map.find(a_effect); it != map.end()) {
			for (auto& lightData : it->second.lights) {
				lightData.RemoveLight(a_clear);
			}
			if (a_clear) {
				map.erase(it);
			}
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

void LightManager::AttachLightsImpl(const ObjectREFRParams& a_refParams, TYPE a_type)
{
	[[maybe_unused]] std::int32_t LP_INDEX = 0;
	std::int32_t                  LP_ADDON_INDEX = 0;

	if (auto mIt = gameModels.find(a_refParams.modelPath); mIt != gameModels.end()) {
		for (const auto [index, data] : std::views::enumerate(mIt->second)) {
			AttachConfigLights(a_refParams, data, static_cast<std::uint32_t>(index), a_type);
		}
	}

	RE::BSVisit::TraverseScenegraphObjects(a_refParams.root, [&](RE::NiAVObject* a_obj) {
		if (auto addonNode = netimmerse_cast<RE::BSValueNode*>(a_obj)) {
			if (auto it = gameAddonNodes.find(addonNode->value); it != gameAddonNodes.end()) {
				for (const auto& [filter, lightData] : it->second) {
					if (!filter.IsInvalid(a_refParams)) {
						if (auto lightPlacerNode = lightData.data.GetOrCreateNode(a_refParams.root, addonNode, LP_ADDON_INDEX)) {
							AttachLight(lightData, a_refParams, lightPlacerNode, a_type, LP_ADDON_INDEX);
						}
						LP_ADDON_INDEX++;
					}
				}
			}
		} /* else if (auto xData = a_obj->GetExtraData<RE::NiStringsExtraData>("LIGHT_PLACER"); xData && xData->value && xData->size > 0) {
			if (auto lightSource = LightSourceData(xData); lightSource.data.IsValid()) {
				if (auto node = LightData::GetOrCreateNode(a_refParams.root->AsNode(), a_obj, LP_INDEX)) {
					AttachLight(lightSource, a_refParams, node, a_type, LP_INDEX);
				}
				LP_INDEX++;
			}
		}*/
		return RE::BSVisit::BSVisitControl::kContinue;
	});
}

void LightManager::AttachConfigLights(const ObjectREFRParams& a_refParams, const Config::LightSourceData& a_lightData, std::uint32_t a_index, TYPE a_type)
{
	RE::NiAVObject* lightPlacerNode = nullptr;
	const auto&     rootNode = a_refParams.root;

	std::visit(overload{
				   [&](const Config::PointData& pointData) {
					   auto& [filter, points, lightData] = pointData;
					   if (!filter.IsInvalid(a_refParams)) {
						   for (const auto [pointIdx, point] : std::views::enumerate(points)) {
							   auto name = LightData::GetNodeName(point, a_index);
							   lightPlacerNode = rootNode->GetObjectByName(name);
							   if (!lightPlacerNode) {
								   lightPlacerNode = RE::NiNode::Create(0);
								   lightPlacerNode->name = name;
								   lightPlacerNode->local.translate = point + lightData.data.offset;
								   lightPlacerNode->local.rotate = lightData.data.rotation;
								   RE::AttachNode(rootNode, lightPlacerNode);
							   }
							   if (lightPlacerNode) {
								   AttachLight(lightData, a_refParams, lightPlacerNode->AsNode(), a_type, static_cast<std::uint32_t>(pointIdx));
							   }
						   }
					   }
				   },
				   [&](const Config::NodeData& nodeData) {
					   auto& [filter, nodes, lightData] = nodeData;
					   if (!filter.IsInvalid(a_refParams)) {
						   for (const auto& nodeName : nodes) {
							   lightPlacerNode = lightData.data.GetOrCreateNode(rootNode, nodeName, a_index);
							   if (lightPlacerNode) {
								   AttachLight(lightData, a_refParams, lightPlacerNode->AsNode(), a_type);
							   }
						   }
					   }
				   } },
		a_lightData);
}

void LightManager::AttachLight(const LightSourceData& a_lightSource, const ObjectREFRParams& a_refParams, RE::NiNode* a_node, TYPE a_type, std::uint32_t a_index)
{
	if (auto [bsLight, niLight] = a_lightSource.data.GenLight(a_refParams.ref, a_refParams.effect, a_node, a_index); bsLight && niLight) {
		switch (a_type) {
		case TYPE::kRef:
			{
				gameRefLights.write([&](auto& map) {
					auto& lightDataVec = map[a_refParams.handle];
					if (std::find(lightDataVec.begin(), lightDataVec.end(), niLight) == lightDataVec.end()) {
						lightDataVec.emplace_back(a_lightSource, bsLight, niLight, a_refParams.ref, a_index);
					}
				});
			}
			break;
		case TYPE::kActor:
			{
				gameActorLights.write([&](auto& map) {
					map[a_refParams.handle].write([&](auto& nodeMap) {
						auto& lightDataVec = nodeMap[a_refParams.root];
						if (std::find(lightDataVec.begin(), lightDataVec.end(), niLight) == lightDataVec.end()) {
							REFR_LIGH lightData(a_lightSource, bsLight, niLight, a_refParams.ref, a_index);
							lightDataVec.push_back(lightData);
							processedGameRefLights.write([&](auto& cellMap) {
								cellMap[a_refParams.ref->GetParentCell()->GetFormID()].write([&](auto& innerMap) {
									innerMap.emplace(lightData, a_refParams.handle);
								});
							});
						}
					});
				});
			}
			break;
		case TYPE::kEffect:
			{
				gameVisualEffectLights.write([&](auto& map) {
					auto& effectLights = map[a_refParams.effect];
					if (std::find(effectLights.lights.begin(), effectLights.lights.end(), niLight) == effectLights.lights.end()) {
						effectLights.lights.emplace_back(a_lightSource, bsLight, niLight, a_refParams.ref, a_index);
					}
				});
			}
			break;
		}
	}
}

bool LightManager::ReattachLightsImpl(const ObjectREFRParams& a_refParams)
{
	if (!gameRefLights.read([&](auto& map) {
			return map.contains(a_refParams.handle);
		})) {
		return false;
	}

	gameRefLights.write([&](auto& map) {
		if (auto it = map.find(a_refParams.handle); it != map.end()) {
			for (auto& lightData : it->second) {
				lightData.ReattachLight(a_refParams.ref);
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
		if (auto it = map.find(a_effect); it != map.end()) {
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
