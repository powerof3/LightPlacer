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

		StringSet whiteList;
		StringSet blackList;

		FlatSet<RE::FormID> whiteListForms;
		FlatSet<RE::FormID> blackListForms;
	};

	struct FilterData
	{
		Filter          filter;
		LightSourceData data;
	};

	struct PointData
	{
		Filter                    filter;
		std::vector<RE::NiPoint3> points;
		LightSourceData           data;
	};

	struct NodeData
	{
		Filter          filter;
		StringSet       nodes;
		LightSourceData data;
	};

	using LightSourceData = std::variant<PointData, NodeData>;
	using LightSourceVec = std::vector<LightSourceData>;
	using AddonLightSourceVec = std::vector<FilterData>;

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
		FlatSet<std::uint32_t>  addonNodes;
		std::vector<FilterData> lights;
	};

	using Format = std::variant<MultiModelSet, MultiVisualEffectSet, MultiAddonSet>;

	void PostProcess(LightSourceVec& a_lightDataVec);
	void PostProcess(AddonLightSourceVec& a_lightDataVec);
}

template <>
struct glz::meta<Config::FilterData>
{
	using T = Config::FilterData;
	static constexpr auto value = object(
		"whiteList", [](auto&& self) -> auto& { return self.filter.whiteList; },
		"blackList", [](auto&& self) -> auto& { return self.filter.blackList; },
		"data", &T::data);
};

template <>
struct glz::meta<Config::PointData>
{
	using T = Config::PointData;
	static constexpr auto value = object(
		"whiteList", [](auto&& self) -> auto& { return self.filter.whiteList; },
		"blackList", [](auto&& self) -> auto& { return self.filter.blackList; },
		"points", &T::points,
		"data", &T::data);
};

template <>
struct glz::meta<Config::NodeData>
{
	using T = Config::NodeData;
	static constexpr auto value = object(
		"whiteList", [](auto&& self) -> auto& { return self.filter.whiteList; },
		"blackList", [](auto&& self) -> auto& { return self.filter.blackList; },
		"nodes", &T::nodes,
		"data", &T::data);
};
