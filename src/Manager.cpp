#include "Manager.h"

bool Config::FilteredData::IsInvalid(const std::string& a_model) const
{
	return (!blackList.empty() && blackList.contains(a_model)) || (!whiteList.empty() && !whiteList.contains(a_model));
}

bool LightManager::ReadConfigs()
{
	logger::info("{:*^50}", "CONFIG FILES");

	std::filesystem::path dir{ "Data\\LightPlacer" };
	if (!std::filesystem::exists(dir)) {
		logger::info("Data\\LightPlacer folder not found");
		return false;
	}

	for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(dir)) {
		if (dirEntry.is_directory() || dirEntry.path().extension() != ".json"sv) {
			continue;
		}
		logger::info("Reading {}...", dirEntry.path().string());
		std::string                 buffer;
		std::vector<Config::Format> tmpConfig;
		auto                        err = glz::read_file_json(tmpConfig, dirEntry.path().string(), buffer);
		if (err) {
			logger::error("\terror:{}", glz::format_error(err, buffer));
		} else {
			logger::info("\t{} entries", tmpConfig.size());
			config.append_range(std::move(tmpConfig));
		}
	}

	return !config.empty();
}

void LightManager::OnDataLoad()
{
	const auto append_strings = [&](const FlatSet<std::string>& a_filterSet, Config::LightDataVec& a_lightData, FlatMap<std::string, Config::LightDataVec>& a_gameMap) {
		PostProcessLightData(a_lightData);
		for (auto& str : a_filterSet) {
			a_gameMap[str].append_range(a_lightData);
		}
	};

	const auto append_formIDs = [&](const FlatSet<std::string>& a_filterSet, Config::LightDataVec& a_lightData, FlatMap<RE::FormID, Config::LightDataVec>& a_gameMap) {
		PostProcessLightData(a_lightData);
		for (auto& rawID : a_filterSet) {
			a_gameMap[RE::GetFormID(rawID)].append_range(a_lightData);
		}
	};

	const auto append_integers = [&](const FlatSet<std::uint32_t>& a_filterSet, Config::LightDataVec& a_lightData, FlatMap<std::uint32_t, Config::LightDataVec>& a_gameMap) {
		PostProcessLightData(a_lightData);
		for (auto& integer : a_filterSet) {
			a_gameMap[integer].append_range(a_lightData);
		}
	};

	flickeringDistanceSq = std::powf(RE::GetINISetting("fFlickeringLightDistance:General")->GetFloat(), 2);

	for (auto& multiData : config) {
		std::visit(overload{
					   [&](Config::MultiModelSet& models) {
						   append_strings(models.models, models.lights, gameModels);
					   },
					   [&](Config::MultiReferenceSet& references) {
						   append_formIDs(references.references, references.lights, gameReferences);
					   },
					   [&](Config::MultiVisualEffectSet& visualEffects) {
						   append_formIDs(visualEffects.visualEffects, visualEffects.lights, gameVisualEffects);
					   },
					   [&](Config::MultiAddonSet& addonNodes) {
						   append_integers(addonNodes.addonNodes, addonNodes.lights, gameAddonNodes);
					   } },
			multiData);
	}

	logger::info("{:*^50}", "RESULTS");

	constexpr auto log_size = []<typename K, typename D>(std::string_view ID, const FlatMap<K, D>& a_map) {
		logger::info("{} : {} entries", ID, a_map.size());
	};

	log_size("Models", gameModels);
	log_size("References", gameReferences);
	log_size("VisualEffects", gameVisualEffects);
	log_size("AddonNodes", gameAddonNodes);
}

void LightManager::PostProcessLightData(Config::LightDataVec& a_lightDataVec)
{
	std::erase_if(a_lightDataVec, [](auto& attachLightData) {
		bool failedPostProcess = false;
		std::visit(overload{
					   [&](Config::PointData& pointData) {
						   failedPostProcess = !pointData.data.PostProcess();
					   },
					   [&](Config::NodeData& nodeData) {
						   failedPostProcess = !nodeData.data.PostProcess();
					   },
					   [&](Config::FilteredData& filteredData) {
						   failedPostProcess = !filteredData.data.PostProcess();
					   } },
			attachLightData);
		return failedPostProcess;
	});
}

void LightManager::AddLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base, RE::NiAVObject* a_root)
{
	if (!a_ref || !a_base) {
		return;
	}

	ObjectREFRParams refParams(a_ref, a_root);
	if (!refParams.IsValid()) {
		return;
	}

	AttachLightsImpl(refParams, a_base, a_base->As<RE::TESModel>(), TYPE::kRef);
}

