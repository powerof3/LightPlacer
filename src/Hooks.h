#pragma once

#include "Manager.h"

namespace Hooks
{
	// adds light on 3d load
	template <class T, std::size_t index = 0>
	struct Clone3D
	{
		static RE::NiAVObject* thunk(T* a_this, RE::TESObjectREFR* a_ref)
		{
			auto node = func(a_this, a_ref);
			if constexpr (std::is_same_v<RE::BGSMovableStatic, T>) {
				LightManager::GetSingleton()->TryAttachLights(a_ref, a_ref ? a_ref->GetBaseObject() : nullptr, node);			
			} else {
				LightManager::GetSingleton()->TryAttachLights(a_ref, a_this, node);			
			}
			return node;
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline constexpr std::size_t            size{ 0x4A };

		static void Install()
		{
			stl::write_vfunc<T, index, Clone3D>();		
			logger::info("Hooked {}::Clone3D"sv, typeid(T).name());
		}
	};
	
	void Install();
}
