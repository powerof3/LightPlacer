#pragma once

#include "Manager.h"

namespace Hooks::Attach
{
	namespace ObjectReference
	{
		template <class T>
		struct Load3D
		{
			static RE::NiAVObject* thunk(T* a_this, bool a_backgroundLoading)
			{
				auto node = func(a_this, a_backgroundLoading);
				if (node) {
					if (auto baseObject = a_this->GetObjectReference()) {
						if (!a_backgroundLoading) {
							LightManager::GetSingleton()->AddLights(a_this, baseObject, node);
						} else {
							// shouldusetaskqueue returns false even though it's on a different thread
							const auto handle = a_this->CreateRefHandle();
							SKSE::GetTaskInterface()->AddTask([handle, baseObject, nodePtr = RE::NiPointer<RE::NiAVObject>(node)]() {
								const auto ref = handle.get();
								if (!ref) {
									return;
								}
								RE::NiAVObject* rootNode = nodePtr.get();
								if (auto currentRoot = ref->Get3D(); currentRoot && currentRoot != rootNode) { // only some objects have attached 3D at this stage
									rootNode = currentRoot;
								}
								LightManager::GetSingleton()->AddLights(ref.get(), baseObject, rootNode);
							});
						}
					}
				}
				return node;
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t                   idx{ 0x6A };

			static void Install()
			{
				stl::write_vfunc<T, Load3D>();
				REX::INFO("Hooked {}::Load3D"sv, typeid(T).name());
			}
		};
	}

	namespace ReferenceEffect
	{
		template <class T>
		struct Init
		{
			static bool thunk(T* a_this)
			{
				auto result = func(a_this);

				if (result) {
					RE::FormID effectID = 0;
					if constexpr (std::is_same_v<RE::ShaderReferenceEffect, T>) {
						if (a_this->effectData) {
							effectID = a_this->effectData->GetFormID();
						}
					} else if constexpr (std::is_same_v<RE::ModelReferenceEffect, T>) {
						if (a_this->artObject) {
							effectID = a_this->artObject->GetFormID();
						}
					}
					LightManager::GetSingleton()->AddReferenceEffectLights(a_this, effectID);
				}

				return result;
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t                   idx{ 0x36 };

			static void Install()
			{
				stl::write_vfunc<T, Init>();
				REX::INFO("Hooked {}::Init"sv, typeid(T).name());
			}
		};
	}

	void Install();
}
