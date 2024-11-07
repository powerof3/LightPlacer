#pragma once

#include "LightData.h"

class LightManager : public ISingleton<LightManager>
{
public:
	bool ReadConfigs();
	void OnDataLoad();

	void TryAttachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base);
	void TryAttachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base, RE::NiAVObject* a_root);
	
	void DetachLights(RE::TESObjectREFR* a_ref);

	void UpdateFlickering(RE::TESObjectCELL* a_cell);
	void UpdateEmittance(RE::TESObjectCELL* a_cell);
	void ClearProcessedLights(RE::TESObjectREFR* a_ref);

private:
	struct Config
	{
		struct MultiModelSet
		{
			StringSet          models;
			AttachLightDataVec lightData;
		};

		struct MultiReferenceSet
		{
			StringSet          references;
			AttachLightDataVec lightData;
		};

		struct MultiAddonSet
		{
			Set<std::uint32_t> addonNodes;
			AttachLightDataVec lightData;
		};

		using Format = std::variant<MultiModelSet, MultiReferenceSet, MultiAddonSet>;
	};

	struct FlickerData
	{
		FlickerData(RE::TESObjectLIGH* a_light, RE::NiPointLight* a_ptLight, float a_fade) :
			light(a_light),
			ptLight(a_ptLight),
			fade(a_fade)
		{}

		RE::TESObjectLIGH*              light{};
		RE::NiPointer<RE::NiPointLight> ptLight{};
		float                           fade{};
	};

	struct EmittanceData
	{
		EmittanceData(RE::NiColor a_diffuse, RE::NiPointLight* a_ptLight) :
			diffuse(std::move(a_diffuse)),
			ptLight(a_ptLight)
		{}

		RE::NiColor                     diffuse{};
		RE::NiPointer<RE::NiPointLight> ptLight{};
	};

	void TryAttachLightsImpl(const ObjectRefData& a_refData, RE::TESBoundObject* a_object);
	
	void AttachConfigLights(const ObjectRefData& a_refData, const std::string& a_model, RE::FormID a_baseFormID);
	void AttachConfigLights(const ObjectRefData& a_refData, const AttachLightData& a_attachData, std::uint32_t a_index);

	void AttachMeshLights(const ObjectRefData& a_refData, const std::string& a_model);
	void SpawnAndProcessLight(const LightData& a_lightData, const ObjectRefData& a_refData, RE::NiNode* a_node, const RE::NiPoint3& a_point = RE::NiPoint3::Zero(), std::uint32_t a_index = 0);

	// members
	std::vector<Config::Format>            config{};
	StringMap<AttachLightDataVec>          gameModels{};
	Map<RE::FormID, AttachLightDataVec>    gameReferences{};
	Map<std::uint32_t, AttachLightDataVec> gameAddonNodes{};

	Map<RE::FormID, Map<RE::RefHandle, std::vector<FlickerData>>>                      flickeringRefs{};
	Map<RE::FormID, Map<RE::RefHandle, Map<RE::TESForm*, std::vector<EmittanceData>>>> emittanceRefs{};
};
