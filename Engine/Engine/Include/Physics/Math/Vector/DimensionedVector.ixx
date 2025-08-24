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
    constexpr auto value(const T& value) -> Q {
        if constexpr (internal::is_quantity<T>) {
            return value.as_default_unit();
        }
        else {
            return value;
        }
    }
}

namespace gse::unit {
	template <typename Q>
	concept quantity_simd_safe =
		std::is_trivially_copyable_v<Q> &&
		std::is_standard_layout_v<Q> &&
		sizeof(Q) == sizeof(typename Q::value_type) &&
		alignof(Q) == alignof(typename Q::value_type);

    template <quantity_simd_safe Q, int N>
	struct vec_t : internal::vec_t<vec_t<Q, N>, Q, N> {
		using internal::vec_t<vec_t, Q, N>::vec_t;
        using value_type = typename Q::value_type;

        template <typename... Args> requires ((std::is_convertible_v<Args, typename Q::value_type> || quantity_simd_safe<Args>) && ...)
        constexpr vec_t(Args... args);

		template <typename U>
		constexpr vec_t(const unitless::vec_t<U, N>& other);

		template <internal::is_arithmetic U> 
		constexpr vec_t(const vec::storage<U, N>& other);

        template <internal::is_unit U> requires has_same_tag<Q, U>
        [[nodiscard]] constexpr auto as() const -> unitless::vec_t<value_type, N>;

		constexpr auto operator<=>(const vec_t& other) const = default;
    };

	template <quantity_simd_safe Q, std::size_t N>
	constexpr auto value_span(vec_t<Q, N>& a) -> std::span<typename Q::value_type, N> {
		using v = typename Q::value_type;
		using aq = std::array<Q, N>;
		using av = std::array<v, N>;

		aq& arr_q = a.storage.data;
		auto* arr_v = std::launder(reinterpret_cast<av*>(&arr_q));
		return std::span<v, N>(*arr_v);
	}

	template <quantity_simd_safe Q, std::size_t N>
	constexpr auto value_span(const vec_t<Q, N>& a) -> std::span<const typename Q::value_type, N> {
		using v = typename Q::value_type;
		using aq = const std::array<Q, N>;
		using av = const std::array<v, N>;

		const aq& arr_q = a.storage.data;
		auto* arr_v = std::launder(reinterpret_cast<const av*>(&arr_q));
		return std::span<const v, N>(*arr_v);
	}
}

export namespace gse {
	template <typename T, int N> using vec_t = unit::vec_t<T, N>;
	template <typename T> using vec2_t = vec_t<T, 2>;
	template <typename T> using vec3_t = vec_t<T, 3>;
	template <typename T> using vec4_t = vec_t<T, 4>;

	template <typename T> using vec2 = vec_t<T, 2>;
	template <typename T> using vec2d = vec_t<T, 2>;
	template <typename T> using vec2i = vec_t<T, 2>;

	template <typename T> using vec3 = vec_t<T, 3>;
	template <typename T> using vec3d = vec_t<T, 3>;
	template <typename T> using vec3i = vec_t<T, 3>;

	template <typename T> using vec4 = vec_t<T, 4>;
	template <typename T> using vec4d = vec_t<T, 4>;
	template <typename T> using vec4i = vec_t<T, 4>;
}

template <gse::unit::quantity_simd_safe Q, int N>
template <typename ... Args> requires ((std::is_convertible_v<Args, typename Q::value_type> || gse::unit::quantity_simd_safe<Args>) && ...)
constexpr gse::unit::vec_t<Q, N>::vec_t(Args... args) : internal::vec_t<vec_t, Q, N>(unit::value<value_type>(args)...) {}

template <gse::unit::quantity_simd_safe Q, int N>
template <typename U>
constexpr gse::unit::vec_t<Q, N>::vec_t(const unitless::vec_t<U, N>& other): internal::vec_t<vec_t, Q, N>() {
	for (std::size_t i = 0; i < N; ++i) {
		this->storage[i] = Q(static_cast<value_type>(other[i]));
	}
}

template <gse::unit::quantity_simd_safe Q, int N>
template <gse::internal::is_arithmetic U>
constexpr gse::unit::vec_t<Q, N>::vec_t(const vec::storage<U, N>& other): internal::vec_t<vec_t, Q, N>() {
	for (std::size_t i = 0; i < N; ++i) {
		this->storage[i] = Q(static_cast<value_type>(other[i]));
	}
}

template <gse::unit::quantity_simd_safe Q, int N>
template <gse::internal::is_unit U> requires gse::unit::has_same_tag<Q, U>
constexpr auto gse::unit::vec_t<Q, N>::as() const -> unitless::vec_t<value_type, N> {
	vec::storage<value_type, N> result;
	simd::mul_s(value_span(*this), U::conversion_factor, std::span(result.data));
	return result;
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
