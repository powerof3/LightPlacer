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
	TryAttachLightsImpl(refData, a_base);
}

void LightManager::TryAttachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base, RE::NiAVObject* a_root)
{
	if (!a_ref || !a_base) {
		return;
	}

	ObjectRefData refData(a_ref, a_root);
	TryAttachLightsImpl(refData, a_base);
}

void LightManager::TryAttachLightsImpl(const ObjectRefData& a_refData, RE::TESBoundObject* a_object)
{
	if (!a_refData.IsValid()) {
		return;
	}

	auto model = a_object->As<RE::TESModel>();
	if (!model) {
		return;
	}

	auto modelPath = RE::SanitizeModel(model->GetModel());
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
					   auto& [points, lightData] = pointData;
					   auto name = std::format("{}{}", LightData::LP_NODE, a_index);
					   if (lightPlacerNode = rootNode->GetObjectByName(name); !lightPlacerNode) {
						   lightPlacerNode = RE::NiNode::Create(0);
						   lightPlacerNode->name = name;
						   RE::AttachNode(rootNode, lightPlacerNode);
					   }
					   if (lightPlacerNode) {
						   for (auto const& [pointIdx, point] : std::views::enumerate(points)) {
							   AttachLight(lightData, a_refData, lightPlacerNode->AsNode(), pointIdx, point);
						   }
					   }
				   },
				   [&](const NodeData& nodeData) {
					   auto& [nodes, lightData] = nodeData;
					   for (const auto& nodeName : nodes) {
						   if (lightPlacerNode = lightData.GetOrCreateNode(rootNode, nodeName, a_index); lightPlacerNode) {
							   AttachLight(lightData, a_refData, lightPlacerNode->AsNode());
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

	if (auto light = a_lightData.SpawnLight(ref, a_node, a_point, a_index); light) {
		gameLightsData.write([&](auto& map) {
			map[handle].emplace(light, ref, a_lightData);
		});
	}
}

void LightManager::DetachLights(RE::TESObjectREFR* a_ref, bool a_clearData)
{
	auto  handle = a_ref->CreateRefHandle().native_handle();
	auto* shadowSceneNode = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];

	gameLightsData.write([&](auto& map) {
		if (auto it = map.find(handle); it != map.end()) {
			for (auto& lightRefrData : it->second) {
				if (auto& ptLight = lightRefrData.ptLight) {
					shadowSceneNode->RemoveLight(ptLight.get());
				}
			}
			if (a_clearData) {
				map.erase(it);
			}
		}
	});
}

void LightManager::AddLightsToProcessQueue(RE::TESObjectCELL* a_cell, RE::TESObjectREFR* a_ref)
{
	auto cellFormID = a_cell->GetFormID();
	auto handle = a_ref->CreateRefHandle().native_handle();

	ForEachLight(handle, [&](const auto& lightREFRData) {
		processedGameLights[cellFormID].emplace(lightREFRData, handle);
	});
}

void LightManager::UpdateFlickeringAndConditions(RE::TESObjectCELL* a_cell)
{
	if (auto it = processedGameLights.find(a_cell->GetFormID()); it != processedGameLights.end()) {
		std::scoped_lock locker(*it->second._lock);

		it->second.lastUpdateTime += RE::GetSecondsSinceLastFrame();
		if (it->second.lastUpdateTime >= 0.1f) {
			it->second.lastUpdateTime = 0.0f;

			std::erase_if(it->second.conditionalLights, [&](const auto& handle) {
				RE::TESObjectREFRPtr ref{};
				RE::LookupReferenceByHandle(handle, ref);

				if (!ref) {
					return true;
				}

				ForEachLight(handle, [&](const auto& lightREFRData) {
					lightREFRData.UpdateConditions(ref);
				});

				return false;
			});
		}

		static auto flickeringDistance = RE::GetINISetting("fFlickeringLightDistance:General")->GetFloat() * RE::GetINISetting("fFlickeringLightDistance:General")->GetFloat();
		auto        pcPos = RE::PlayerCharacter::GetSingleton()->GetPosition();

		std::erase_if(it->second.flickeringLights, [&](const auto& handle) {
			RE::TESObjectREFRPtr ref{};
			RE::LookupReferenceByHandle(handle, ref);

			if (!ref) {
				return true;
			}

			if (ref->GetPosition().GetSquaredDistance(pcPos) < flickeringDistance) {
				ForEachLight(handle, [&](const auto& lightREFRData) {
					lightREFRData.UpdateFlickering(ref);
				});
			}
			return false;
		});
	}
}

void LightManager::UpdateEmittance(RE::TESObjectCELL* a_cell)
{
	if (auto it = processedGameLights.find(a_cell->GetFormID()); it != processedGameLights.end()) {
		std::scoped_lock locker(*it->second._lock);

		std::erase_if(it->second.emittanceLights, [&](const auto& handle) {
			RE::TESObjectREFRPtr ref{};
			RE::LookupReferenceByHandle(handle, ref);

			if (!ref) {
				return true;
			}

			ForEachLight(handle, [&](const auto& lightREFRData) {
				lightREFRData.UpdateEmittance(ref);
			});

			return false;
		});
	}
}

void LightManager::RemoveLightsFromProcessQueue(RE::TESObjectCELL* a_cell, const RE::ObjectRefHandle& a_handle)
{
	if (auto it = processedGameLights.find(a_cell->GetFormID()); it != processedGameLights.end()) {
		it->second.erase(a_handle.native_handle());
	}
}
