#pragma once

#include "Manager.h"

namespace Hooks::Update
{
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
				REX::INFO("Hooked {}::UpdatePosition"sv, typeid(T).name());
			}
		};
	}

	namespace TESObjectREFR
	{
		// interior/exterior cell transitions

		template <class T>
		struct SetParentCell
		{
			static void thunk(T* a_this, RE::TESObjectCELL* a_cell)
			{
				auto oldCell = a_this->GetParentCell();

				func(a_this, a_cell);

				LightManager::GetSingleton()->UpdateParentCell(a_this, oldCell, a_this->GetParentCell());
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t                   idx{ 0x98 };

			static void Install()
			{
				stl::write_vfunc<T, SetParentCell>();
				REX::INFO("Hooked {}::SetParentCell"sv, typeid(T).name());
			}
		};
	}

	void Install();
}
