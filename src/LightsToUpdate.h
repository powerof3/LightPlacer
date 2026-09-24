#pragma once

class LightsToUpdate
{
public:
	static RE::FormID GetCellID(const RE::TESObjectCELL* a_cell) { return a_cell ? a_cell->GetFormID() : 0; }

	void Add(RE::RefHandle a_handle, RE::FormID a_cellFormID, bool a_update, bool a_updateEmittance);
	void Move(RE::RefHandle a_handle, RE::FormID a_cellFormID);
	void Remove(RE::RefHandle a_handle);
	void Remove(RE::RefHandle a_handle, RE::FormID a_cellFormID);

	std::vector<RE::RefHandle> GetRefs(RE::FormID a_cellFormID) const;
	std::vector<RE::RefHandle> GetEmittanceRefs(RE::FormID a_cellFormID) const;

private:
	struct QueuedRef
	{
		RE::FormID cellFormID{ 0 };
		bool       update{ false };
		bool       updateEmittance{ false };
		bool       canBeMoved{ false };
	};

	struct CellRefs
	{
		FlatSet<RE::RefHandle> updating;
		FlatSet<RE::RefHandle> emittance;
	};

	void InsertIntoCell(RE::RefHandle a_handle, const QueuedRef& a_queued);
	void EraseFromCell(RE::RefHandle a_handle, RE::FormID a_cellFormID);

	// members
	ConcurrentMap<RE::RefHandle, QueuedRef> refs;
	ConcurrentMap<RE::FormID, CellRefs>     cells;
};
