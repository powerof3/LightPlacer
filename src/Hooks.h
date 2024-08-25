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
			LightManager::GetSingleton()->TryAttachLights(a_ref, a_this->As<RE::TESBoundObject>(), node);
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
