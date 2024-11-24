#include "Manager.h"

bool Config::FilteredData::IsInvalid(const std::string& a_model) const
{
	return (!blackList.empty() && blackList.contains(a_model)) || (!whiteList.empty() && !whiteList.contains(a_model));
}

void LightManager::ProcessedLights::emplace(const REFR_LIGH& a_data, RE::RefHandle a_handle)
{
	if (!a_data.light->GetNoFlicker()) {
		stl::unique_insert(flickeringLights, a_handle);
	}
	if (a_data.emittanceForm) {
		stl::unique_insert(emittanceLights, a_handle);
	}
	if (a_data.conditions) {
		stl::unique_insert(conditionalLights, a_handle);
	}
}

void LightManager::ProcessedLights::erase(RE::RefHandle a_handle)
{
	stl::unique_erase(flickeringLights, a_handle);
	stl::unique_erase(conditionalLights, a_handle);
	stl::unique_erase(emittanceLights, a_handle);
}

bool LightManager::ReadConfigs()
{
	logger::info("{:*^30}", "CONFIG FILES");

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
		std::string buffer;
		auto        err = glz::read_file_json(config, dirEntry.path().string(), buffer);
		if (err) {
			logger::info("\terror:{}", glz::format_error(err, buffer));
		}
	}

	return !config.empty();
}

