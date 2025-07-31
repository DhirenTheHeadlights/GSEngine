export module gse.physics.math:unit_vec;

import std;

import :quant;
import :base_vec;
import :unitless_vec;

namespace gse::unit {
    template <typename T>
    concept is_quantity_or_unit = internal::is_quantity<T> || internal::is_unit<T>;

    template <typename T, typename U>
    concept has_same_tag = std::is_same_v<typename T::quantity_tag, typename U::quantity_tag>;

    template <typename Q, typename T>
    using convertible_type = std::conditional_t<
        internal::is_unit<Q>,
        typename Q::value_type,
        T
    >;

    template <typename Q, typename T>
    constexpr auto get_value(const T& value) -> Q {
        if constexpr (internal::is_quantity<T>) {
            return value.as_default_unit();
        }
        else {
            return value;
        }
    }
}

namespace gse::unit {
    template <internal::is_quantity Q, int N>
	struct vec_t : internal::vec_t<vec_t<Q, N>, Q, N> {
		using internal::vec_t<vec_t, Q, N>::vec_t;
        using value_type = typename Q::value_type;

        template <typename... Args>
            requires ((std::is_convertible_v<Args, typename Q::value_type> || internal::is_quantity<Args>) && ...)
        constexpr vec_t(Args... args) : internal::vec_t<vec_t, Q, N>(unit::get_value<value_type>(args)...) {}

		template <typename U>
		constexpr vec_t(const unitless::vec_t<U, N>& other) : internal::vec_t<vec_t, Q, N>() {
			for (size_t i = 0; i < N; ++i) {
				this->storage[i] = Q(static_cast<value_type>(other[i]));
			}
		}

		template <internal::is_arithmetic U> 
		constexpr vec_t(const vec::storage<U, N> other) : internal::vec_t<vec_t, Q, N>() {
			for (size_t i = 0; i < N; ++i) {
				this->storage[i] = Q(static_cast<value_type>(other[i]));
			}
		}

        template <internal::is_unit U> requires has_same_tag<Q, U>
        [[nodiscard]] constexpr auto as() const -> unitless::vec_t<value_type, N> {
			unitless::vec_t<value_type, N> result;
            for (int i = 0; i < N; ++i) {
                result[i] = this->storage[i].as_default_unit() * U::conversion_factor;
            }
			return result;
        }

		constexpr auto operator<=>(const vec_t& other) const = default;
    };
}

export template <typename T, int N, typename CharT>
struct std::formatter<gse::unit::vec_t<T, N>, CharT> {
	std::formatter<gse::vec::storage<T, N>, CharT> storage_formatter;

	constexpr auto parse(std::format_parse_context& ctx) {
		return storage_formatter.parse(ctx);
	}

	template <typename FormatContext>
	auto format(const gse::unit::vec_t<T, N>& v, FormatContext& ctx) const {
		return storage_formatter.format(v.storage, ctx);
	}
};

