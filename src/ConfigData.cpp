#include "ConfigData.h"
#include "SourceData.h"

void Config::Filter::PostProcess()
{
	constexpr auto post_process = [](StringSet& a_stringSet, FlatSet<RE::FormID>& a_FormIDSet) {
		erase_if(a_stringSet, [&a_FormIDSet](const auto& str) {
			if (str.ends_with(".nif")) {
				return false;
			}
			if (auto formID = RE::GetFormID(str); formID != 0) {
				a_FormIDSet.emplace(formID);
			}
			return true;
		});
	};

	post_process(blackList, blackListForms);
	post_process(whiteList, whiteListForms);
}

bool Config::Filter::IsInvalid(const SourceData& a_srcData) const
{
	if (!blackList.empty() || !blackListForms.empty()) {
		if (IsBlacklisted(a_srcData)) {
			return true;
		}
	}

	if (!whiteList.empty() || !whiteListForms.empty()) {
		if (!IsWhitelisted(a_srcData)) {
			return true;
		}
	}

	return false;
}

bool Config::Filter::IsBlacklisted(const SourceData& a_srcData) const
{
	const auto refID = a_srcData.ref->GetFormID();
	const auto baseID = a_srcData.base->GetFormID();

	return blackList.contains(a_srcData.modelPath.data()) || stl::contains(blackListForms, refID, baseID, a_srcData.cellID, a_srcData.worldSpaceID, a_srcData.locationID);
}

bool Config::Filter::IsWhitelisted(const SourceData& a_srcData) const
{
	const auto refID = a_srcData.ref->GetFormID();
	const auto baseID = a_srcData.base->GetFormID();

	if (!whiteList.empty() && !whiteListForms.empty()) {
		return whiteList.contains(a_srcData.modelPath.data()) && stl::contains(whiteListForms, refID, baseID, a_srcData.cellID, a_srcData.worldSpaceID, a_srcData.locationID);
	}

	return whiteList.contains(a_srcData.modelPath.data()) || stl::contains(whiteListForms, refID, baseID, a_srcData.cellID, a_srcData.worldSpaceID, a_srcData.locationID);
}

void Config::PostProcess(Config::LightSourceVec& a_lightDataVec)
{
	std::erase_if(a_lightDataVec, [](auto& attachLightData) {
		bool failedPostProcess = false;
		std::visit(overload{
					   [&](Config::PointData& pointData) {
						   failedPostProcess = !pointData.data.PostProcess();
						   if (!failedPostProcess) {
							   pointData.filter.PostProcess();
						   }
					   },
					   [&](Config::NodeData& nodeData) {
						   failedPostProcess = !nodeData.data.PostProcess();
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