void LightManager::ReattachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base)
{
	if (!a_ref || !a_base || a_base->Is(RE::FormType::Light)) {
		return;
	}

	ObjectREFRParams refParams(a_ref);
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
							lightRefrData.RemoveLight();
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
					lightRefrData.RemoveLight();
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
	if (!a_ref || a_slot == -1) {
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

	ObjectREFRParams refParams(a_ref, a_root);
	if (!refParams.IsValid()) {
		return;
	}

	AttachLightsImpl(refParams, bipObject.item->As<RE::TESBoundObject>(), bipObject.part, TYPE::kActor);
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
						lightData.RemoveLight();
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

	if (gameVisualEffectLights.read([&](auto& map) {
			return map.contains(a_effect);
		})) {
		return;
	}

	auto ref = a_effect->target.get();
	if (!ref) {
		return;
	}

	if (auto invMgr = RE::Inventory3DManager::GetSingleton(); invMgr && invMgr->tempRef == ref.get()) {
		return;
	}

	ObjectREFRParams refParams(ref.get(), a_effect->GetAttachRoot());
	if (!refParams.IsValid()) {
		return;
	}

	refParams.effect = a_effect;

	if (auto it = gameVisualEffects.find(a_effectID); it != gameVisualEffects.end()) {
		for (const auto& [index, data] : std::views::enumerate(it->second)) {
			AttachConfigLights(refParams, data, static_cast<std::uint32_t>(index), TYPE::kEffect);
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
				lightData.RemoveLight();
			}
			if (a_clear) {
				map.erase(it);
			}
		}
	});
}

void LightManager::AttachLightsImpl(const ObjectREFRParams& a_refParams, RE::TESBoundObject* a_object, RE::TESModel* a_model, TYPE a_type)
{
	if (!a_model) {
		return;
	}

	auto modelPath = RE::SanitizeModel(a_model->GetModel());
	if (modelPath.empty()) {
		return;
	}

	AttachReferenceLights(a_refParams, modelPath, a_object->GetFormID(), a_type);
	AttachMeshLights(a_refParams, modelPath, a_type);
}

void LightManager::AttachReferenceLights(const ObjectREFRParams& a_refParams, const std::string& a_model, RE::FormID a_baseFormID, TYPE a_type)
{
	auto refID = a_refParams.ref->GetFormID();

	auto fIt = gameReferences.find(refID);
	if (fIt == gameReferences.end()) {
		fIt = gameReferences.find(a_baseFormID);
	}

	if (fIt != gameReferences.end()) {
		for (const auto& [index, data] : std::views::enumerate(fIt->second)) {
			AttachConfigLights(a_refParams, data, static_cast<std::uint32_t>(index), a_type);
		}
	}

	if (auto mIt = gameModels.find(a_model); mIt != gameModels.end()) {
		for (const auto& [index, data] : std::views::enumerate(mIt->second)) {
			AttachConfigLights(a_refParams, data, static_cast<std::uint32_t>(index), a_type);
		}
	}
}

void LightManager::AttachConfigLights(const ObjectREFRParams& a_refParams, const Config::LightData& a_lightData, std::uint32_t a_index, TYPE a_type)
{
	RE::NiAVObject* lightPlacerNode = nullptr;
	const auto&     rootNode = a_refParams.root;

	std::visit(overload{
				   [&](const Config::PointData& pointData) {
					   auto name = LightCreateParams::GetNodeName(a_index);
					   lightPlacerNode = rootNode->GetObjectByName(name);
					   if (!lightPlacerNode) {
						   lightPlacerNode = RE::NiNode::Create(0);
						   lightPlacerNode->name = name;
						   RE::AttachNode(rootNode, lightPlacerNode);
					   }
					   if (lightPlacerNode) {
						   for (const auto& [pointIdx, point] : std::views::enumerate(pointData.points)) {
							   AttachLight(pointData.data, a_refParams, lightPlacerNode->AsNode(), a_type, static_cast<std::uint32_t>(pointIdx), point);
						   }
					   }
				   },
				   [&](const Config::NodeData& nodeData) {
					   for (const auto& nodeName : nodeData.nodes) {
						   lightPlacerNode = LightCreateParams::GetOrCreateNode(rootNode, nodeName, a_index);
						   if (lightPlacerNode) {
							   AttachLight(nodeData.data, a_refParams, lightPlacerNode->AsNode(), a_type);
						   }
					   }
				   },
				   [&](const Config::FilteredData&) {} },
		a_lightData);
}

