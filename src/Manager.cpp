#include "Manager.h"

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
						   AttachLightVecPostProcess(models.lightData);
						   for (auto& model : models.models) {
							   gameModels[model].append_range(models.lightData);
						   }
					   },
					   [&](Config::MultiReferenceSet& references) {
						   AttachLightVecPostProcess(references.lightData);
						   for (auto& rawID : references.references) {
							   gameReferences[RE::GetFormID(rawID)].append_range(references.lightData);
						   }
					   },
					   [&](Config::MultiAddonSet& addonNodes) {
						   AttachLightVecPostProcess(addonNodes.lightData);
						   for (auto& index : addonNodes.addonNodes) {
							   gameAddonNodes[index].append_range(addonNodes.lightData);
						   }
					   } },
			multiData);
	}
}

void LightManager::TryAttachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base)
{
	if (!a_ref || !a_base || a_base->Is(RE::FormType::Light)) {
		return;
	}

	ObjectRefData refData(a_ref);
	TryAttachLightsImpl(refData, a_base, a_base->As<RE::TESModel>());
}

void LightManager::TryAttachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base, RE::NiAVObject* a_root)
{
	if (!a_ref || !a_base) {
		return;
	}

	ObjectRefData refData(a_ref, a_root);
	TryAttachLightsImpl(refData, a_base, a_base->As<RE::TESModel>());
}

void LightManager::TryAttachLights(RE::TESObjectREFR* a_ref, RE::BSTSmartPointer<RE::BipedAnim>& a_bipedAnim, std::int32_t a_slot, RE::NiAVObject* a_root)
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

	ObjectRefData refData(a_ref, a_root);
	TryAttachLightsImpl(refData, bipObject.item->As<RE::TESBoundObject>(), bipObject.part);
}

void LightManager::TryAttachLightsImpl(const ObjectRefData& a_refData, RE::TESBoundObject* a_object, RE::TESModel* a_model)
{
	if (!a_refData.IsValid() || !a_model) {
		return;
	}

	auto modelPath = RE::SanitizeModel(a_model->GetModel());
	if (modelPath.empty()) {
		return;
	}

	AttachConfigLights(a_refData, modelPath, a_object->GetFormID());
	AttachMeshLights(a_refData, modelPath);
}

void LightManager::AttachConfigLights(const ObjectRefData& a_refData, const std::string& a_model, RE::FormID a_baseFormID)
{
	auto refID = a_refData.ref->GetFormID();

	auto fIt = gameReferences.find(refID);
	if (fIt == gameReferences.end()) {
		fIt = gameReferences.find(a_baseFormID);
	}

	if (fIt != gameReferences.end()) {
		for (const auto& [index, data] : std::views::enumerate(fIt->second)) {
			AttachConfigLights(a_refData, data, index);
		}
	}

	if (auto mIt = gameModels.find(a_model); mIt != gameModels.end()) {
		for (const auto& [index, data] : std::views::enumerate(mIt->second)) {
			AttachConfigLights(a_refData, data, index);
		}
	}
}

void LightManager::AttachConfigLights(const ObjectRefData& a_refData, const AttachLightData& a_attachData, std::uint32_t a_index)
{
	RE::NiAVObject* lightPlacerNode = nullptr;
	auto            rootNode = a_refData.root;

	std::visit(overload{
				   [&](const PointData& pointData) {
					   auto name = std::format("{}{}", LightData::LP_NODE, a_index);
					   if (lightPlacerNode = rootNode->GetObjectByName(name); !lightPlacerNode) {
						   lightPlacerNode = RE::NiNode::Create(0);
						   lightPlacerNode->name = name;
						   RE::AttachNode(rootNode, lightPlacerNode);
					   }
					   if (lightPlacerNode) {
						   for (auto const& [pointIdx, point] : std::views::enumerate(pointData.points)) {
							   AttachLight(pointData.data, a_refData, lightPlacerNode->AsNode(), pointIdx, point);
						   }
					   }
				   },
				   [&](const NodeData& nodeData) {
					   for (const auto& nodeName : nodeData.nodes) {
						   if (lightPlacerNode = nodeData.data.GetOrCreateNode(rootNode, nodeName, a_index); lightPlacerNode) {
							   AttachLight(nodeData.data, a_refData, lightPlacerNode->AsNode());
						   }
					   }
				   },
				   [&](const FilteredData&) {
					   return;  // not handled here.
				   } },
		a_attachData);
}

