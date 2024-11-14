#pragma once

template <>
struct glz::meta<RE::NiPoint3>
{
	using T = RE::NiPoint3;
	static constexpr auto value = array(&T::x, &T::y, &T::z);
};

namespace RE
{
	template <class T>
	void AttachNode(RE::NiNode* a_root, T* a_obj)
	{
		if (RE::TaskQueueInterface::ShouldUseTaskQueue()) {
			RE::TaskQueueInterface::GetSingleton()->QueueNodeAttach(a_obj, a_root);
		} else {
			a_root->AttachChild(a_obj);
		}
	}

	inline void UpdateLight(TESObjectLIGH* a_light, const NiPointer<NiPointLight>& a_ptLight, TESObjectREFR* a_ref, float a_wantDimmer)
	{
		using func_t = decltype(&RE::UpdateLight);
		static REL::Relocation<func_t> func{ RELOCATION_ID(17212, 17614) };
		return func(a_light, a_ptLight, a_ref, a_wantDimmer);
	}

	inline std::string SanitizeModel(const std::string& a_path)
	{
		auto path = string::tolower(a_path);

		path = srell::regex_replace(path, srell::regex("/+|\\\\+"), "\\");
		path = srell::regex_replace(path, srell::regex("^\\\\+"), "");
		path = srell::regex_replace(path, srell::regex(R"(.*?[^\s]meshes\\|^meshes\\)", srell::regex::icase), "");

		return path;
	}

	inline FormID GetFormID(const std::string& a_str)
	{
		if (const auto splitID = string::split(a_str, "~"); splitID.size() == 2) {
			const auto  formID = string::to_num<FormID>(splitID[0], true);
			const auto& modName = splitID[1];
			if (g_mergeMapperInterface) {
				const auto [mergedModName, mergedFormID] = g_mergeMapperInterface->GetNewFormID(modName.c_str(), formID);
				return TESDataHandler::GetSingleton()->LookupFormID(mergedFormID, mergedModName);
			} else {
				return TESDataHandler::GetSingleton()->LookupFormID(formID, modName);
			}
		}
		if (string::is_only_hex(a_str, true)) {
			return string::to_num<RE::FormID>(a_str, true);
		}
		if (const auto form = RE::TESForm::LookupByEditorID(a_str)) {
			return form->GetFormID();
		}
		return static_cast<RE::FormID>(0);
	}

	template <class T>
	inline T* GetFormFromID(const std::string& a_str)
	{
		auto formID = GetFormID(a_str);
		return formID != 0 ? RE::TESForm::LookupByID<T>(GetFormID(a_str)) : nullptr;
	}
}
