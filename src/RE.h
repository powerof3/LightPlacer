#pragma once

template <>
struct glz::meta<RE::NiPoint3>
{
	using T = RE::NiPoint3;
	static constexpr auto value = array(&T::x, &T::y, &T::z);
};

template <>
struct glz::meta<RE::NiMatrix3>
{
	static constexpr auto read = [](RE::NiMatrix3& input, const std::array<float, 3>& vec) {
		input.SetEulerAnglesXYZ(RE::deg_to_rad(vec[0]), RE::deg_to_rad(vec[1]), RE::deg_to_rad(vec[2]));
	};
	static constexpr auto write = [](auto& input) -> auto {
		std::array<float, 3> vec{};
		input.ToEulerAnglesXYZ(vec[0], vec[1], vec[2]);
		vec[0] = RE::rad_to_deg(vec[0]);
		vec[1] = RE::rad_to_deg(vec[1]);
		vec[2] = RE::rad_to_deg(vec[2]);
		return vec;
	};
	static constexpr auto value = array(custom<read, write>);
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

	std::string GetNodeName(TESObjectARMA* a_arma, const TESObjectREFR* a_refr, const TESObjectARMO* a_armor, float a_weightOverride);
	std::string GetNodeName(TESObjectWEAP* a_weap);

	TESBoundObject* GetReferenceEffectBase(const ReferenceEffect* a_referenceEffect);

	bool IsActor(const TESObjectREFR* a_ref);

	float NiSinQImpl(float a_value);
	float NiCosQImpl(float a_value);
	float NiSinQ(float a_radians);
	float NiCosQ(float a_radians);

	void UpdateLight(TESObjectLIGH* a_light, const NiPointer<NiPointLight>& a_ptLight, TESObjectREFR* a_ref, float a_wantDimmer);
}