void LightManager::AttachMeshLights(const ObjectREFRParams& a_refParams, const std::string& a_model, TYPE a_type)
{
	std::int32_t LP_INDEX = 0;
	std::int32_t LP_ADDON_INDEX = 0;

	RE::BSVisit::TraverseScenegraphObjects(a_refParams.root, [&](RE::NiAVObject* a_obj) {
		if (auto addonNode = netimmerse_cast<RE::BSValueNode*>(a_obj)) {
			if (auto it = gameAddonNodes.find(addonNode->value); it != gameAddonNodes.end()) {
				for (const auto& data : it->second) {
					if (auto& filteredData = std::get<Config::FilteredData>(data); !filteredData.IsInvalid(a_model)) {
						AttachLight(filteredData.data, a_refParams, addonNode, a_type, LP_ADDON_INDEX);
						LP_ADDON_INDEX++;
					}
				}
			}
		} else if (auto xData = a_obj->GetExtraData<RE::NiStringsExtraData>("LIGHT_PLACER"); xData && xData->value && xData->size > 0) {
			if (auto lightParams = LightCreateParams(xData); lightParams.IsValid()) {
				if (auto node = LightCreateParams::GetOrCreateNode(a_refParams.root->AsNode(), a_obj, LP_INDEX)) {
					AttachLight(lightParams, a_refParams, node, a_type, LP_INDEX);
				}
				LP_INDEX++;
			}
		}
		return RE::BSVisit::BSVisitControl::kContinue;
	});
}

void LightManager::AttachLight(const LightCreateParams& a_lightParams, const ObjectREFRParams& a_refParams, RE::NiNode* a_node, TYPE a_type, std::uint32_t a_index, const RE::NiPoint3& a_point)
{
	auto& [ref, effect, root, handle] = a_refParams;

	if (auto [bsLight, niLight] = a_lightParams.GenLight(ref, a_node, a_point, a_index); bsLight && niLight) {
		switch (a_type) {
		case TYPE::kRef:
			{
				gameRefLights.write([&](auto& map) {
					auto& lightDataVec = map[handle];
					if (std::find(lightDataVec.begin(), lightDataVec.end(), niLight) == lightDataVec.end()) {
						lightDataVec.emplace_back(a_lightParams, bsLight, niLight, ref, a_node, a_point, a_index);
					}
				});
			}
			break;
		case TYPE::kActor:
			{
				gameActorLights.write([&](auto& map) {
					map[handle].write([&](auto& nodeMap) {
						auto& lightDataVec = nodeMap[root];
						if (std::find(lightDataVec.begin(), lightDataVec.end(), niLight) == lightDataVec.end()) {
							REFR_LIGH lightData(a_lightParams, bsLight, niLight, ref, a_node, a_point, a_index);
							lightDataVec.push_back(lightData);
							processedGameRefLights.write([&](auto& cellMap) {
								cellMap[ref->GetParentCell()->GetFormID()].write([&](auto& innerMap) {
									innerMap.emplace(lightData, handle);
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
					auto& effectLights = map[effect];
					if (std::find(effectLights.lights.begin(), effectLights.lights.end(), niLight) == effectLights.lights.end()) {
						effectLights.lights.emplace_back(a_lightParams, bsLight, niLight, ref, a_node, a_point, a_index);
					}
				});
			}
			break;
		}
	}
}

bool LightManager::ReattachLightsImpl(const ObjectREFRParams& a_refParams)
{
	auto& [ref, effect, root, handle] = a_refParams;

	if (!gameRefLights.read([&](auto& map) {
			return map.contains(handle);
		})) {
		return false;
	}

	gameRefLights.write([&](auto& map) {
		if (auto it = map.find(handle); it != map.end()) {
			for (auto& lightData : it->second) {
				lightData.ReattachLight(ref);
			}
		}
	});

	return true;
}

void LightManager::AddLightsToProcessQueue(RE::TESObjectCELL* a_cell, RE::TESObjectREFR* a_ref)
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

void LightManager::UpdateFlickeringAndConditions(RE::TESObjectCELL* a_cell)
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
						lightREFRData.UpdateAnimation();
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

void LightManager::UpdateEmittance(RE::TESObjectCELL* a_cell)
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
	const auto ref = a_effect->target.get();

	gameVisualEffectLights.read_unsafe([&](auto& map) {
		if (auto it = map.find(a_effect); it != map.end()) {
			auto& [flickerTimer, conditionTimer, lightDataVec] = it->second;

			const bool updateFlicker = flickerTimer.UpdateTimer(RE::BSTimer::GetSingleton()->delta * 2.0f);
			const bool updateConditions = conditionTimer.UpdateTimer(0.25f);
			const bool withinFlickerDistance = ref->GetPosition().GetSquaredDistance(RE::PlayerCharacter::GetSingleton()->GetPosition()) < flickeringDistanceSq;

			for (auto& lightData : lightDataVec) {
				if (updateConditions) {
					lightData.UpdateConditions(ref.get());
				}
				lightData.UpdateAnimation();
				if (withinFlickerDistance && updateFlicker) {
					lightData.UpdateFlickering();
				}
			}
		}
	});
}
