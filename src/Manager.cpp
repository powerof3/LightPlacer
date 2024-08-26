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

void LightManager::LoadFormsFromConfig()
{
	for (auto& multiData : config) {
		std::visit(overload{
					   [&](Config::MultiModelSet& models) {
						   LoadFormsFromAttachLightVec(models.lightData);
						   for (auto& model : models.models) {
							   gameModels[model].append_range(models.lightData);
						   }
					   },
					   [&](Config::MultiReferenceSet& references) {
						   LoadFormsFromAttachLightVec(references.lightData);
						   for (auto& rawID : references.references) {
							   gameReferences[RE::GetFormID(rawID)].append_range(references.lightData);
						   }
					   },
					   [&](Config::MultiAddonSet& addonNodes) {
						   LoadFormsFromAttachLightVec(addonNodes.lightData);
						   for (auto& index: addonNodes.addonNodes) {
							   gameAddonNodes[index].append_range(addonNodes.lightData);
						   }
					   } },
			multiData);
	}
}

void LightManager::TryAttachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base)
{
	ObjectRefData refData(a_ref);

	if (!refData.IsValid() || !a_base || a_base->Is(RE::FormType::Light)) {
		return;
	}

	auto model = a_base->As<RE::TESModel>();
	if (!model) {
		return;
	}

	std::string modelPath = RE::SanitizeModel(model->GetModel());

	AttachConfigLights(refData, modelPath, a_base->GetFormID());
	AttachMeshLights(refData, modelPath);
}

void LightManager::TryAttachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base, RE::NiAVObject* a_root)
{
	ObjectRefData refData(a_ref, a_root);

	if (!refData.IsValid() || !a_base) {
		return;
	}

	auto model = a_base->As<RE::TESModel>();
	if (!model) {
		return;
	}

	std::string modelPath = RE::SanitizeModel(model->GetModel());

	AttachConfigLights(refData, modelPath, a_base->GetFormID());
	AttachMeshLights(refData, modelPath);
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
					   auto name = std::format("LightPlacerNode {}", a_index);
					   if (lightPlacerNode = rootNode->GetObjectByName(name); !lightPlacerNode) {
						   if (lightPlacerNode = RE::NiNode::Create(0); lightPlacerNode) {
							   lightPlacerNode->name = name;
							   RE::AttachNode(rootNode, lightPlacerNode);
							   for (auto const& [pointIdx, point] : std::views::enumerate(points)) {
								   SpawnAndProcessLight(lightData, a_refData, lightPlacerNode->AsNode(), point, pointIdx);
							   }
						   }
					   } else {
						   lightData.ReattachExistingLights(a_refData.ref, lightPlacerNode);
					   }
				   },
				   [&](const NodeData& nodeData) {
					   auto& [nodes, lightData] = nodeData;
					   for (const auto& nodeName : nodes) {
						   if (lightPlacerNode = lightData.GetOrCreateNode(rootNode, nodeName, a_index); lightPlacerNode) {
							   if (lightData.ReattachExistingLights(a_refData.ref, lightPlacerNode) == 0) {
								   SpawnAndProcessLight(lightData, a_refData, lightPlacerNode->AsNode());
							   }
						   }
					   }
				   },
				   [&](const FilteredData&) {
					   return; // not handled here.
				   } },
		a_attachData);
}

void LightManager::AttachMeshLights(const ObjectRefData& a_refData, const std::string& a_model)
{
	RE::BSVisit::TraverseScenegraphObjects(a_refData.root, [&](RE::NiAVObject* a_obj) {
		if (auto addonNode = netimmerse_cast<RE::BSValueNode*>(a_obj)) {
			if (auto it = gameAddonNodes.find(addonNode->value); it != gameAddonNodes.end()) {
				for (const auto& [index, data] : std::views::enumerate(it->second)) {
					if (auto& filteredData = std::get<FilteredData>(data); !filteredData.IsInvalid(a_model)) {
						if (filteredData.data.ReattachExistingLights(a_refData.ref, addonNode) == 0) {
							SpawnAndProcessLight(filteredData.data, a_refData, addonNode);
						}
					}
				}
			}
		}		
		if (auto xData = a_obj->GetExtraData<RE::NiStringsExtraData>("LIGHT_PLACER"); xData && xData->value && xData->size > 0) {
			auto lightData = LightData(xData);
			if (auto node = lightData.GetOrCreateNode(a_refData.root->AsNode(), a_obj, 0)) {
				if (lightData.ReattachExistingLights(a_refData.ref, node) == 0) {
					SpawnAndProcessLight(lightData, a_refData, node);
				}
			}
		}
		return RE::BSVisit::BSVisitControl::kContinue;
	});
}

