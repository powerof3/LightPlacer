#include "LightsToUpdate.h"

void LightsToUpdate::Add(RE::RefHandle a_handle, RE::FormID a_cellFormID, bool a_update, bool a_updateEmittance, bool a_canBeMoved)
{
	if (!a_update && !a_updateEmittance) {
		return;
	}

	refs.try_emplace_and_visit(
		a_handle, QueuedRef{ a_cellFormID, a_update, a_updateEmittance, a_canBeMoved },
		[&](const auto& entry) {
			InsertIntoCell(a_handle, entry.second);
		},
		[&](auto& entry) {
			auto& queued = entry.second;
			queued.canBeMoved |= a_canBeMoved;
			if (queued.cellFormID == a_cellFormID && (queued.update || !a_update) && (queued.updateEmittance || !a_updateEmittance)) {
				return;
			}
			EraseFromCell(a_handle, queued.cellFormID);
			queued.cellFormID = a_cellFormID;
			queued.update |= a_update;
			queued.updateEmittance |= a_updateEmittance;
			InsertIntoCell(a_handle, queued);
		});
}

bool LightsToUpdate::Move(RE::RefHandle a_handle, RE::FormID a_cellFormID)
{
	bool movable = false;

	refs.visit(a_handle, [&](auto& entry) {
		auto& queued = entry.second;
		if (!queued.canBeMoved) {
			return;
		}
		movable = true;
		if (queued.cellFormID == a_cellFormID) {
			return;
		}
		EraseFromCell(a_handle, queued.cellFormID);
		queued.cellFormID = a_cellFormID;
		InsertIntoCell(a_handle, queued);
	});

	return movable;
}

void LightsToUpdate::Remove(RE::RefHandle a_handle)
{
	refs.erase_if(a_handle, [&](const auto& entry) {
		EraseFromCell(a_handle, entry.second.cellFormID);
		return true;
	});
}

void LightsToUpdate::Remove(RE::RefHandle a_handle, RE::FormID a_cellFormID)
{
	refs.erase_if(a_handle, [&](const auto& entry) {
		if (entry.second.cellFormID != a_cellFormID) {
			return false;
		}
		EraseFromCell(a_handle, a_cellFormID);
		return true;
	});
}

std::vector<RE::RefHandle> LightsToUpdate::GetRefs(RE::FormID a_cellFormID) const
{
	std::vector<RE::RefHandle> result;
	cells.cvisit(a_cellFormID, [&](const auto& entry) {
		result.assign(entry.second.updating.begin(), entry.second.updating.end());
	});
	return result;
}

std::vector<RE::RefHandle> LightsToUpdate::GetEmittanceRefs(RE::FormID a_cellFormID) const
{
	std::vector<RE::RefHandle> result;
	cells.cvisit(a_cellFormID, [&](const auto& entry) {
		result.assign(entry.second.emittance.begin(), entry.second.emittance.end());
	});
	return result;
}

void LightsToUpdate::InsertIntoCell(RE::RefHandle a_handle, const QueuedRef& a_queued)
{
	const auto insert = [&](auto& entry) {
		if (a_queued.update) {
			entry.second.updating.insert(a_handle);
		}
		if (a_queued.updateEmittance) {
			entry.second.emittance.insert(a_handle);
		}
	};
	cells.try_emplace_and_visit(a_queued.cellFormID, insert, insert);
}

void LightsToUpdate::EraseFromCell(RE::RefHandle a_handle, RE::FormID a_cellFormID)
{
	cells.erase_if(a_cellFormID, [&](auto& entry) {
		entry.second.updating.erase(a_handle);
		entry.second.emittance.erase(a_handle);
		return entry.second.updating.empty() && entry.second.emittance.empty();
	});
}
