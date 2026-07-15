#pragma once

#include "LightData.h"

struct SourceAttachData;

namespace Config
{
	struct Filter
	{
		void PostProcess();

		bool IsInvalid(const SourceAttachData& a_srcData) const;
		bool IsBlacklisted(const SourceAttachData& a_srcData) const;
		bool IsWhitelisted(const SourceAttachData& a_srcData) const;

		// members
		StringSet           whiteList;
		StringSet           blackList;
		FlatSet<RE::FormID> whiteListForms;
		FlatSet<RE::FormID> blackListForms;
	};

	template <class T>
	struct Placement
	{
		T                                  attacher;
		LIGH::LightDefinition              definition;
		std::shared_ptr<const std::string> path;
	};

	using PointPlacement = Placement<std::vector<RE::NiPoint3>>;
	using NodePlacement = Placement<StringSet>;
	using PointPlacementPtr = std::shared_ptr<const PointPlacement>;
	using NodePlacementPtr = std::shared_ptr<const NodePlacement>;

	template <class T>
	struct Filtered
	{
		LIGH::LightDefinition& get()
			requires !std::is_same_v<T, LIGH::LightDefinition>
		{
			return data.definition;
		}

		Filter filter;
		T      data;
	};

	// deprecated
	using AddonEntry = Filtered<LIGH::LightDefinition>;
	using AddonEntries = std::vector<AddonEntry>;

	using PointEntry = Filtered<PointPlacement>;
	using NodeEntry = Filtered<NodePlacement>;

	using LightEntry = std::variant<PointEntry, NodeEntry>;
	using LightEntryPtr = std::shared_ptr<const LightEntry>;
	using LightEntries = std::vector<LightEntry>;

	using LightEntryGroup = std::vector<LightEntryPtr>;                 // entries applying to one model/formID
	using LightEntryGroupPtr = std::shared_ptr<const LightEntryGroup>;  // one group shared across keys

	struct MultiModelSet
	{
		StringSet    models;
		LightEntries lights;
	};

	struct MultiFormIDSet
	{
		StringSet    formIDs;
		LightEntries lights;
	};

	// deprecated
	struct MultiAddonSet
	{
		FlatSet<std::uint32_t>  addonNodes;
		std::vector<AddonEntry> lights;
	};

	using Format = std::variant<MultiModelSet, MultiFormIDSet, MultiAddonSet>;

	void PostProcess(LightEntries& a_lightEntries, const std::shared_ptr<const std::string>& a_path);
	void PostProcess(AddonEntries& a_lightDataVec);
}

template <>
struct glz::meta<Config::AddonEntry>
{
	using T = Config::AddonEntry;
	static constexpr auto value = object(
		"whiteList", [](auto&& self) -> auto& { return self.filter.whiteList; },
		"blackList", [](auto&& self) -> auto& { return self.filter.blackList; },
		"data", &T::data);
};

template <>
struct glz::meta<Config::PointEntry>
{
	using T = Config::PointEntry;
	static constexpr auto value = object(
		"whiteList", [](auto&& self) -> auto& { return self.filter.whiteList; },
		"blackList", [](auto&& self) -> auto& { return self.filter.blackList; },
		"points", [](auto&& self) -> auto& { return self.data.attacher; },
		"data", [](auto&& self) -> auto& { return self.data.definition; });
};

template <>
struct glz::meta<Config::NodeEntry>
{
	using T = Config::NodeEntry;
	static constexpr auto value = object(
		"whiteList", [](auto&& self) -> auto& { return self.filter.whiteList; },
		"blackList", [](auto&& self) -> auto& { return self.filter.blackList; },
		"nodes", [](auto&& self) -> auto& { return self.data.attacher; },
		"data", [](auto&& self) -> auto& { return self.data.definition; });
};

template <>
struct glz::meta<Config::MultiModelSet>
{
	using T = Config::MultiModelSet;
	static constexpr auto value = object(
		"models", &T::models,
		"lights", &T::lights);
};

template <>
struct glz::meta<Config::MultiFormIDSet>
{
	using T = Config::MultiFormIDSet;
	static constexpr auto modify = glz::object(
		"formIDs", &T::formIDs,
		"visualEffects", [](auto& self) -> auto& { return self.formIDs; },
		"lights", &T::lights);
};

template <>
struct glz::meta<Config::MultiAddonSet>
{
	using T = Config::MultiAddonSet;
	static constexpr auto value = object(
		"addonNodes", &T::addonNodes,
		"lights", &T::lights);
};