export namespace gse::unit {
	template <internal::is_quantity T, int N>								constexpr auto operator+(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>;
	template <internal::is_quantity T, int N>								constexpr auto operator-(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>;

	template <internal::is_quantity T, internal::is_arithmetic U, int N>	constexpr auto operator*(const vec_t<T, N>& lhs, const U& rhs) -> vec_t<T, N>;
	template <internal::is_arithmetic U, internal::is_quantity T, int N> 	constexpr auto operator*(const U& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>;
	template <internal::is_quantity T, internal::is_arithmetic U, int N> 	constexpr auto operator/(const vec_t<T, N>& lhs, const U& rhs) -> vec_t<T, N>;
	template <internal::is_arithmetic U, internal::is_quantity T, int N>	constexpr auto operator/(const U& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>;

	template <internal::is_quantity T, internal::is_quantity U, int N>		constexpr auto operator*(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs);
	template <internal::is_quantity T, internal::is_quantity U, int N>		constexpr auto operator/(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs);
	template <internal::is_quantity T, int N>								constexpr auto operator/(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> unitless::vec_t<typename T::value_type, N>;

	template <internal::is_quantity T, internal::is_quantity U, int N>		constexpr auto operator*(const vec_t<T, N>& lhs, const U& rhs);
	template <internal::is_quantity T, internal::is_quantity U, int N>		constexpr auto operator/(const vec_t<T, N>& lhs, const U& rhs);
	template <internal::is_quantity T, internal::is_quantity U, int N>		constexpr auto operator*(const U& lhs, const vec_t<T, N>& rhs);
	template <internal::is_quantity T, internal::is_quantity U, int N>		constexpr auto operator/(const U& lhs, const vec_t<T, N>& rhs);

	template <internal::is_quantity T, int N>								constexpr auto operator/(const vec_t<T, N>& lhs, const T& rhs) -> unitless::vec_t<typename T::value_type, N>;
	template <internal::is_quantity T, int N>								constexpr auto operator/(const T& lhs, const vec_t<T, N>& rhs) -> unitless::vec_t<typename T::value_type, N>;

	template <internal::is_quantity T, int N>								constexpr auto operator+=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>&;
	template <internal::is_quantity T, int N>								constexpr auto operator-=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>&;
	template <internal::is_quantity T, internal::is_arithmetic U, int N>	constexpr auto operator*=(vec_t<T, N>& lhs, const U& rhs) -> vec_t<T, N>&;
	template <internal::is_quantity T, internal::is_arithmetic U, int N>	constexpr auto operator/=(vec_t<T, N>& lhs, const U& rhs) -> vec_t<T, N>&;

    template <internal::is_quantity T, internal::is_arithmetic U, int N>	constexpr auto operator+(const vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> vec_t<T, N>;
    template <internal::is_arithmetic T, internal::is_quantity U, int N>	constexpr auto operator+(const unitless::vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N>;
    template <internal::is_quantity T, internal::is_arithmetic U, int N>	constexpr auto operator-(const vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> vec_t<T, N>;
    template <internal::is_arithmetic T, internal::is_quantity U, int N>	constexpr auto operator-(const unitless::vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N>;
    template <internal::is_quantity T, internal::is_arithmetic U, int N>	constexpr auto operator*(const vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> vec_t<T, N>;
    template <internal::is_arithmetic T, internal::is_quantity U, int N>	constexpr auto operator*(const unitless::vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N>;
    template <internal::is_quantity T, internal::is_arithmetic U, int N>	constexpr auto operator/(const vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> vec_t<T, N>;
    template <internal::is_arithmetic T, internal::is_quantity U, int N>	constexpr auto operator/(const unitless::vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N>;

    template <internal::is_arithmetic T, int N, internal::is_quantity U>	constexpr auto operator*(const vec_t<T, N>& lhs, const U& rhs) -> vec_t<U, N>;
    template <internal::is_arithmetic T, int N, internal::is_quantity U>	constexpr auto operator*(const U& lhs, const vec_t<T, N>& rhs) -> vec_t<U, N>;
    template <internal::is_arithmetic T, int N, internal::is_quantity U>	constexpr auto operator/(const vec_t<T, N>& lhs, const U& rhs) -> vec_t<U, N>;
    template <internal::is_arithmetic T, int N, internal::is_quantity U>    constexpr auto operator/(const U& lhs, const vec_t<T, N>& rhs) -> vec_t<U, N>;

	template <internal::is_quantity T, int N>			    constexpr auto operator-(const vec_t<T, N>& value)->vec_t<T, N>;
}

export namespace gse::unitless {
	template <typename T, int N, internal::is_quantity U>	constexpr auto operator*(const vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<U, N>;
	template <typename T, int N, internal::is_quantity U>	constexpr auto operator*(const U& lhs, const vec_t<T, N>& rhs) -> unit::vec_t<U, N>;
	template <typename T, int N, internal::is_quantity U>	constexpr auto operator/(const vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<U, N>;
	template <typename T, int N, internal::is_quantity U>	constexpr auto operator/(const U& lhs, const vec_t<T, N>& rhs) -> unit::vec_t<U, N>;
}

export namespace gse {
	template <typename T, int N>                using vec_t = unit::vec_t<T, N>;
	template <typename T>					    using vec2_t = vec_t<T, 2>;
	template <typename T>					    using vec3_t = vec_t<T, 3>;
	template <typename T>					    using vec4_t = vec_t<T, 4>;

    template <typename T>                       using vec2  = vec_t<T, 2>;
    template <typename T>                       using vec2d = vec_t<T, 2>;
    template <typename T>                       using vec2i = vec_t<T, 2>;

    template <typename T>                       using vec3  = vec_t<T, 3>;
    template <typename T>                       using vec3d = vec_t<T, 3>;
    template <typename T>                       using vec3i = vec_t<T, 3>;

	template <typename T>                       using vec4  = vec_t<T, 4>;
	template <typename T>                       using vec4d = vec_t<T, 4>;
	template <typename T>                       using vec4i = vec_t<T, 4>;

    enum class axis : std::uint8_t {
        x = 0,
        y = 1,
        z = 2,
        w = 3
    };
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator+(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage + rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator-(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage - rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, gse::internal::is_arithmetic U, int N> 
constexpr auto gse::unit::operator*(const vec_t<T, N>& lhs, const U& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage * rhs);
}

template <gse::internal::is_arithmetic U, gse::internal::is_quantity T, int N> 
constexpr auto gse::unit::operator*(const U& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs * rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unit::operator/(const vec_t<T, N>& lhs, const U& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage / rhs);
}

template <gse::internal::is_arithmetic U, gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator/(const U& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs / rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator*(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs) {
	using result_q = decltype(T()* U());
	return unit::vec_t<result_q, N>(lhs.template as<typename T::default_unit>().storage * rhs.template as<typename U::default_unit>().storage);
}

template <gse::internal::is_quantity T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator/(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs) {
	using result_q = decltype(T() / U());
	return unit::vec_t<result_q, N>(lhs.template as<typename T::default_unit>().storage / rhs.template as<typename U::default_unit>().storage);
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator/(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> unitless::vec_t<typename T::value_type, N> {
	return unitless::vec_t<typename T::value_type, N>(lhs.template as<typename T::default_unit>().storage / rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator*(const vec_t<T, N>& lhs, const U& rhs) {
	using result_q = decltype(T() * U());
	return unit::vec_t<result_q, N>(lhs.template as<typename T::default_unit>().storage * rhs.template as<typename U::default_unit>());
}

template <gse::internal::is_quantity T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator/(const vec_t<T, N>& lhs, const U& rhs) {
	using result_q = decltype(T() / U());
	return unit::vec_t<result_q, N>(lhs.template as<typename T::default_unit>().storage / rhs.template as<typename U::default_unit>());
}

template <gse::internal::is_quantity T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator*(const U& lhs, const vec_t<T, N>& rhs) {
	using result_q = decltype(U() * T());
	return unit::vec_t<result_q, N>(lhs.template as<typename U::default_unit>() * rhs.template as<typename T::default_unit>());
}

template <gse::internal::is_quantity T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator/(const U& lhs, const vec_t<T, N>& rhs) {
	using result_q = decltype(U() / T());
	return unit::vec_t<result_q, N>(lhs.template as<typename U::default_unit>() / rhs.template as<typename T::default_unit>());
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator/(const vec_t<T, N>& lhs, const T& rhs) -> unitless::vec_t<typename T::value_type, N> {
	return unitless::vec_t<typename T::value_type, N>(lhs.template as<typename T::default_unit>().storage / rhs);
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator/(const T& lhs, const vec_t<T, N>& rhs) -> unitless::vec_t<typename T::value_type, N> {
	return unitless::vec_t<typename T::value_type, N>(lhs / rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator+=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>& {
	auto v1 = lhs.template as<typename T::default_unit>();
	auto v2 = rhs.template as<typename T::default_unit>();

	v1.storage += v2.storage; 
	lhs = unit::vec_t<T, N>(v1); 
	return lhs;
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator-=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>& {
	auto v1 = lhs.template as<typename T::default_unit>();
	auto v2 = rhs.template as<typename T::default_unit>();

	v1.storage -= v2.storage;
	lhs = unit::vec_t<T, N>(v1);
	return lhs;
}

template <gse::internal::is_quantity T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unit::operator*=(vec_t<T, N>& lhs, const U& rhs) -> vec_t<T, N>& {
	auto v1 = lhs.template as<typename T::default_unit>();

	v1.storage *= rhs;
	lhs = unit::vec_t<T, N>(v1);
	return lhs;
}

template <gse::internal::is_quantity T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unit::operator/=(vec_t<T, N>& lhs, const U& rhs) -> vec_t<T, N>& {
	auto v1 = lhs.template as<typename T::default_unit>();

	v1.storage /= rhs;
	lhs = unit::vec_t<T, N>(v1);
	return lhs;
}

template <gse::internal::is_quantity T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unit::operator+(const vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage + rhs.storage);
}

template <gse::internal::is_arithmetic T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator+(const unitless::vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.storage + rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unit::operator-(const vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage - rhs.storage);
}

template <gse::internal::is_arithmetic T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator-(const unitless::vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.storage - rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unit::operator*(const vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage * rhs.storage);
}

template <gse::internal::is_arithmetic T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator*(const unitless::vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.storage * rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unit::operator/(const vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage / rhs.storage);
}

template <gse::internal::is_arithmetic T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator/(const unitless::vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.storage / rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_arithmetic T, int N, gse::internal::is_quantity U>
constexpr auto gse::unit::operator*(const vec_t<T, N>& lhs, const U& rhs) -> vec_t<U, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage * rhs.template as<typename T::default_unit>());
}

template <gse::internal::is_arithmetic T, int N, gse::internal::is_quantity U>
constexpr auto gse::unit::operator*(const U& lhs, const vec_t<T, N>& rhs) -> vec_t<U, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>() * rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_arithmetic T, int N, gse::internal::is_quantity U>
constexpr auto gse::unit::operator/(const vec_t<T, N>& lhs, const U& rhs) -> vec_t<U, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage / rhs.template as<typename T::default_unit>());
}

template <gse::internal::is_arithmetic T, int N, gse::internal::is_quantity U>
constexpr auto gse::unit::operator/(const U& lhs, const vec_t<T, N>& rhs) -> vec_t<U, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>() / rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator-(const vec_t<T, N>& value) -> vec_t<T, N> {
	return unit::vec_t<T, N>(-value.template as<typename T::default_unit>().storage);
}

template <typename T, int N, gse::internal::is_quantity U>
constexpr auto gse::unitless::operator*(const vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<U, N> {
	return unit::vec_t<U, N>(lhs.storage * rhs.template as<typename U::default_unit>());
}

template <typename T, int N, gse::internal::is_quantity U>
constexpr auto gse::unitless::operator*(const U& lhs, const vec_t<T, N>& rhs) -> unit::vec_t<U, N> {
	return unit::vec_t<U, N>(lhs.template as<typename U::default_unit>() * rhs.storage);
}

template <typename T, int N, gse::internal::is_quantity U>
constexpr auto gse::unitless::operator/(const vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<U, N> {
	return unit::vec_t<U, N>(lhs.storage / rhs.template as<typename U::default_unit>());
}

template <typename T, int N, gse::internal::is_quantity U>
constexpr auto gse::unitless::operator/(const U& lhs, const vec_t<T, N>& rhs) -> unit::vec_t<U, N> {
	return unit::vec_t<U, N>(lhs.template as<typename U::default_unit>() / rhs.storage);
}
