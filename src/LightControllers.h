#pragma once

template <class T>
struct Keyframe
{
	static T Lerp(const Keyframe<T>& a_start, const Keyframe<T>& a_end, const float a_t)
	{
		return a_start.value + a_t * (a_end.value - a_start.value);
	}

	// members
	float time{};
	T     value{};
};

template <class T>
class LightController
{
public:
	LightController() = default;
	explicit LightController(const std::vector<Keyframe<T>>& a_keyframes);

	T GetValue(float a_time);

private:
	// members
	std::vector<Keyframe<T>> keyframes{};
	float                    currentTime{ 0.0f };
	float                    cycleDuration{ -1.0f };
};

template <class T>
LightController<T>::LightController(const std::vector<Keyframe<T>>& a_keyframes) :
	keyframes(a_keyframes),
	cycleDuration(a_keyframes.back().time - a_keyframes.front().time)
{}

template <class T>
T LightController<T>::GetValue(float a_time)
{
	currentTime = std::fmod(currentTime + a_time, cycleDuration);

	for (std::size_t i = 0; i < keyframes.size() - 1; ++i) {
		const auto& currKeyframe = keyframes[i];
		const auto& nextKeyframe = keyframes[i + 1];

		if (currentTime >= currKeyframe.time && currentTime <= nextKeyframe.time) {
			float t = (currentTime - currKeyframe.time) / (nextKeyframe.time - currKeyframe.time);
			return Keyframe<T>::Lerp(currKeyframe, nextKeyframe, t);
		}
	}

	return keyframes.front().value;
}

using ColorKeyframe = Keyframe<RE::NiColor>;
using FloatKeyframe = Keyframe<float>;

template <>
struct glz::meta<ColorKeyframe>
{
	using T = ColorKeyframe;

	static constexpr auto value = object(
		"time", &T::time,
		"color", &T::value);
};

template <>
struct glz::meta<FloatKeyframe>
{
	using T = FloatKeyframe;

	static constexpr auto value = object(
		"time", &T::time,
		"value", &T::value);
};
