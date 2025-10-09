#pragma once

#include "LightData.h"

struct SourceAttachData;

namespace Config
{
	struct Filter
	{
		void PostProcess();

		bool IsInvalid(const SourceAttachDataPtr& a_srcData) const;
		bool IsBlacklisted(const SourceAttachDataPtr& a_srcData) const;
		bool IsWhitelisted(const SourceAttachDataPtr& a_srcData) const;

		// members
		StringSet           whiteList;
		StringSet           blackList;
		FlatSet<RE::FormID> whiteListForms;
		FlatSet<RE::FormID> blackListForms;
	};

	template <class T>
	struct Data
	{
		T                     attacher;
		LIGH::LightSourceData data;
		std::string           path;
	};

	using PointData = Data<std::vector<RE::NiPoint3>>;
	using NodeData = Data<StringSet>;

	template <class T>
	struct FilteredData
	{
		LIGH::LightSourceData& get()
			requires !std::is_same_v<T, LIGH::LightSourceData>
		{
			return data.data;
		}

		Filter filter;
		T      data;
	};

	using FilteredRawData = FilteredData<LIGH::LightSourceData>;
	using AddonLightSourceVec = std::vector<FilteredRawData>;

	using FilteredPointData = FilteredData<PointData>;
	using FilteredNodeData = FilteredData<NodeData>;
	using LightSourceData = std::variant<FilteredPointData, FilteredNodeData>;
	using LightSourceVec = std::vector<LightSourceData>;

	struct MultiModelSet
	{
		StringSet      models;
		LightSourceVec lights;
	};

	struct MultiVisualEffectSet
	{
		StringSet      visualEffects;
		LightSourceVec lights;
	};

	// deprecated
	struct MultiAddonSet
	{
		FlatSet<std::uint32_t>       addonNodes;
		std::vector<FilteredRawData> lights;
	};

	using Format = std::variant<MultiModelSet, MultiVisualEffectSet, MultiAddonSet>;

	void PostProcess(LightSourceVec& a_lightDataVec, const std::string& path);
	void PostProcess(AddonLightSourceVec& a_lightDataVec);
}

template <>
struct glz::meta<Config::FilteredRawData>
{
	using T = Config::FilteredRawData;
	static constexpr auto value = object(
		"whiteList", [](auto&& self) -> auto& { return self.filter.whiteList; },
		"blackList", [](auto&& self) -> auto& { return self.filter.blackList; },
		"data", &T::data);
};

template <>
struct glz::meta<Config::FilteredPointData>
{
	using T = Config::FilteredPointData;
	static constexpr auto value = object(
		"whiteList", [](auto&& self) -> auto& { return self.filter.whiteList; },
		"blackList", [](auto&& self) -> auto& { return self.filter.blackList; },
		"points", [](auto&& self) -> auto& { return self.data.attacher; },
		"data", [](auto&& self) -> auto& { return self.data.data; });
};

template <>
struct glz::meta<Config::FilteredNodeData>
{
	using T = Config::FilteredNodeData;
	static constexpr auto value = object(
		"whiteList", [](auto&& self) -> auto& { return self.filter.whiteList; },
		"blackList", [](auto&& self) -> auto& { return self.filter.blackList; },
		"nodes", [](auto&& self) -> auto& { return self.data.attacher; },
		"data", [](auto&& self) -> auto& { return self.data.data; });
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
struct glz::meta<Config::MultiVisualEffectSet>
{
	using T = Config::MultiVisualEffectSet;
	static constexpr auto value = object(
		"visualEffects", &T::visualEffects,
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