void LightManager::OnDataLoad()
{
	for (auto& multiData : config) {
		std::visit(overload{
					   [&](Config::MultiModelSet& models) {
						   PostProcessLightData(models.lightData);
						   for (auto& model : models.models) {
							   gameModels[model].append_range(models.lightData);
						   }
					   },
					   [&](Config::MultiReferenceSet& references) {
						   PostProcessLightData(references.lightData);
						   for (auto& rawID : references.references) {
							   gameReferences[RE::GetFormID(rawID)].append_range(references.lightData);
						   }
					   },
					   [&](Config::MultiAddonSet& addonNodes) {
						   PostProcessLightData(addonNodes.lightData);
						   for (auto& index : addonNodes.addonNodes) {
							   gameAddonNodes[index].append_range(addonNodes.lightData);
						   }
					   } },
			multiData);
	}
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

	AttachLightsImpl(refParams, a_base, a_base->As<RE::TESModel>());
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
					for (auto& [node, lightDataVec] : nodeMap) {
						for (auto& lightData : lightDataVec) {
							lightData.RemoveLight();
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

void LightManager::AddWornLights(RE::TESObjectREFR* a_ref, RE::BSTSmartPointer<RE::BipedAnim>& a_bipedAnim, std::int32_t a_slot, RE::NiAVObject* a_root)
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

	AttachLightsImpl(refParams, bipObject.item->As<RE::TESBoundObject>(), bipObject.part);
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
	if (a_root) {
		auto handle = a_handle.native_handle();
		gameActorLights.write([&](auto& map) {
			if (auto it = map.find(handle); it != map.end()) {
				it->second.write([&](auto& nodeMap) {
					if (auto it = nodeMap.find(a_root->AsNode()); it != nodeMap.end()) {
						for (auto& lightData : it->second) {
							lightData.RemoveLight();
						}
						nodeMap.erase(it);
					}
				});
			}
		});
	}
}

void LightManager::AttachLightsImpl(const ObjectREFRParams& a_refParams, RE::TESBoundObject* a_object, RE::TESModel* a_model)
{
	if (!a_model) {
		return;
	}

	auto modelPath = RE::SanitizeModel(a_model->GetModel());
	if (modelPath.empty()) {
		return;
	}

	AttachConfigLights(a_refParams, modelPath, a_object->GetFormID());
	AttachMeshLights(a_refParams, modelPath);
}

void LightManager::AttachConfigLights(const ObjectREFRParams& a_refParams, const std::string& a_model, RE::FormID a_baseFormID)
{
	auto refID = a_refParams.ref->GetFormID();

	auto fIt = gameReferences.find(refID);
	if (fIt == gameReferences.end()) {
		fIt = gameReferences.find(a_baseFormID);
	}

	if (fIt != gameReferences.end()) {
		for (const auto& [index, data] : std::views::enumerate(fIt->second)) {
			AttachConfigLights(a_refParams, data, index);
		}
	}

	if (auto mIt = gameModels.find(a_model); mIt != gameModels.end()) {
		for (const auto& [index, data] : std::views::enumerate(mIt->second)) {
			AttachConfigLights(a_refParams, data, index);
		}
	}
}

void LightManager::AttachConfigLights(const ObjectREFRParams& a_refParams, const Config::LightData& a_lightData, std::uint32_t a_index)
{
	RE::NiAVObject* lightPlacerNode = nullptr;
	auto            rootNode = a_refParams.root;

	std::visit(overload{
				   [&](const Config::PointData& pointData) {
					   auto name = pointData.data.GetNodeName(a_index);
					   if (lightPlacerNode = rootNode->GetObjectByName(name); !lightPlacerNode) {
						   lightPlacerNode = RE::NiNode::Create(0);
						   lightPlacerNode->name = name;
						   RE::AttachNode(rootNode, lightPlacerNode);
					   }
					   if (lightPlacerNode) {
						   for (auto const& [pointIdx, point] : std::views::enumerate(pointData.points)) {
							   AttachLight(pointData.data, a_refParams, lightPlacerNode->AsNode(), pointIdx, point);
						   }
					   }
				   },
				   [&](const Config::NodeData& nodeData) {
					   for (const auto& nodeName : nodeData.nodes) {
						   if (lightPlacerNode = nodeData.data.GetOrCreateNode(rootNode, nodeName, a_index); lightPlacerNode) {
							   AttachLight(nodeData.data, a_refParams, lightPlacerNode->AsNode());
						   }
					   }
				   },
				   [&](const Config::FilteredData&) {
					   return;  // not handled here.
				   } },
		a_lightData);
}

void LightManager::AttachMeshLights(const ObjectREFRParams& a_refParams, const std::string& a_model)
{
	std::int32_t LP_INDEX = 0;
	std::int32_t LP_ADDON_INDEX = 0;

	RE::BSVisit::TraverseScenegraphObjects(a_refParams.root, [&](RE::NiAVObject* a_obj) {
		if (auto addonNode = netimmerse_cast<RE::BSValueNode*>(a_obj)) {
			if (auto it = gameAddonNodes.find(addonNode->value); it != gameAddonNodes.end()) {
				for (const auto& data : it->second) {
					if (auto& filteredData = std::get<Config::FilteredData>(data); !filteredData.IsInvalid(a_model)) {
						AttachLight(filteredData.data, a_refParams, addonNode, LP_ADDON_INDEX);
						LP_ADDON_INDEX++;
					}
				}
			}
		} else if (auto xData = a_obj->GetExtraData<RE::NiStringsExtraData>("LIGHT_PLACER"); xData && xData->value && xData->size > 0) {
			if (auto lightParams = LightCreateParams(xData); lightParams.IsValid()) {
				if (auto node = lightParams.GetOrCreateNode(a_refParams.root->AsNode(), a_obj, LP_INDEX)) {
					AttachLight(lightParams, a_refParams, node, LP_INDEX);
				}
				LP_INDEX++;
			}
		}
		return RE::BSVisit::BSVisitControl::kContinue;
	});
}

void LightManager::AttachLight(const LightCreateParams& a_lightParams, const ObjectREFRParams& a_refParams, RE::NiNode* a_node, std::uint32_t a_index, const RE::NiPoint3& a_point)
{
	auto& [ref, root, handle] = a_refParams;

	RE::BSLight*      bsLight = nullptr;
	RE::NiPointLight* niLight = nullptr;

	if (std::tie(bsLight, niLight) = a_lightParams.GenLight(ref, a_node, a_point, a_index); bsLight && niLight) {
		if (RE::IsActor(ref)) {
			gameActorLights.write([&](auto& map) {
				map[handle].write([&](auto& nodeMap) {
					auto& lightDataVec = nodeMap[root];
					if (std::find(lightDataVec.begin(), lightDataVec.end(), niLight) == lightDataVec.end()) {
						REFR_LIGH lightData(a_lightParams, bsLight, niLight, ref, a_node, a_point, a_index);
						lightDataVec.push_back(lightData);
						processedGameLights.write([&](auto& map) {
							map[ref->GetParentCell()->GetFormID()].write([&](auto& innerMap) {
								innerMap.emplace(lightData, handle);
							});
						});
					}
				});
			});
		} else {
			gameRefLights.write([&](auto& map) {
				auto& lightDataVec = map[handle];
				if (std::find(lightDataVec.begin(), lightDataVec.end(), niLight) == lightDataVec.end()) {
					lightDataVec.emplace_back(a_lightParams, bsLight, niLight, ref, a_node, a_point, a_index);
				}
			});
		}
	}
}

bool LightManager::ReattachLightsImpl(const ObjectREFRParams& a_refParams)
{
	auto& [ref, root, handle] = a_refParams;

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
		processedGameLights.write([&](auto& map) {
			map[cellFormID].write([&](auto& innerMap) {
				innerMap.emplace(lightREFRData, handle);
			});
		});
	});
}

