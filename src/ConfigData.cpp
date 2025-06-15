#include "ConfigData.h"
#include "SourceData.h"

void Config::Filter::PostProcess()
{
	constexpr auto post_process = [](StringSet& a_stringSet, FlatSet<RE::FormID>& a_FormIDSet) {
		for (auto& str : a_stringSet) {
			if (auto formID = RE::GetFormID(str); formID != 0) {
				a_FormIDSet.emplace(formID);
			}
		}
		a_stringSet.clear();
	};

	post_process(blackList, blackListForms);
	post_process(whiteList, whiteListForms);
}

bool Config::Filter::IsInvalid(const std::unique_ptr<SourceAttachData>& a_srcData) const
{
	if (!blackListForms.empty() && IsBlacklisted(a_srcData)) {
		return true;
	}

	if (!whiteListForms.empty() && !IsWhitelisted(a_srcData)) {
		return true;
	}

	return false;
}

bool Config::Filter::IsBlacklisted(const std::unique_ptr<SourceAttachData>& a_srcData) const
{
	for (const auto& id : a_srcData->filterIDs) {
		if (blackListForms.contains(id)) {
			return true;
		}
	}

	return false;
}

bool Config::Filter::IsWhitelisted(const std::unique_ptr<SourceAttachData>& a_srcData) const
{
	for (const auto& id : a_srcData->filterIDs) {
		if (whiteListForms.contains(id)) {
			return true;
		}
	}

	return false;
}

void Config::PostProcess(Config::LightSourceVec& a_lightDataVec)
{
	std::erase_if(a_lightDataVec, [](auto& attachLightData) {
		bool failedPostProcess = false;
		std::visit(overload{
					   [&](Config::FilteredPointData& pointData) {
						   failedPostProcess = !pointData.get().PostProcess();
						   if (!failedPostProcess) {
							   pointData.filter.PostProcess();
						   }
					   },
					   [&](Config::FilteredNodeData& nodeData) {
						   failedPostProcess = !nodeData.get().PostProcess();
						   if (!failedPostProcess) {
							   nodeData.filter.PostProcess();
						   }
					   } },
			attachLightData);
		return failedPostProcess;
	});
}

void Config::PostProcess(Config::AddonLightSourceVec& a_lightDataVec)
{
	std::erase_if(a_lightDataVec, [](auto& filterData) {
		bool failedPostProcess = !filterData.data.PostProcess();
		if (!failedPostProcess) {
			filterData.filter.PostProcess();
		}
		return failedPostProcess;
	});
}
