export module gse.physics.math.unitless_vec;

import std;

import gse.physics.math.base_vec;

export namespace gse::unitless {
	template <typename T, int N>
	struct vec_t : internal::vec_t<vec_t<T, N>, T, N> {
		using internal::vec_t<vec_t, T, N>::vec_t;

		template <typename U>
		constexpr vec_t(const vec_t<U, N>& other) {
			for (int i = 0; i < N; ++i) {
				this->storage.data[i] = static_cast<T>(other[i]);
			}
		}
	};

	template <typename T> using vec2_t = vec_t<T, 2>;
	template <typename T> using vec3_t = vec_t<T, 3>;
	template <typename T> using vec4_t = vec_t<T, 4>;

	using vec2i = vec2_t<int>;
	using vec3i = vec3_t<int>;
	using vec4i = vec4_t<int>;

	using vec2 = vec2_t<float>;
	using vec3 = vec3_t<float>;
	using vec4 = vec4_t<float>;

	using vec2d = vec2_t<double>;
	using vec3d = vec3_t<double>;
	using vec4d = vec4_t<double>;

	template <typename T, int N> constexpr auto operator+(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>;
	template <typename T, int N> constexpr auto operator-(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>;
	template <typename T, int N> constexpr auto operator*(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>;
	template <typename T, int N> constexpr auto operator/(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>;

	template <typename T, int N> constexpr auto operator+=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>&;
	template <typename T, int N> constexpr auto operator-=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>&;
	template <typename T, int N> constexpr auto operator*=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>&;
	template <typename T, int N> constexpr auto operator/=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>&;

	template <typename T, int N> constexpr auto operator*(const vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N>;
	template <typename T, int N> constexpr auto operator*(const T& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>;
	template <typename T, int N> constexpr auto operator/(const vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N>;
	template <typename T, int N> constexpr auto operator/(const T& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>;

	template <typename T, int N> constexpr auto operator*=(vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N>&;
	template <typename T, int N> constexpr auto operator/=(vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N>&;

	template <typename Derived1, typename Derived2, typename T1, typename T2, int N> requires (!std::is_same_v<Derived1, Derived2> || !std::is_same_v<T1, T2>) constexpr auto operator+(const internal::vec_t<Derived1, T1, N>& lhs, const internal::vec_t<Derived2, T2, N>& rhs) -> vec_t<std::common_type_t<T1, T2>, N>;
	template <typename Derived1, typename Derived2, typename T1, typename T2, int N> requires (!std::is_same_v<Derived1, Derived2> || !std::is_same_v<T1, T2>) constexpr auto operator-(const internal::vec_t<Derived1, T1, N>& lhs, const internal::vec_t<Derived2, T2, N>& rhs) -> vec_t<std::common_type_t<T1, T2>, N>;
	template <typename Derived1, typename Derived2, typename T1, typename T2, int N> requires (!std::is_same_v<Derived1, Derived2> || !std::is_same_v<T1, T2>) constexpr auto operator*(const internal::vec_t<Derived1, T1, N>& lhs, const internal::vec_t<Derived2, T2, N>& rhs) -> vec_t<std::common_type_t<T1, T2>, N>;
	template <typename Derived1, typename Derived2, typename T1, typename T2, int N> requires (!std::is_same_v<Derived1, Derived2> || !std::is_same_v<T1, T2>) constexpr auto operator/(const internal::vec_t<Derived1, T1, N>& lhs, const internal::vec_t<Derived2, T2, N>& rhs) -> vec_t<std::common_type_t<T1, T2>, N>;

	template <typename Derived1, typename Derived2, typename T1, typename T2, int N> requires (!std::is_same_v<Derived1, Derived2> || !std::is_same_v<T1, T2>) constexpr auto operator+=(internal::vec_t<Derived1, T1, N>& lhs, const internal::vec_t<Derived2, T2, N>& rhs) -> internal::vec_t<Derived1, T1, N>&;
	template <typename Derived1, typename Derived2, typename T1, typename T2, int N> requires (!std::is_same_v<Derived1, Derived2> || !std::is_same_v<T1, T2>) constexpr auto operator-=(internal::vec_t<Derived1, T1, N>& lhs, const internal::vec_t<Derived2, T2, N>& rhs) -> internal::vec_t<Derived1, T1, N>&;
	template <typename Derived1, typename Derived2, typename T1, typename T2, int N> requires (!std::is_same_v<Derived1, Derived2> || !std::is_same_v<T1, T2>) constexpr auto operator*=(internal::vec_t<Derived1, T1, N>& lhs, const internal::vec_t<Derived2, T2, N>& rhs) -> internal::vec_t<Derived1, T1, N>&;
	template <typename Derived1, typename Derived2, typename T1, typename T2, int N> requires (!std::is_same_v<Derived1, Derived2> || !std::is_same_v<T1, T2>) constexpr auto operator/=(internal::vec_t<Derived1, T1, N>& lhs, const internal::vec_t<Derived2, T2, N>& rhs) -> internal::vec_t<Derived1, T1, N>&;

	template <typename Derived, typename T, typename U, int N> requires std::is_arithmetic_v<U> && !std::is_same_v<T, U> constexpr auto operator*(const internal::vec_t<Derived, T, N>& lhs, const U& rhs) -> Derived;
	template <typename Derived, typename T, typename U, int N> requires std::is_arithmetic_v<U> && !std::is_same_v<T, U> constexpr auto operator*(const U& lhs, const internal::vec_t<Derived, T, N>& rhs) -> Derived;
	template <typename Derived, typename T, typename U, int N> requires std::is_arithmetic_v<U> && !std::is_same_v<T, U> constexpr auto operator/(const internal::vec_t<Derived, T, N>& lhs, const U& rhs) -> Derived;
	template <typename Derived, typename T, typename U, int N> requires std::is_arithmetic_v<U> && !std::is_same_v<T, U> constexpr auto operator/(const U& lhs, const internal::vec_t<Derived, T, N>& rhs) -> Derived;

	template <typename Derived, typename T, typename U, int N> requires std::is_arithmetic_v<U> && !std::is_same_v<T, U> constexpr auto operator*=(internal::vec_t<Derived, T, N>& lhs, const U& rhs) -> internal::vec_t<Derived, T, N>&;
	template <typename Derived, typename T, typename U, int N> requires std::is_arithmetic_v<U> && !std::is_same_v<T, U> constexpr auto operator/=(internal::vec_t<Derived, T, N>& lhs, const U& rhs) -> internal::vec_t<Derived, T, N>&;

	template <typename T, int N> requires std::is_integral_v<T> constexpr auto operator==(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool;
	template <typename T, int N> requires std::is_integral_v<T> constexpr auto operator!=(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool;
	template <typename T, int N> requires !std::is_integral_v<T> constexpr auto operator==(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool;
	template <typename T, int N> requires !std::is_integral_v<T> constexpr auto operator!=(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool;

	template <typename T, int N> constexpr auto operator-(const vec_t<T, N>& value)->vec_t<T, N>;
}

template <typename T, int N>
constexpr auto gse::unitless::operator+(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] + rhs[i];
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::unitless::operator-(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] - rhs[i];
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::unitless::operator*(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] * rhs[i];
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::unitless::operator/(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] / rhs[i];
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::unitless::operator+=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>& {
	for (int i = 0; i < N; ++i) {
		lhs[i] += rhs[i];
	}
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator-=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>& {
	for (int i = 0; i < N; ++i) {
		lhs[i] -= rhs[i];
	}
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator*=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>& {
	for (int i = 0; i < N; ++i) {
		lhs[i] *= rhs[i];
	}
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator/=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>& {
	for (int i = 0; i < N; ++i) {
		lhs[i] /= rhs[i];
	}
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator*(const vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] * rhs;
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::unitless::operator*(const T& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs * rhs[i];
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::unitless::operator/(const vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] / rhs;
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::unitless::operator/(const T& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs / rhs[i];
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::unitless::operator*=(vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N>& {
	for (int i = 0; i < N; ++i) {
		lhs[i] *= rhs;
	}
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator/=(vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N>& {
	for (int i = 0; i < N; ++i) {
		lhs[i] /= rhs;
	}
	return lhs;
}

template <typename Derived1, typename Derived2, typename T1, typename T2, int N> requires (!std::is_same_v<Derived1, Derived2> || !std::is_same_v<T1, T2>)
constexpr auto gse::unitless::operator+(const internal::vec_t<Derived1, T1, N>& lhs, const internal::vec_t<Derived2, T2, N>& rhs) -> vec_t<std::common_type_t<T1, T2>, N> {
	vec_t<std::common_type_t<T1, T2>, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] + rhs[i];
	}
	return result;
}

template <typename Derived1, typename Derived2, typename T1, typename T2, int N> requires (!std::is_same_v<Derived1, Derived2> || !std::is_same_v<T1, T2>)
constexpr auto gse::unitless::operator-(const internal::vec_t<Derived1, T1, N>& lhs, const internal::vec_t<Derived2, T2, N>& rhs) -> vec_t<std::common_type_t<T1, T2>, N> {
	vec_t<std::common_type_t<T1, T2>, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] - rhs[i];
	}
	return result;
}

template <typename Derived1, typename Derived2, typename T1, typename T2, int N> requires (!std::is_same_v<Derived1, Derived2> || !std::is_same_v<T1, T2>)
constexpr auto gse::unitless::operator*(const internal::vec_t<Derived1, T1, N>& lhs, const internal::vec_t<Derived2, T2, N>& rhs) -> vec_t<std::common_type_t<T1, T2>, N> {
	vec_t<std::common_type_t<T1, T2>, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] * rhs[i];
	}
	return result;
}

template <typename Derived1, typename Derived2, typename T1, typename T2, int N> requires (!std::is_same_v<Derived1, Derived2> || !std::is_same_v<T1, T2>)
constexpr auto gse::unitless::operator/(const internal::vec_t<Derived1, T1, N>& lhs, const internal::vec_t<Derived2, T2, N>& rhs) -> vec_t<std::common_type_t<T1, T2>, N> {
	vec_t<std::common_type_t<T1, T2>, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] / rhs[i];
	}
	return result;
}

template <typename Derived1, typename Derived2, typename T1, typename T2, int N> requires (!std::is_same_v<Derived1, Derived2> || !std::is_same_v<T1, T2>)
constexpr auto gse::unitless::operator+=(internal::vec_t<Derived1, T1, N>& lhs, const internal::vec_t<Derived2, T2, N>& rhs) -> internal::vec_t<Derived1, T1, N>& {
	for (int i = 0; i < N; ++i) {
		lhs[i] += rhs[i];
	}
	return lhs;
}

template <typename Derived1, typename Derived2, typename T1, typename T2, int N> requires (!std::is_same_v<Derived1, Derived2> || !std::is_same_v<T1, T2>)
constexpr auto gse::unitless::operator-=(internal::vec_t<Derived1, T1, N>& lhs, const internal::vec_t<Derived2, T2, N>& rhs) -> internal::vec_t<Derived1, T1, N>& {
	for (int i = 0; i < N; ++i) {
		lhs[i] -= rhs[i];
	}
	return lhs;
}

template <typename Derived1, typename Derived2, typename T1, typename T2, int N> requires (!std::is_same_v<Derived1, Derived2> || !std::is_same_v<T1, T2>)
constexpr auto gse::unitless::operator*=(internal::vec_t<Derived1, T1, N>& lhs, const internal::vec_t<Derived2, T2, N>& rhs) -> internal::vec_t<Derived1, T1, N>& {
	for (int i = 0; i < N; ++i) {
		lhs[i] *= rhs[i];
	}
	return lhs;
}

template <typename Derived1, typename Derived2, typename T1, typename T2, int N> requires (!std::is_same_v<Derived1, Derived2> || !std::is_same_v<T1, T2>)
constexpr auto gse::unitless::operator/=(internal::vec_t<Derived1, T1, N>& lhs, const internal::vec_t<Derived2, T2, N>& rhs) -> internal::vec_t<Derived1, T1, N>& {
	for (int i = 0; i < N; ++i) {
		lhs[i] /= rhs[i];
	}
	return lhs;
}

template <typename Derived, typename T, typename U, int N> requires std::is_arithmetic_v<U> && !std::is_same_v<T, U>
constexpr auto gse::unitless::operator*(const internal::vec_t<Derived, T, N>& lhs, const U& rhs) -> Derived {
	Derived result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] * rhs;
	}
	return result;
}

template <typename Derived, typename T, typename U, int N> requires std::is_arithmetic_v<U> && !std::is_same_v<T, U>
constexpr auto gse::unitless::operator*(const U& lhs, const internal::vec_t<Derived, T, N>& rhs) -> Derived {
	Derived result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs * rhs[i];
	}
	return result;
}

template <typename Derived, typename T, typename U, int N> requires std::is_arithmetic_v<U> && !std::is_same_v<T, U>
constexpr auto gse::unitless::operator/(const internal::vec_t<Derived, T, N>& lhs, const U& rhs) -> Derived {
	Derived result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] / rhs;
	}
	return result;
}

template <typename Derived, typename T, typename U, int N> requires std::is_arithmetic_v<U> && !std::is_same_v<T, U>
constexpr auto gse::unitless::operator/(const U& lhs, const internal::vec_t<Derived, T, N>& rhs) -> Derived {
	Derived result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs / rhs[i];
	}
	return result;
}

template <typename Derived, typename T, typename U, int N> requires std::is_arithmetic_v<U> && !std::is_same_v<T, U>
constexpr auto gse::unitless::operator*=(internal::vec_t<Derived, T, N>& lhs, const U& rhs) -> internal::vec_t<Derived, T, N>& {
	lhs = lhs * rhs;
	return lhs;
}

template <typename Derived, typename T, typename U, int N> requires std::is_arithmetic_v<U> && !std::is_same_v<T, U>
constexpr auto gse::unitless::operator/=(internal::vec_t<Derived, T, N>& lhs, const U& rhs) -> internal::vec_t<Derived, T, N>& {
	lhs = lhs / rhs;
	return lhs;
}

template <typename T, int N> requires std::is_integral_v<T>
constexpr auto gse::unitless::operator==(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool {
	for (int i = 0; i < N; ++i) {
		if (lhs[i] != rhs[i]) {
			return false;
		}
	}
	return true;
}

template <typename T, int N> requires std::is_integral_v<T>
constexpr auto gse::unitless::operator!=(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool {
	return !(lhs == rhs);
}

template <typename T, int N> requires !std::is_integral_v<T>
constexpr auto gse::unitless::operator==(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool {
	for (int i = 0; i < N; ++i) {
		if (std::abs(lhs[i] - rhs[i]) > std::numeric_limits<T>::epsilon()) {
			return false;
		}
	}
	return true;
}

template <typename T, int N> requires !std::is_integral_v<T>
constexpr auto gse::unitless::operator!=(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool {
	return !(lhs == rhs);
}

template <typename T, int N>
constexpr auto gse::unitless::operator-(const vec_t<T, N>& value) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = -value[i];
	}
	return result;
}