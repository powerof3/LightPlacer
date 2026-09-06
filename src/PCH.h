#pragma once

#define NOMINMAX

#include <shared_mutex>

#include "RE/Skyrim.h"
#include "REX/REX.h"
#include "SKSE/SKSE.h"

#include <ClibUtil/distribution.hpp>
#include <MergeMapperPluginAPI.h>
//#include <boost_unordered.hpp>
#include <boost/regex.hpp>
#include <boost/unordered/concurrent_node_map.hpp>
#include <boost/unordered/concurrent_node_set.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <frozen/bits/elsa_std.h>
#include <frozen/unordered_map.h>
#include <glaze/glaze.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <xbyak/xbyak.h>

#undef ERROR

namespace dist = clib_util::distribution;

using namespace std::literals;
using namespace REX::STR::literals;

namespace stl
{
	template <class F, size_t vtbl_idx, class T>
	void write_vfunc()
	{
		REL::Relocation<std::uintptr_t> vtbl{ F::VTABLE[vtbl_idx] };
		T::func = vtbl.write_vfunc(T::idx, T::thunk);
	}

	template <class F, class T>
	void write_vfunc()
	{
		write_vfunc<F, 0, T>();
	}

	template <class T, std::size_t size = 5>
	void write_thunk_call(std::uintptr_t a_src)
	{
		auto& trampoline = REL::GetTrampoline();
		if (size == 6) {
			T::func = *(uintptr_t*)trampoline.write_call<6>(a_src, T::thunk);
		} else {
			T::func = trampoline.write_call<size>(a_src, T::thunk);
		}
	}

	template <class T, std::size_t BYTES>
	void hook_function_prologue(std::uintptr_t a_src)
	{
		struct Patch : Xbyak::CodeGenerator
		{
			Patch(std::uintptr_t a_originalFuncAddr, std::size_t a_originalByteLength)
			{
				// Hook returns here. Execute the restored bytes and jump back to the original function.
				for (size_t i = 0; i < a_originalByteLength; ++i) {
					db(*reinterpret_cast<std::uint8_t*>(a_originalFuncAddr + i));
				}

				jmp(ptr[rip]);
				dq(a_originalFuncAddr + a_originalByteLength);
			}
		};

		Patch p(a_src, BYTES);
		p.ready();

		auto& trampoline = REL::GetTrampoline();
		trampoline.write_jmp<5>(a_src, T::thunk);

		auto alloc = trampoline.allocate(p.getSize());
		std::memcpy(alloc, p.getCode(), p.getSize());

		T::func = reinterpret_cast<std::uintptr_t>(alloc);
	}
}

namespace Runtime
{
	inline constexpr REL::Version SSE_1_7_99(1, 7, 99, 0);
	inline constexpr REL::Version MIN_ADDRESS_LIBRARY_V5 = SSE_1_7_99;

	[[nodiscard]] inline bool IsAtLeast1_7_99() noexcept
	{
		static bool result = REX::FModule::GetExecutingModule().GetFileVersion() >= Runtime::SSE_1_7_99;
		return result;
	}
}

#include "Common.h"
#include "RE.h"
#include "Version.h"

#ifdef SKYRIM_AE
#	define OFFSET(se, ae) ae
#	define OFFSET_3(se, ae, vr) ae
#elif SKYRIMVR
#	define OFFSET(se, ae) se
#	define OFFSET_3(se, ae, vr) vr
#else
#	define OFFSET(se, ae) se
#	define OFFSET_3(se, ae, vr) se
#endif
