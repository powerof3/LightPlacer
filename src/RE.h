#pragma once

template <>
struct glz::meta<RE::NiPoint3>
{
	using T = RE::NiPoint3;
	static constexpr auto value = array(&T::x, &T::y, &T::z);
};

template <>
struct glz::meta<RE::NiColor>
{
	using T = RE::NiColor;
	static constexpr auto value = array(&T::red, &T::green, &T::blue);
};

namespace RE
{
	static constexpr NiColor COLOR_BLACK(0, 0, 0);

	template <class T>
	void AttachNode(NiNode* a_root, T* a_obj)
	{
		if (TaskQueueInterface::ShouldUseTaskQueue()) {
			TaskQueueInterface::GetSingleton()->QueueNodeAttach(a_obj, a_root);
		} else {
			a_root->AttachChild(a_obj, true);
		}
	}

	FormID GetFormID(const std::string& a_str);

	template <class T>
	T* GetFormFromID(const std::string& a_str)
	{
		auto formID = GetFormID(a_str);
		return formID != 0 ? TESForm::LookupByID<T>(GetFormID(a_str)) : nullptr;
	}

	const char*  GetGameVersionImpl();
	REL::Version GetGameVersion();

	TESBoundObject* GetReferenceEffectBase(const ReferenceEffect* a_referenceEffect);

	bool IsActor(const TESObjectREFR* a_ref);

	float NiSinQImpl(float a_value);
	float NiCosQImpl(float a_value);
	float NiSinQ(float a_radians);
	float NiCosQ(float a_radians);

	std::string SanitizeModel(const std::string& a_path);

	void UpdateLight(TESObjectLIGH* a_light, const NiPointer<NiPointLight>& a_ptLight, TESObjectREFR* a_ref, float a_wantDimmer);
}
