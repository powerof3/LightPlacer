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
	static constexpr auto value = custom<read, write>;
};

template <>
struct glz::meta<RE::NiColor>
{
	static constexpr auto read = [](RE::NiColor& input, const std::array<float, 3>& vec) {
		for (std::size_t i = 0; i < RE::NiColor::kTotal; ++i) {
			if (vec[i] >= -1.0f && vec[i] <= 1.0f) {
				input[i] = vec[i];
			} else {
				input[i] = vec[i] / 255;
			}
		}
	};
	static constexpr auto write = [](auto const& input) -> auto {
		return std::array{ input.red, input.green, input.blue };
	};
	static constexpr auto value = custom<read, write>;
};

namespace RE
{
	static constexpr NiPoint3 POINT_MAX{ NI_INFINITY, NI_INFINITY, NI_INFINITY };

	static constexpr NiColor COLOR_BLACK{ 0.0f, 0.0f, 0.0f };
	static constexpr NiColor COLOR_WHITE{ 1.0f, 1.0f, 1.0f };
	static constexpr NiColor COLOR_MAX{ NI_INFINITY, NI_INFINITY, NI_INFINITY };

	static constexpr NiMatrix3 MATRIX_ZERO{};

	using NiNodePtr = NiPointer<NiNode>;

	template <class T, class U>
	void AttachNode(const U a_root, T* a_obj)
	{
		if (TaskQueueInterface::ShouldUseTaskQueue()) {
			if constexpr (std::is_same_v<U, NiNodePtr>) {
				TaskQueueInterface::GetSingleton()->QueueNodeAttach(a_obj, a_root.get());
			} else {
				TaskQueueInterface::GetSingleton()->QueueNodeAttach(a_obj, a_root);
			}
		} else {
			a_root->AttachChild(a_obj, true);
		}
	}

	template <class T>
	void UpdateNode(T* a_obj)
	{
		if (TaskQueueInterface::ShouldUseTaskQueue()) {
			TaskQueueInterface::GetSingleton()->QueueUpdateNiObject(a_obj);
		} else {
			NiUpdateData updateData;
			a_obj->Update(updateData);
		}
	}

	FormID GetFormID(const std::string& a_str);
	template <class T>
	T* GetFormFromID(const std::string& a_str)
	{
		auto formID = GetFormID(a_str);
		return formID != 0 ? TESForm::LookupByID<T>(GetFormID(a_str)) : nullptr;
	}

#ifndef SKYRIMVR
	const char*  GetGameVersionImpl();
	REL::Version GetGameVersion();
#endif

	NiAVObject*     GetReferenceAttachRoot(ReferenceEffect* a_referenceEffect);
	TESBoundObject* GetReferenceEffectBase(const TESObjectREFRPtr& a_ref, const ReferenceEffect* a_referenceEffect);
	BGSArtObject*   GetCastingArt(const MagicItem* a_magicItem);
	BGSArtObject*   GetCastingArt(const ActorMagicCaster* a_actorMagicCaster);
	NiNode*         GetCastingArtNode(ActorMagicCaster* a_actorMagicCaster);
	NiAVObject*     GetObjectByName(RE::NiAVObject* a_root, std::string_view a_name);
	bool            IsDynDOLODForm(const TESObjectREFR* a_ref);
	float           NiSinQImpl(float a_value);
	float           NiCosQImpl(float a_value);
	float           NiSinQ(float a_radians);
	float           NiCosQ(float a_radians);
	bool            ToggleMasterParticleAddonNodes(const NiNode* a_node, bool a_enable);
	void            UpdateLight(TESObjectLIGH* a_light, const NiPointer<NiPointLight>& a_ptLight, TESObjectREFR* a_ref, float a_wantDimmer);
	void            WrapRotation(NiPoint3& a_rotation);
}
