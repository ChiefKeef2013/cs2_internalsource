#pragma once

namespace random {

	inline std::mt19937& rng()
	{
		static std::mt19937 generator{ std::random_device{}() };
		return generator;
	}

	inline float floating(float min, float max)
	{
		return std::uniform_real_distribution<float>{ min, max }(rng());
	}

	inline float normal_clamped(float mean, float standard_deviation, float min, float max)
	{
		const auto value = std::normal_distribution<float>{ mean, standard_deviation }(rng());
		return std::clamp(value, min, max);
	}

	inline float hold_duration(float mean = 0.085f, float standard_deviation = 0.012f)
	{
		return normal_clamped(mean, standard_deviation, 0.045f, 0.140f);
	}

}
