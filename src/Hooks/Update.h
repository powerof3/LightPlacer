#pragma once

#include "Manager.h"

namespace Hooks::Update
{
	// remove lights
	template <std::size_t N>
	struct RemoveExternalEmittance
	{
		static void thunk(RE::TESObjectCELL* a_cell, const RE::ObjectRefHandle& a_handle)
		{
			func(a_cell, a_handle);

			if (a_cell && a_cell->loadedData) {
				LightManager::GetSingleton()->RemoveLightsFromUpdateQueue(a_cell, a_handle);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	namespace ReferenceEffect
	{
		template <class T>
		struct UpdatePosition
		{
			static void thunk(T* a_this)
			{
				func(a_this);

				LightManager::GetSingleton()->UpdateReferenceEffectLights(a_this);
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t                   idx{ 0x3B };

			static void Install()
			{
				stl::write_vfunc<T, UpdatePosition>();
				logger::info("Hooked {}::UpdatePosition"sv, typeid(T).name());
			}
		};
	}

	static void Install_RemoveExternalEmittance();

	void Install();
}
