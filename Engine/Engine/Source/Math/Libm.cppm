export module gse.math:libm;

import std;

export namespace gse::libm {
	auto sinf(
		float x
	) -> float;

	auto cosf(
		float x
	) -> float;

	auto tanf(
		float x
	) -> float;

	auto asinf(
		float x
	) -> float;

	auto acosf(
		float x
	) -> float;

	auto atanf(
		float x
	) -> float;

	auto atan2f(
		float y,
		float x
	) -> float;

	auto expf(
		float x
	) -> float;

	auto logf(
		float x
	) -> float;

	auto powf(
		float x,
		float y
	) -> float;

	template <typename T>
	constexpr auto sin(
		T x
	);

	template <typename T>
	constexpr auto cos(
		T x
	);

	template <typename T>
	constexpr auto tan(
		T x
	);

	template <typename T>
	constexpr auto asin(
		T x
	);

	template <typename T>
	constexpr auto acos(
		T x
	);

	template <typename T>
	constexpr auto atan(
		T x
	);

	template <typename T>
	constexpr auto atan2(
		T y,
		T x
	);

	template <typename T>
	constexpr auto exp(
		T x
	);

	template <typename T>
	constexpr auto log(
		T x
	);

	template <typename T>
	constexpr auto pow(
		T x,
		T y
	);
}

template <typename T>
constexpr auto gse::libm::sin(const T x) {
	if constexpr (std::same_as<T, float>) {
		if !consteval {
			return sinf(x);
		}
	}
	return std::sin(x);
}

template <typename T>
constexpr auto gse::libm::cos(const T x) {
	if constexpr (std::same_as<T, float>) {
		if !consteval {
			return cosf(x);
		}
	}
	return std::cos(x);
}

template <typename T>
constexpr auto gse::libm::tan(const T x) {
	if constexpr (std::same_as<T, float>) {
		if !consteval {
			return tanf(x);
		}
	}
	return std::tan(x);
}

template <typename T>
constexpr auto gse::libm::asin(const T x) {
	if constexpr (std::same_as<T, float>) {
		if !consteval {
			return asinf(x);
		}
	}
	return std::asin(x);
}

template <typename T>
constexpr auto gse::libm::acos(const T x) {
	if constexpr (std::same_as<T, float>) {
		if !consteval {
			return acosf(x);
		}
	}
	return std::acos(x);
}

template <typename T>
constexpr auto gse::libm::atan(const T x) {
	if constexpr (std::same_as<T, float>) {
		if !consteval {
			return atanf(x);
		}
	}
	return std::atan(x);
}

template <typename T>
constexpr auto gse::libm::atan2(const T y, const T x) {
	if constexpr (std::same_as<T, float>) {
		if !consteval {
			return atan2f(y, x);
		}
	}
	return std::atan2(y, x);
}

template <typename T>
constexpr auto gse::libm::exp(const T x) {
	if constexpr (std::same_as<T, float>) {
		if !consteval {
			return expf(x);
		}
	}
	return std::exp(x);
}

template <typename T>
constexpr auto gse::libm::log(const T x) {
	if constexpr (std::same_as<T, float>) {
		if !consteval {
			return logf(x);
		}
	}
	return std::log(x);
}

template <typename T>
constexpr auto gse::libm::pow(const T x, const T y) {
	if constexpr (std::same_as<T, float>) {
		if !consteval {
			return powf(x, y);
		}
	}
	return std::pow(x, y);
}
