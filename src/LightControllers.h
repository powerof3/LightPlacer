#pragma once

namespace Animation
{
	enum class INTERPOLATION : std::uint8_t
	{
		kStep,
		kLinear,
		kCubic
	};

	// all-in-one controller
	struct LightData
	{
		LightData operator+(const LightData& a_rhs) const;
		LightData operator-(const LightData& a_rhs) const;
		LightData operator*(float a_scalar) const;

		friend LightData operator*(float a_scalar, const LightData& data);

		bool GetValidColor() const;
		bool GetValidFade() const;
		bool GetValidRadius() const;
		bool GetValidTranslation() const;
		bool GetValidRotation() const;

		void Update(const RE::NiPointer<RE::NiPointLight>& a_niLight, float a_scale) const;

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

		bool GetValidFade() const { return false; }
		bool GetValidTranslation() const { return false; }

	private:
		// members
		KeyframeSequence<T, index> sequence;
		float                      cycleDuration{ -1.0f };
		float                      currentTime{ 0.0f };
	};

	template <>
	inline bool LightController<LightData>::GetValidFade() const
	{
		return sequence.keys.front().value.GetValidFade();
	}
	template <>
	inline bool LightController<LightData>::GetValidTranslation() const
	{
		return sequence.keys.front().value.GetValidTranslation();
	}
}

template <>
struct glz::meta<Animation::INTERPOLATION>
{
	using enum Animation::INTERPOLATION;
	static constexpr auto value = enumerate("Step", kStep, "Linear", kLinear, "Cubic", kCubic);
};

template <>
struct glz::meta<Animation::Keyframe<Animation::LightData>>
{
	using T = Animation::Keyframe<Animation::LightData>;

	static constexpr auto value = object(
		"time", &T::time,
		"data", &T::value,
		"forward", &T::forward,
		"backward", &T::backward);
};

// translation
template <>
struct glz::meta<Animation::Keyframe<RE::NiPoint3>>
{
	using T = Animation::Keyframe<RE::NiPoint3>;

	static constexpr auto value = object(
		"time", &T::time,
		"translation", &T::value,
		"forward", &T::forward,
		"backward", &T::backward);
};

// rotation
template <>
struct glz::meta<Animation::Keyframe<RE::NiPoint3, 1>>
{
	using T = Animation::Keyframe<RE::NiPoint3, 1>;

	static constexpr auto value = object(
		"time", &T::time,
		"rotation", &T::value,
		"forward", &T::forward,
		"backward", &T::backward);
};

// color
template <>
struct glz::meta<Animation::Keyframe<RE::NiColor>>
{
	using T = Animation::Keyframe<RE::NiColor>;

	static constexpr auto value = object(
		"time", &T::time,
		"color", &T::value,
		"forward", &T::forward,
		"backward", &T::backward);
};

// generic
template <>
struct glz::meta<Animation::Keyframe<float>>
{
	using T = Animation::Keyframe<float>;

	static constexpr auto value = object(
		"time", &T::time,
		"value", &T::value,
		"forward", &T::forward,
		"backward", &T::backward);
};

using AIOKeyframeSequence = Animation::KeyframeSequence<Animation::LightData>;
using PositionKeyframeSequence = Animation::KeyframeSequence<RE::NiPoint3, 0>;
using RotationKeyframeSequence = Animation::KeyframeSequence<RE::NiPoint3, 1>;
using ColorKeyframeSequence = Animation::KeyframeSequence<RE::NiColor>;
using FloatKeyframeSequence = Animation::KeyframeSequence<float>;

using AIOController = Animation::LightController<Animation::LightData>;
using PositionController = Animation::LightController<RE::NiPoint3, 0>;
using RotationController = Animation::LightController<RE::NiPoint3, 1>;
using ColorController = Animation::LightController<RE::NiColor>;
using FloatController = Animation::LightController<float>;
