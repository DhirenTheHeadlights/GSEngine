export module gse.physics.math.unitless_vec;

import std;

import gse.physics.math.units.quant;
import gse.physics.math.base_vec;

export namespace gse::unitless {
	template <internal::is_arithmetic T, int N>
	struct vec_t : internal::vec_t<vec_t<T, N>, T, N> {
		using internal::vec_t<vec_t, T, N>::vec_t;

		template <typename U>
		constexpr vec_t(const vec_t<U, N>& other) {
			for (int i = 0; i < N; ++i) {
				this->storage[i] = static_cast<T>(other[i]);
			}
		}

		template <typename U>
		constexpr vec_t(const vec::storage<U, N>& other) {
			for (int i = 0; i < N; ++i) {
				this->storage[i] = static_cast<T>(other[i]);
			}
		}

		constexpr operator vec::storage<T, N>() const {
			if constexpr (N == 3) {
				return vec::storage<T, 3>{ this->storage[0], this->storage[1], this->storage[2] };
			}
			else {
				return vec::storage<T, N>{};
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
}

export namespace gse::unitless {
	template <typename T, int N> constexpr auto operator+(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>;
	template <typename T, int N> constexpr auto operator-(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>;
	template <typename T, int N> constexpr auto operator*(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>;
	template <typename T, int N> constexpr auto operator/(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>;

	template <typename T, typename U, int N> constexpr auto operator+(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<std::common_type_t<T, U>, N>;
	template <typename T, typename U, int N> constexpr auto operator-(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<std::common_type_t<T, U>, N>;
	template <typename T, typename U, int N> constexpr auto operator*(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<std::common_type_t<T, U>, N>;
	template <typename T, typename U, int N> constexpr auto operator/(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<std::common_type_t<T, U>, N>;

	template <typename T, int N> constexpr auto operator+=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>&;
	template <typename T, int N> constexpr auto operator-=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>&;
	template <typename T, int N> constexpr auto operator*=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>&;
	template <typename T, int N> constexpr auto operator/=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>&;

	template <typename T, typename U, int N> constexpr auto operator+=(vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N>&;
	template <typename T, typename U, int N> constexpr auto operator-=(vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N>&;
	template <typename T, typename U, int N> constexpr auto operator*=(vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N>&;
	template <typename T, typename U, int N> constexpr auto operator/=(vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N>&;

	template <typename T, internal::is_arithmetic U, int N> constexpr auto operator*(const vec_t<T, N>& lhs, const U& rhs) -> vec_t<std::common_type_t<T, U>, N>;
	template <typename T, internal::is_arithmetic U, int N> constexpr auto operator*(const U& lhs, const vec_t<T, N>& rhs) -> vec_t<std::common_type_t<T, U>, N>;
	template <typename T, internal::is_arithmetic U, int N> constexpr auto operator/(const vec_t<T, N>& lhs, const U& rhs) -> vec_t<std::common_type_t<T, U>, N>;
	template <typename T, internal::is_arithmetic U, int N> constexpr auto operator/(const U& lhs, const vec_t<T, N>& rhs) -> vec_t<std::common_type_t<T, U>, N>;

	template <typename T, internal::is_arithmetic U, int N> constexpr auto operator*=(vec_t<T, N>& lhs, const U& rhs) -> vec_t<T, N>&;
	template <typename T, internal::is_arithmetic U, int N> constexpr auto operator/=(vec_t<T, N>& lhs, const U& rhs) -> vec_t<T, N>&;

	template <typename T, int N> constexpr auto operator*(const vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N>;
	template <typename T, int N> constexpr auto operator*(const T& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>;
	template <typename T, int N> constexpr auto operator/(const vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N>;
	template <typename T, int N> constexpr auto operator/(const T& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>;

	template <typename T, int N> constexpr auto operator*=(vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N>&;
	template <typename T, int N> constexpr auto operator/=(vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N>&;

	template <typename T, int N> constexpr auto operator+(const vec_t<T, N>& value) -> vec_t<T, N>;
	template <typename T, int N> constexpr auto operator-(const vec_t<T, N>& value) -> vec_t<T, N>;

	template <typename T, int N> constexpr auto operator==(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool;
	template <typename T, int N> constexpr auto operator!=(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool;

	template <typename T, int N> constexpr auto operator>(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool;
	template <typename T, int N> constexpr auto operator>=(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool;
	template <typename T, int N> constexpr auto operator<(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool;
	template <typename T, int N> constexpr auto operator<=(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool;
}

template <typename T, int N>
constexpr auto gse::unitless::operator+(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(lhs.storage + rhs.storage);
}

template <typename T, int N>
constexpr auto gse::unitless::operator-(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(lhs.storage - rhs.storage);
}

template <typename T, int N>
constexpr auto gse::unitless::operator*(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(lhs.storage * rhs.storage);
}

template <typename T, int N>
constexpr auto gse::unitless::operator/(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(lhs.storage / rhs.storage);
}

template <typename T, typename U, int N>
constexpr auto gse::unitless::operator+(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<std::common_type_t<T, U>, N> {
	return unitless::vec_t<std::common_type_t<T, U>, N>(lhs.storage + rhs.storage);
}

template <typename T, typename U, int N>
constexpr auto gse::unitless::operator-(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<std::common_type_t<T, U>, N> {
	return unitless::vec_t<std::common_type_t<T, U>, N>(lhs.storage - rhs.storage);
}

template <typename T, typename U, int N>
constexpr auto gse::unitless::operator*(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<std::common_type_t<T, U>, N> {
	return unitless::vec_t<std::common_type_t<T, U>, N>(lhs.storage * rhs.storage);
}

template <typename T, typename U, int N>
constexpr auto gse::unitless::operator/(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<std::common_type_t<T, U>, N> {
	return unitless::vec_t<std::common_type_t<T, U>, N>(lhs.storage / rhs.storage);
}

template <typename T, int N>
constexpr auto gse::unitless::operator+=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>& {
	lhs.storage += rhs.storage;
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator-=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>& {
	lhs.storage -= rhs.storage;
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator*=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>& {
	lhs.storage *= rhs.storage;
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator/=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>& {
	lhs.storage /= rhs.storage;
	return lhs;
}

template <typename T, typename U, int N>
constexpr auto gse::unitless::operator+=(vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N>& {
	lhs.storage += rhs.storage;
	return lhs;
}

template <typename T, typename U, int N>
constexpr auto gse::unitless::operator-=(vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N>& {
	lhs.storage -= rhs.storage;
	return lhs;
}

template <typename T, typename U, int N>
constexpr auto gse::unitless::operator*=(vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N>& {
	lhs.storage *= rhs.storage;
	return lhs;
}

template <typename T, typename U, int N>
constexpr auto gse::unitless::operator/=(vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N>& {
	lhs.storage /= rhs.storage;
	return lhs;
}

template <typename T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unitless::operator*(const vec_t<T, N>& lhs, const U& rhs) -> vec_t<std::common_type_t<T, U>, N> {
	return unitless::vec_t<std::common_type_t<T, U>, N>(lhs.storage * rhs);
}

template <typename T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unitless::operator*(const U& lhs, const vec_t<T, N>& rhs) -> vec_t<std::common_type_t<T, U>, N> {
	return unitless::vec_t<std::common_type_t<T, U>, N>(lhs * rhs.storage);
}

template <typename T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unitless::operator/(const vec_t<T, N>& lhs, const U& rhs) -> vec_t<std::common_type_t<T, U>, N> {
	return unitless::vec_t<std::common_type_t<T, U>, N>(lhs.storage / rhs);
}

template <typename T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unitless::operator/(const U& lhs, const vec_t<T, N>& rhs) -> vec_t<std::common_type_t<T, U>, N> {
	return unitless::vec_t<std::common_type_t<T, U>, N>(lhs / rhs.storage);
}

template <typename T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unitless::operator*=(vec_t<T, N>& lhs, const U& rhs) -> vec_t<T, N>& {
	lhs.storage *= rhs;
	return lhs;
}

template <typename T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unitless::operator/=(vec_t<T, N>& lhs, const U& rhs) -> vec_t<T, N>& {
	lhs.storage /= rhs;
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator*(const vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(lhs.storage * rhs);
}

template <typename T, int N>
constexpr auto gse::unitless::operator*(const T& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(lhs * rhs.storage);
}

template <typename T, int N>
constexpr auto gse::unitless::operator/(const vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(lhs.storage / rhs);
}

template <typename T, int N>
constexpr auto gse::unitless::operator/(const T& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(lhs / rhs.storage);
}

template <typename T, int N>
constexpr auto gse::unitless::operator*=(vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N>& {
	lhs.storage *= rhs;
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator/=(vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N>& {
	lhs.storage /= rhs;
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator+(const vec_t<T, N>& value) -> vec_t<T, N> {
	return value;
}

template <typename T, int N>
constexpr auto gse::unitless::operator-(const vec_t<T, N>& value) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(-value.storage);
}

template <typename T, int N>
constexpr auto gse::unitless::operator==(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool {
	return lhs.storage == rhs.storage;
}

template <typename T, int N>
constexpr auto gse::unitless::operator!=(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool {
	return lhs.storage != rhs.storage;
}

template <typename T, int N>
constexpr auto gse::unitless::operator>(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool {
	return lhs.storage > rhs.storage;
}

template <typename T, int N>
constexpr auto gse::unitless::operator>=(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool {
	return lhs.storage >= rhs.storage;
}

template <typename T, int N>
constexpr auto gse::unitless::operator<(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool {
	return lhs.storage < rhs.storage;
}

template <typename T, int N>
constexpr auto gse::unitless::operator<=(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> bool {
	return lhs.storage <= rhs.storage;
}