void LightManager::SpawnAndProcessLight(const LightData& a_lightData, const ObjectRefData& a_refData, RE::NiNode* a_node, const RE::NiPoint3& a_point, std::uint32_t a_index)
{
	auto& [ref, root, cellFormID, handle] = a_refData;

	if (auto [light, flickers, emittance] = a_lightData.SpawnLight(ref, a_node, a_point, a_index); light) {
		if (flickers) {
			flickeringRefs[cellFormID][handle].emplace_back(a_lightData.light, light, a_lightData.GetFade());
		}
		if (emittance) {
			emittanceRefs[cellFormID][handle][emittance].emplace_back(a_lightData.GetDiffuse(), light);
		}
	}
}

void LightManager::DetachLights(RE::TESObjectREFR* a_ref)
{
	if (auto root = a_ref->Get3D()) {
		auto* shadowSceneNode = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
		RE::BSVisit::TraverseScenegraphLights(root, [&](RE::NiPointLight* ptLight) {
			if (ptLight->name.contains("LP|")) {
				shadowSceneNode->RemoveLight(ptLight);
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});
	}
}

void LightManager::ClearProcessedLights(RE::TESObjectREFR* a_ref)
{
	if (auto parentCell = a_ref->parentCell) {
		auto handle = a_ref->GetHandle().native_handle();
		if (auto it = flickeringRefs.find(parentCell->GetFormID()); it != flickeringRefs.end()) {
			if (auto hIt = it->second.find(handle); hIt != it->second.end()) {
				hIt->second.clear();
			}
		}
		if (auto it = emittanceRefs.find(parentCell->GetFormID()); it != emittanceRefs.end()) {
			if (auto hIt = it->second.find(handle); hIt != it->second.end()) {
				hIt->second.clear();
			}
		}
	}
}

void LightManager::UpdateFlickering(RE::TESObjectCELL* a_cell)
{
	if (auto it = flickeringRefs.find(a_cell->GetFormID()); it != flickeringRefs.end()) {
		static auto flickeringDistance = RE::GetINISetting("fFlickeringLightDistance:General")->GetFloat() * RE::GetINISetting("fFlickeringLightDistance:General")->GetFloat();
		auto        pcPos = RE::PlayerCharacter::GetSingleton()->GetPosition();

		std::erase_if(it->second, [&](auto& map) {
			auto& [handle, lightVec] = map;
			RE::TESObjectREFRPtr ref{};
			if (RE::LookupReferenceByHandle(handle, ref); ref) {
				if (ref->GetPosition().GetSquaredDistance(pcPos) < flickeringDistance) {
					for (auto& [light, ptLight, fade] : lightVec) {
						if (light && ptLight) {
							auto originalFade = light->fade;
							light->fade = fade;
							RE::UpdateLight(light, ptLight, ref.get(), -1.0f);
							light->fade = originalFade;
						}
					}
				}
				return false;
			}
			return true;
		});
	}
}

void LightManager::UpdateEmittance(RE::TESObjectCELL* a_cell)
{
	if (auto it = emittanceRefs.find(a_cell->GetFormID()); it != emittanceRefs.end()) {
		RE::NiColor emittanceColor(1.0, 1.0, 1.0);

		std::erase_if(it->second, [&emittanceColor](auto& map) {
			auto [handle, emittanceMap] = map;
			RE::TESObjectREFRPtr ref{};
			if (RE::LookupReferenceByHandle(handle, ref); ref) {
				for (auto& [src, lights] : emittanceMap) {
					if (src) {
						if (auto light = src->As<RE::TESObjectLIGH>()) {
							emittanceColor = light->emittanceColor;
						} else if (auto region = src->As<RE::TESRegion>()) {
							emittanceColor = region->emittanceColor;
						}
						for (auto& [diffuse, light] : lights) {
							if (light) {
								light->diffuse = diffuse * emittanceColor;
							}
						}
					}
				}
				return false;
			}
			return true;
		});
	}
}
