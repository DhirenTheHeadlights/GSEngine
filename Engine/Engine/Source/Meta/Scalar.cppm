export module gse.meta:scalar;

import std;

export namespace gse {
	template <typename T>
	struct scalar;

	template <typename T>
	auto scalar_of(const T& v) -> typename scalar<T>::type;

	template <typename T>
	auto scalar_to(typename scalar<T>::type v) -> T;
}

export template <typename T>
requires std::is_arithmetic_v<T>
struct gse::scalar<T> {
	using type = T;

	static auto get(const T& v) -> T;

	static auto from(T v) -> T;
};

template <typename T>
requires std::is_arithmetic_v<T>
auto gse::scalar<T>::get(const T& v) -> T {
	return v;
}

template <typename T>
requires std::is_arithmetic_v<T>
auto gse::scalar<T>::from(const T v) -> T {
	return v;
}

template <typename T>
auto gse::scalar_of(const T& v) -> typename scalar<T>::type {
	return scalar<T>::get(v);
}

template <typename T>
auto gse::scalar_to(typename scalar<T>::type v) -> T {
	return scalar<T>::from(v);
}
