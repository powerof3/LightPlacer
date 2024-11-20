#pragma once

#define NOMINMAX

#include <shared_mutex>

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <ClibUtil/RNG.hpp>
#include <ClibUtil/distribution.hpp>
#include <ClibUtil/simpleINI.hpp>
#include <ClibUtil/singleton.hpp>
#include <MergeMapperPluginAPI.h>
#include <boost_unordered.hpp>
#include <glaze/glaze.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <srell.hpp>
#include <xbyak/xbyak.h>

#include <ClibUtil/editorID.hpp>

#define DLLEXPORT __declspec(dllexport)

namespace logger = SKSE::log;
namespace string = clib_util::string;
namespace ini = clib_util::ini;
namespace edid = clib_util::editorID;
namespace dist = clib_util::distribution;

using namespace std::literals;
using namespace string::literals;
using namespace clib_util::singleton;

// for visting variants
template <class... Ts>
struct overload : Ts...
{
	using Ts::operator()...;
};

// https://www.reddit.com/r/cpp/comments/p132c7/a_c_locking_wrapper/h8b8nml/
template <
	class T,
	class M = std::mutex,
	template <class...> class RL = std::unique_lock,
	template <class...> class WL = std::unique_lock>
struct mutex_guarded
{
	mutex_guarded() = default;
	explicit mutex_guarded(T in) :
		data(std::move(in))
	{}

	auto read(auto&& f) const
	{
		auto l = lock();
		return f(data);
	}
	auto write(auto&& f)
	{
		auto l = lock();
		return f(data);
	}

private:
	mutable M mutex;
	T         data;

	auto lock() { return WL<M>(mutex); }
	auto lock() const { return RL<M>(mutex); }
};

template <class T>
using shared_guarded = mutex_guarded<T, std::shared_mutex, std::shared_lock>;

template <class K, class D, class H = boost::hash<K>>
using Map = boost::unordered_flat_map<K, D, H>;

template <class K, class H = boost::hash<K>>
using Set = boost::unordered_flat_set<K, H>;

template <class K, class D, class H = boost::hash<K>>
using LockedMap = shared_guarded<Map<K, D, H>>;

template <class K, class H = boost::hash<K>>
using LockedSet = shared_guarded<Set<K, H>>;

namespace stl
{
	using namespace SKSE::stl;

	template <class F, size_t index, class T>
	void write_vfunc()
	{
		REL::Relocation<std::uintptr_t> vtbl{ F::VTABLE[index] };
		T::func = vtbl.write_vfunc(T::size, T::thunk);
	}

	template <class F, class T>
	void write_vfunc()
	{
		write_vfunc<F, 0, T>();
	}

	template <class T>
	void write_thunk_call(std::uintptr_t a_src)
	{
		auto& trampoline = SKSE::GetTrampoline();
		T::func = trampoline.write_call<5>(a_src, T::thunk);
	}

	template <class T, std::size_t BYTES>
	void hook_function_prologue(std::uintptr_t a_src)
	{
		struct Patch : Xbyak::CodeGenerator
		{
			Patch(std::uintptr_t a_originalFuncAddr, std::size_t a_originalByteLength)
			{
				// Hook returns here. Execute the restored bytes and jump back to the original function.
				for (size_t i = 0; i < a_originalByteLength; i++)
					db(*reinterpret_cast<uint8_t*>(a_originalFuncAddr + i));

				jmp(qword[rip]);
				dq(a_originalFuncAddr + a_originalByteLength);
			}
		};

		Patch p(a_src, BYTES);
		p.ready();

		auto& trampoline = SKSE::GetTrampoline();
		trampoline.write_branch<5>(a_src, T::thunk);

		auto alloc = trampoline.allocate(p.getSize());
		std::memcpy(alloc, p.getCode(), p.getSize());

		T::func = reinterpret_cast<std::uintptr_t>(alloc);
	}
}

#include "Common.h"
#include "Version.h"

#ifdef SKYRIM_AE
#	define OFFSET(se, ae) ae
#else
#	define OFFSET(se, ae) se
#endif