void LightManager::AttachMeshLights(const ObjectRefData& a_refData, const std::string& a_model)
{
	std::int32_t LP_INDEX = 0;
	std::int32_t LP_ADDON_INDEX = 0;

	RE::BSVisit::TraverseScenegraphObjects(a_refData.root, [&](RE::NiAVObject* a_obj) {
		if (auto addonNode = netimmerse_cast<RE::BSValueNode*>(a_obj)) {
			if (auto it = gameAddonNodes.find(addonNode->value); it != gameAddonNodes.end()) {
				for (const auto& data : it->second) {
					if (auto& filteredData = std::get<FilteredData>(data); !filteredData.IsInvalid(a_model)) {
						AttachLight(filteredData.data, a_refData, addonNode, LP_ADDON_INDEX);
						LP_ADDON_INDEX++;
					}
				}
			}
		} else if (auto xData = a_obj->GetExtraData<RE::NiStringsExtraData>("LIGHT_PLACER"); xData && xData->value && xData->size > 0) {
			if (auto lightData = LightData(xData); lightData.IsValid()) {
				if (auto node = lightData.GetOrCreateNode(a_refData.root->AsNode(), a_obj, LP_INDEX)) {
					AttachLight(lightData, a_refData, node, LP_INDEX);
				}
				LP_INDEX++;
			}
		}
		return RE::BSVisit::BSVisitControl::kContinue;
	});
}

void LightManager::AttachLight(const LightData& a_lightData, const ObjectRefData& a_refData, RE::NiNode* a_node, std::uint32_t a_index, const RE::NiPoint3& a_point)
{
	auto& [ref, root, handle] = a_refData;

	if (auto bsLight = a_lightData.GenLight(ref, a_node, a_point, a_index); bsLight && bsLight->light) {
		if (RE::IsActor(ref)) {
			gameActorLights.write([&](auto& map) {
				map[handle].write([&](auto& nodeMap) {
					LightREFRData data(bsLight, ref, a_lightData);
					nodeMap[root].emplace(data);
					processedGameLights.write([&](auto& map) {
						map[ref->GetParentCell()->GetFormID()].write([&](auto& innerMap) {
							innerMap.emplace(data, handle);
						});
					});
				});
			});
		} else {
			gameRefLights.write([&](auto& map) {
				map[handle].emplace(bsLight, ref, a_lightData);
			});
		}
	}
}

void LightManager::DetachLights(RE::TESObjectREFR* a_ref, bool a_clearData)
{
	auto handle = a_ref->CreateRefHandle().native_handle();

	if (RE::IsActor(a_ref)) {
		gameActorLights.write([&](auto& map) {
			if (auto it = map.find(handle); it != map.end()) {
				it->second.write([&](auto& nodeMap) {
					for (auto& [node, lightDataVec] : nodeMap) {
						for (auto& lightRefrData : lightDataVec) {
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

void LightManager::ReattachWornLights(const RE::ActorHandle& a_handle)
{
	auto handle = a_handle.native_handle();

	gameActorLights.read([&](auto& map) {
		if (auto it = map.find(handle); it != map.end()) {
			it->second.read([&](auto& nodeMap) {
				for (auto& [node, lightDataVec] : nodeMap) {
					for (auto& lightRefrData : lightDataVec) {
						lightRefrData.ReattachLight();
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
