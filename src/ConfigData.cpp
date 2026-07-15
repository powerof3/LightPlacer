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

bool Config::Filter::IsInvalid(const SourceAttachData& a_srcData) const
{
	if (!blackListForms.empty() && IsBlacklisted(a_srcData)) {
		return true;
	}

	if (!whiteListForms.empty() && !IsWhitelisted(a_srcData)) {
		return true;
	}

	return false;
}

bool Config::Filter::IsBlacklisted(const SourceAttachData& a_srcData) const
{
	for (const auto& id : a_srcData.filterIDs) {
		if (blackListForms.contains(id)) {
			return true;
		}
	}

	return false;
}

bool Config::Filter::IsWhitelisted(const SourceAttachData& a_srcData) const
{
	for (const auto& id : a_srcData.filterIDs) {
		if (whiteListForms.contains(id)) {
			return true;
		}
	}

	return false;
}

void Config::PostProcess(LightEntries& a_lightEntries, const std::shared_ptr<const std::string>& a_path)
{
	std::erase_if(a_lightEntries, [&](auto& lightEntry) {
		bool failedPostProcess = false;
		std::visit(overload{
					   [&](Config::PointEntry& pointEntry) {
						   failedPostProcess = !pointEntry.get().PostProcess();
						   if (!failedPostProcess) {
							   pointEntry.filter.PostProcess();
							   pointEntry.data.path = a_path;
						   }
					   },
					   [&](Config::NodeEntry& nodeEntry) {
						   failedPostProcess = !nodeEntry.get().PostProcess();
						   if (!failedPostProcess) {
							   nodeEntry.filter.PostProcess();
							   nodeEntry.data.path = a_path;
						   }
					   } },
			lightEntry);
		return failedPostProcess;
	});
}

void Config::PostProcess(Config::AddonEntries& a_addonLights)
{
	std::erase_if(a_addonLights, [&](auto& filterData) {
		bool failedPostProcess = !filterData.data.PostProcess();
		if (!failedPostProcess) {
			filterData.filter.PostProcess();
		}
		return failedPostProcess;
	});
}
