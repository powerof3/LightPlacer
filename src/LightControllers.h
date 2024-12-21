#pragma once

namespace Animation
{
	enum class INTERPOLATION : std::uint8_t
	{
		kStep,
		kLinear,
		kCubic
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
		bool  empty() const { return keys.empty(); }
		float GetDuration() const { return keys.back().time - keys.front().time; }

		T GetValue(const float a_time)
		{
			for (std::size_t i = lastIndex; i < keys.size() - 1; ++i) {
				const auto& currKeyframe = keys[i];
				const auto& nextKeyframe = keys[i + 1];

				if (a_time >= currKeyframe.time && a_time <= nextKeyframe.time) {
					lastIndex = i;
					return Interpolate(a_time, currKeyframe, nextKeyframe);
				}
			}

			lastIndex = 0;
			return keys.front().value;
		}

		// members
		INTERPOLATION                   interpolation{ INTERPOLATION::kLinear };
		std::vector<Keyframe<T, index>> keys{};
		std::size_t                     lastIndex{ 0 };

	private:
		T Interpolate(float a_time, const Keyframe<T, index>& a_start, const Keyframe<T, index>& a_end)
		{
			float t = (a_time - a_start.time) / (a_end.time - a_start.time);

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

					float h1 = (2 * t3) - (3 * t2) + 1;
					float h2 = 1 - h1;
					float h3 = t3 - (2 * t2) + t;
					float h4 = t3 - t2;

					return h1 * a_start.value +
					       h2 * a_end.value +
					       h3 * a_start.forward +
					       h4 * a_end.backward;
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
		explicit LightController(const KeyframeSequence<T, index>& a_sequence, bool a_randomAnimStart) :
			sequence(a_sequence),
			cycleDuration(a_sequence.GetDuration())
		{
			if (a_randomAnimStart) {
				currentTime = clib_util::RNG().generate(0.0f, cycleDuration);
			}
		}

		T GetValue(const float a_time)
		{
			currentTime = std::fmod(currentTime + a_time, cycleDuration);
			return sequence.GetValue(currentTime);
		}

	private:
		// members
		KeyframeSequence<T, index> sequence;
		float                      cycleDuration{ -1.0f };
		float                      currentTime{ 0.0f };
	};
}

template <>
struct glz::meta<Animation::INTERPOLATION>
{
	using enum Animation::INTERPOLATION;
	static constexpr auto value = enumerate("Step", kStep, "Linear", kLinear, "Cubic", kCubic);
};

using PosKeyframe = Animation::Keyframe<RE::NiPoint3, 0>;
template <>
struct glz::meta<PosKeyframe>
{
	using T = PosKeyframe;

	static constexpr auto value = object(
		"time", &T::time,
		"translation", &T::value,
		"forward", &T::forward,
		"backward", &T::backward);
};

using RotKeyframe = Animation::Keyframe<RE::NiPoint3, 1>;
template <>
struct glz::meta<RotKeyframe>
{
	using T = RotKeyframe;

	static constexpr auto value = object(
		"time", &T::time,
		"rotation", &T::value,
		"forward", &T::forward,
		"backward", &T::backward);
};

using ColorKeyframe = Animation::Keyframe<RE::NiColor>;
template <>
struct glz::meta<ColorKeyframe>
{
	using T = ColorKeyframe;

	static constexpr auto value = object(
		"time", &T::time,
		"color", &T::value,
		"forward", &T::forward,
		"backward", &T::backward);
};

using FloatKeyframe = Animation::Keyframe<float>;
template <>
struct glz::meta<FloatKeyframe>
{
	using T = FloatKeyframe;

	static constexpr auto value = object(
		"time", &T::time,
		"value", &T::value,
		"forward", &T::forward,
		"backward", &T::backward);
};

using PosKeyframeSequence = Animation::KeyframeSequence<RE::NiPoint3, 0>;
using RotKeyframeSequence = Animation::KeyframeSequence<RE::NiPoint3, 1>;
using ColorKeyframeSequence = Animation::KeyframeSequence<RE::NiColor>;
using FloatKeyframeSequence = Animation::KeyframeSequence<float>;

using PosController = Animation::LightController<RE::NiPoint3, 0>;
using RotController = Animation::LightController<RE::NiPoint3, 1>;
using ColorController = Animation::LightController<RE::NiColor>;
using FloatController = Animation::LightController<float>;