void LightManager::UpdateFlickeringAndConditions(RE::TESObjectCELL* a_cell)
{
	processedGameLights.read_unsafe([&](auto& map) {
		if (auto it = map.find(a_cell->GetFormID()); it != map.end()) {
			it->second.write([&](auto& innerMap) {
				innerMap.lastUpdateTime += RE::GetSecondsSinceLastFrame();
				if (innerMap.lastUpdateTime >= 0.25f) {
					innerMap.lastUpdateTime = 0.0f;

					std::erase_if(innerMap.conditionalLights, [&](const auto& handle) {
						RE::TESObjectREFRPtr ref{};
						RE::LookupReferenceByHandle(handle, ref);

						if (!ref) {
							return true;
						}

						ForEachLight(ref.get(), handle, [&](const auto& lightREFRData) {
							lightREFRData.UpdateConditions(ref.get());
						});

						return false;
					});
				}

				static auto flickeringDistance = RE::GetINISetting("fFlickeringLightDistance:General")->GetFloat() * RE::GetINISetting("fFlickeringLightDistance:General")->GetFloat();
				auto        pcPos = RE::PlayerCharacter::GetSingleton()->GetPosition();

				std::erase_if(innerMap.flickeringLights, [&](const auto& handle) {
					RE::TESObjectREFRPtr ref{};
					RE::LookupReferenceByHandle(handle, ref);

					if (!ref) {
						return true;
					}

					if (ref->GetPosition().GetSquaredDistance(pcPos) < flickeringDistance) {
						ForEachLight(ref.get(), handle, [&](const auto& lightREFRData) {
							lightREFRData.UpdateFlickering();
						});
					}
					return false;
				});
			});
		}
	});
}

void LightManager::UpdateEmittance(RE::TESObjectCELL* a_cell)
{
	processedGameLights.read_unsafe([&](auto& map) {
		if (auto it = map.find(a_cell->GetFormID()); it != map.end()) {
			it->second.write([&](auto& innerMap) {
				std::erase_if(innerMap.emittanceLights, [&](const auto& handle) {
					RE::TESObjectREFRPtr ref{};
					RE::LookupReferenceByHandle(handle, ref);

					if (!ref) {
						return true;
					}

					ForEachLight(ref.get(), handle, [&](const auto& lightREFRData) {
						lightREFRData.UpdateEmittance();
					});

					return false;
				});
			});
		}
	});
}

void LightManager::RemoveLightsFromProcessQueue(RE::TESObjectCELL* a_cell, const RE::ObjectRefHandle& a_handle)
{
	processedGameLights.read_unsafe([&](auto& map) {
		if (auto it = map.find(a_cell->GetFormID()); it != map.end()) {
			it->second.write([&](auto& innerMap) {
				innerMap.erase(a_handle.native_handle());
			});
		}
	});
}
