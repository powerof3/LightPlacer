#include "ConfigData.h"

void Config::Filter::PostProcess()
{
	constexpr auto post_process = [](FlatSet<std::string>& a_stringSet, FlatSet<RE::FormID>& a_FormIDSet) {
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

bool Config::Filter::IsInvalid(const ObjectREFRParams& a_refParams) const
{
	if (!blackList.empty() || !blackListForms.empty()) {
		if (IsBlacklisted(a_refParams)) {
			return true;
		}
	}

	if (!whiteList.empty() || !whiteListForms.empty()) {
		if (!IsWhitelisted(a_refParams)) {
			return true;
		}
	}

	return false;
}

bool Config::Filter::IsBlacklisted(const ObjectREFRParams& a_refParams) const
{
	return blackList.contains(a_refParams.modelPath) || (blackListForms.contains(a_refParams.cellID) || blackListForms.contains(a_refParams.worldSpaceID) || blackListForms.contains(a_refParams.locationID));
}

bool Config::Filter::IsWhitelisted(const ObjectREFRParams& a_refParams) const
{
	if (!whiteList.empty() && !whiteListForms.empty()) {
		return whiteList.contains(a_refParams.modelPath) && (whiteListForms.contains(a_refParams.cellID) || whiteListForms.contains(a_refParams.worldSpaceID) || whiteListForms.contains(a_refParams.locationID));
	}

	return whiteList.contains(a_refParams.modelPath) || (whiteListForms.contains(a_refParams.cellID) || whiteListForms.contains(a_refParams.worldSpaceID) || whiteListForms.contains(a_refParams.locationID));
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
