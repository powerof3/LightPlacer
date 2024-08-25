#pragma once

#include "LightData.h"

class LightManager : public ISingleton<LightManager>
{
public:
	bool ReadConfigs();
	void LoadFormsFromConfig();

	void TryAttachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base);
	void TryAttachLights(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base, RE::NiAVObject* a_root);
	void DetachLights(RE::TESObjectREFR* a_ref);

	void UpdateFlickering(RE::TESObjectCELL* a_cell);
	void UpdateEmittance(RE::TESObjectCELL* a_cell);
	void ClearProcessedLights(RE::TESObjectREFR* a_ref);

private:
	struct Config
	{
		bool empty() const { return model.empty() && reference.empty(); }

		StringMap<AttachLightDataVec> model{};
		// only used for reading
		StringMap<AttachLightDataVec> reference{};
	};

	struct FlickerData
	{
		FlickerData(RE::TESObjectLIGH* a_light, RE::NiPointLight* a_ptLight) :
			light(a_light),
			ptLight(a_ptLight)
		{}

		RE::TESObjectLIGH*              light{};
		RE::NiPointer<RE::NiPointLight> ptLight{};
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

	void AttachConfigLights(const ObjectRefData& a_refData, RE::TESBoundObject* a_base);
	void AttachConfigLights(const ObjectRefData& a_refData, const AttachLightData& a_attachData, std::uint32_t a_index);

	void AttachMeshLights(const ObjectRefData& a_refData);
	void SpawnAndProcessLight(const LightData& a_lightData, const ObjectRefData& a_refData, RE::NiNode* a_node, const RE::NiPoint3& a_point = RE::NiPoint3::Zero(), std::uint32_t a_index = 0);

	// members
	Config                              config{};
	Map<RE::FormID, AttachLightDataVec> gameReferences{};

	Map<RE::FormID, Map<RE::RefHandle, std::vector<FlickerData>>>                      flickeringRefs{};
	Map<RE::FormID, Map<RE::RefHandle, Map<RE::TESForm*, std::vector<EmittanceData>>>> emittanceRefs{};
};
