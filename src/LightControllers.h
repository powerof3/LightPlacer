#pragma once

enum class INTERPOLATION : std::uint8_t
{
	kStep,
	kLinear,
	kCubic
};

namespace LIGH
{
	struct LightDefinition;
}

// all-in-one controller
struct LightAnimData
{
	bool GetValidColor() const;
	bool GetValidFade() const;
	bool GetValidRadius() const;
	bool GetValidTranslation() const;
	bool GetValidRotation() const;

	// members
	RE::NiColor  color{ RE::COLOR_MAX };
	float        radius{ RE::NI_INFINITY };
	float        fade{ RE::NI_INFINITY };
	RE::NiPoint3 translation{ RE::POINT_MAX };
	RE::NiPoint3 rotation{ RE::POINT_MAX };

private:
	template <typename T>
	static bool IsValid(const T& value)
	{
		if constexpr (std::is_same_v<float, T>) {
			return value != RE::NI_INFINITY;
		} else if constexpr (std::is_same_v<RE::NiColor, T>) {
			return value.red != RE::NI_INFINITY;
		} else if constexpr (std::is_same_v<RE::NiPoint3, T>) {
			return value.x != RE::NI_INFINITY;
		} else {
			return false;
		}
	}
};

template <class T, std::uint32_t index = 0>  // specialize between same types
struct Keyframe
{
	// members
	float time{};
	T     value{};
	T     forward{};
	T     backward{};
};

template <class T, std::uint32_t index = 0>
class KeyframeSequence
{
public:
	void     clear() { keys = {}; }
	bool     empty() const { return keys.empty(); }
	explicit operator bool() const { return !empty(); }

	float GetDuration() const { return keys.empty() ? 0.0f : keys.back().time - keys.front().time; }

	T GetValue(const float a_time, std::uint32_t& a_lastIndex) const
	{
		for (auto i = a_lastIndex; i < keys.size() - 1; ++i) {
			const auto& currKeyframe = keys[i];
			const auto& nextKeyframe = keys[i + 1];

			if (a_time >= currKeyframe.time && a_time <= nextKeyframe.time) {
				a_lastIndex = i;
				return Interpolate(a_time, currKeyframe, nextKeyframe);
			}
		}

		a_lastIndex = 0;
		return keys.front().value;
	}

	INTERPOLATION                   interpolation{ INTERPOLATION::kLinear };
	std::vector<Keyframe<T, index>> keys{};

private:
	T Interpolate(float a_time, const Keyframe<T, index>& a_start, const Keyframe<T, index>& a_end) const
	{
		float dt = a_end.time - a_start.time;

		if (dt <= 0.0f) {
			return a_start.value;
		}

		float t = (a_time - a_start.time) / dt;

		switch (interpolation) {
		case INTERPOLATION::kStep:
			return a_start.value;
		case INTERPOLATION::kLinear:
			return (1 - t) * a_start.value + t * a_end.value;
		case INTERPOLATION::kCubic:
			{
				// Hermite interpolation formula
				float t2 = t * t;
				float t3 = t2 * t;
				float h1 = 2 * t3 - 3 * t2 + 1;
				float h2 = -2 * t3 + 3 * t2;
				float h3 = t3 - 2 * t2 + t;
				float h4 = t3 - t2;

				return h1 * a_start.value +
				       h2 * a_end.value +
				       h3 * a_start.forward * dt +
				       h4 * a_end.backward * dt;
			}
		default:
			return T();
		}
	}
};

template <class T, std::uint32_t index = 0>
class LightController
{
public:
	LightController() = default;
	LightController(const KeyframeSequence<T, index>* a_sequence, bool a_randomAnimStart) :
		sequence(a_sequence),
		duration(a_sequence->GetDuration())
	{
		if (a_randomAnimStart) {
			currentTime = clib_util::RNG().generate(0.0f, duration);
		}
	}

	T GetValue(const float a_delta)
	{
		currentTime = std::fmod(currentTime + a_delta, duration);
		return sequence->GetValue(currentTime, lastIndex);
	}

	bool     empty() const { return !sequence || sequence->empty(); }
	explicit operator bool() const { return !empty(); }

private:
	// members
	const KeyframeSequence<T, index>* sequence{ nullptr };
	std::uint32_t                     lastIndex{ 0 };
	float                             currentTime{ 0.0f };
	float                             duration{ 0.0f };
};

template <>
struct glz::meta<INTERPOLATION>
{
	using enum INTERPOLATION;
	static constexpr auto value = enumerate("Step", kStep, "Linear", kLinear, "Cubic", kCubic);
};

template <>
struct glz::meta<Keyframe<LightAnimData>>
{
	using T = Keyframe<LightAnimData>;
	static constexpr auto value = object(
		"time", &T::time,
		"data", &T::value,
		"forward", &T::forward,
		"backward", &T::backward);
};

// translation
template <>
struct glz::meta<Keyframe<RE::NiPoint3>>
{
	using T = Keyframe<RE::NiPoint3>;
	static constexpr auto value = object(
		"time", &T::time,
		"translation", &T::value,
		"forward", &T::forward,
		"backward", &T::backward);
};

// rotation
template <>
struct glz::meta<Keyframe<RE::NiPoint3, 1>>
{
	using T = Keyframe<RE::NiPoint3, 1>;
	static constexpr auto value = object(
		"time", &T::time,
		"rotation", &T::value,
		"forward", &T::forward,
		"backward", &T::backward);
};

// color
template <>
struct glz::meta<Keyframe<RE::NiColor>>
{
	using T = Keyframe<RE::NiColor>;

	static constexpr auto value = object(
		"time", &T::time,
		"color", &T::value,
		"forward", &T::forward,
		"backward", &T::backward);
};

// generic
template <>
struct glz::meta<Keyframe<float>>
{
	using T = Keyframe<float>;

	static constexpr auto value = object(
		"time", &T::time,
		"value", &T::value,
		"forward", &T::forward,
		"backward", &T::backward);
};

using PositionKeyframe = Keyframe<RE::NiPoint3, 0>;
using RotationKeyframe = Keyframe<RE::NiPoint3, 1>;
using ColorKeyframe = Keyframe<RE::NiColor>;
using FloatKeyframe = Keyframe<float>;

using AIOKeyframeSequence = KeyframeSequence<LightAnimData>;
using PositionKeyframeSequence = KeyframeSequence<RE::NiPoint3, 0>;
using RotationKeyframeSequence = KeyframeSequence<RE::NiPoint3, 1>;
using ColorKeyframeSequence = KeyframeSequence<RE::NiColor>;
using FloatKeyframeSequence = KeyframeSequence<float>;

using PositionController = LightController<RE::NiPoint3, 0>;
using RotationController = LightController<RE::NiPoint3, 1>;
using ColorController = LightController<RE::NiColor>;
using FloatController = LightController<float>;

struct LightControllers
{
	LightControllers() = default;
	LightControllers(const LIGH::LightDefinition& a_lightDef);

	void UpdateAnimation(const RE::NiPointer<RE::NiPointLight>& a_light, float a_delta, float a_scalingFactor);

	// members
	ColorController    colorController{};
	FloatController    radiusController{};
	FloatController    fadeController{};
	PositionController positionController{};
	RotationController rotationController{};
};
