export module gse.physics.math.unit_vec;

import std;

import gse.physics.math.units.quant;
import gse.physics.math.vec;

namespace gse::unit_vec {
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

namespace gse {
	struct placeholder_tag {};
	constexpr char placeholder_name[] = "placeholder";
	using placeholder_unit = internal::unit<placeholder_tag, 1.0f, placeholder_name>;
	using placeholder_units = internal::unit_list<placeholder_unit>;

    export template <typename T>
	class unitless_t : internal::quantity<T, placeholder_tag, placeholder_unit, placeholder_units> {
    	using internal::quantity<T, placeholder_tag, placeholder_unit, placeholder_units>::quantity;

    	constexpr operator T() const {
			return this->m_val;
		}
	};

	static_assert(internal::is_quantity<unitless_t<float>>);

    export using unitless = unitless_t<float>;
}

namespace gse::unit_vec {
    template <internal::is_quantity Q, int N>
    struct vec_t : unitless_vec::vec_t<vec_t<Q, N>, typename Q::value_type, N> {
        using value_type = typename Q::value_type;

        template <typename... Args>
            requires ((std::is_convertible_v<Args, typename Q::value_type> || internal::is_quantity<Args>) && ...)
        constexpr vec_t(Args... args) : unitless_vec::vec_t<vec_t, value_type, N>(unit_vec::get_value<value_type>(args)...) {}

        template <internal::is_quantity U>
			requires std::is_convertible_v<unitless_t<typename U::value_type>, U> || std::is_same_v<typename Q::quantity_tag, typename U::quantity_tag>
        constexpr vec_t(const vec_t<U, N>& other) { // Convert from unitless or same unit/different value_type
            for (size_t i = 0; i < N; ++i) {
                this->data[i] = static_cast<value_type>(other[i]);
            }
        }

        template <typename U>
			requires internal::is_unit<U> && has_same_tag<Q, U>
        [[nodiscard]] constexpr auto as() const -> vec_t<unitless_t<value_type>, N> {
            for (int i = 0; i < N; ++i) {
                this->data[i] *= U::conversion_factor;
            }
            return { this->data };
        }
    };
}

export namespace gse {
    template <typename T, int N>                  using unitless_vec_t = unit_vec::vec_t<unitless_t<T>, N>;
	template <typename T>                         using unitless_vec2_t = unitless_vec_t<T, 2>;
	template <typename T>                         using unitless_vec3_t = unitless_vec_t<T, 3>;
	template <typename T>                         using unitless_vec4_t = unitless_vec_t<T, 4>;

	template <typename T, int N>                  using vec_t = unit_vec::vec_t<T, N>;
	template <typename T>					      using vec2_t = vec_t<T, 2>;
	template <typename T>					      using vec3_t = vec_t<T, 3>;
	template <typename T>					      using vec4_t = vec_t<T, 4>;

    template <typename T = unitless>              using vec2  = vec_t<T, 2>;
    template <typename T = unitless_t<double>>    using vec2d = vec_t<T, 2>;
    template <typename T = unitless_t<int>>       using vec2i = vec_t<T, 2>;

    template <typename T = unitless>              using vec3  = vec_t<T, 3>;
    template <typename T = unitless_t<double>>    using vec3d = vec_t<T, 3>;
    template <typename T = unitless_t<int>>       using vec3i = vec_t<T, 3>;

	template <typename T = unitless>              using vec4  = vec_t<T, 4>;
	template <typename T = unitless_t<double>>    using vec4d = vec_t<T, 4>;
	template <typename T = unitless_t<int>>       using vec4i = vec_t<T, 4>;

    enum class axis : std::uint8_t {
        x = 0,
        y = 1,
        z = 2,
        w = 3
    };
}

namespace gse {
    template <typename T>
    concept is_unitless = std::is_same_v<T, unitless_t<typename T::value_type>>;

    template <typename T, typename U>
    using non_unitless_type = 
        std::conditional_t<
			is_unitless<T> && !is_unitless<U>, U,
			std::conditional_t<
				!is_unitless<T> && is_unitless<U>, T,
				unitless
			>
		>;

    template <typename T, int N>
	auto as_default_unit(const vec_t<T, N>& vec) -> vec_t<unitless_t<T>, N> {
		if constexpr (is_unitless<T>) {
			return vec;
		}
		else {
			return vec.template as<T::default_unit>();
		}
    } 

    template <typename T>
        requires internal::is_unit<T>
	auto as_default_unit(const T& scalar) -> typename T::value_type {
		return scalar.template as<T::default_unit>();
    }

	/// These free functions are designed to allow for the use of unitless vectors in operations that would otherwise not be allowed for multiple vectors with units
	/// The operator overloads return the non-unitless type of the two types being operated on

    export template <typename T, typename U, int N> requires is_unitless<T> || is_unitless<U> constexpr auto operator*(const vec_t<T, N>& a, const vec_t<U, N>& b) -> vec_t<non_unitless_type<T, U>, N>;
    export template <typename T, typename U, int N> requires is_unitless<T> || is_unitless<U> constexpr auto operator/(const vec_t<T, N>& a, const vec_t<U, N>& b) -> vec_t<non_unitless_type<T, U>, N>;
    export template <typename T, typename U, int N> requires is_unitless<T> || is_unitless<U> constexpr auto operator*(const vec_t<T, N>& a, const U& scalar) -> vec_t<non_unitless_type<T, U>, N>;
    export template <typename T, typename U, int N> requires is_unitless<T> || is_unitless<U> constexpr auto operator*(const T& scalar, const vec_t<U, N>& a) -> vec_t<non_unitless_type<T, U>, N>;
    export template <typename T, typename U, int N> requires is_unitless<T> || is_unitless<U> constexpr auto operator/(const vec_t<T, N>& a, const U& scalar) -> vec_t<non_unitless_type<T, U>, N>;
    export template <typename T, typename U, int N> requires is_unitless<T> || is_unitless<U> constexpr auto operator/(const T& scalar, const vec_t<U, N>& a) -> vec_t<non_unitless_type<T, U>, N>;
    export template <typename T, typename U> requires is_unitless<T> || is_unitless<U> constexpr auto operator*(const T& a, const U& b) -> non_unitless_type<T, U>;
    export template <typename T, typename U> requires is_unitless<T> || is_unitless<U> constexpr auto operator/(const T& a, const U& b) -> non_unitless_type<T, U>;
}

template <typename T, typename U, int N> requires gse::is_unitless<T> || gse::is_unitless<U>
constexpr auto gse::operator*(const vec_t<T, N>& a, const vec_t<U, N>& b) -> vec_t<non_unitless_type<T, U>, N> {
	return vec_t<non_unitless_type<T, U>, N>(as_default_unit(a) * as_default_unit(b));
}

template <typename T, typename U, int N> requires gse::is_unitless<T> || gse::is_unitless<U>
constexpr auto gse::operator/(const vec_t<T, N>& a, const vec_t<U, N>& b) -> vec_t<non_unitless_type<T, U>, N> {
	return vec_t<non_unitless_type<T, U>, N>(as_default_unit(a) / as_default_unit(b));
}

template <typename T, typename U, int N> requires gse::is_unitless<T> || gse::is_unitless<U>
constexpr auto gse::operator*(const vec_t<T, N>& a, const U& scalar) -> vec_t<non_unitless_type<T, U>, N> {
	return vec_t<non_unitless_type<T, U>, N>(as_default_unit(a) * as_default_unit(scalar));
}

template <typename T, typename U, int N> requires gse::is_unitless<T> || gse::is_unitless<U>
constexpr auto gse::operator*(const T& scalar, const vec_t<U, N>& a) -> vec_t<non_unitless_type<T, U>, N> {
	return vec_t<non_unitless_type<T, U>, N>(as_default_unit(scalar) * as_default_unit(a));
}

template <typename T, typename U, int N> requires gse::is_unitless<T> || gse::is_unitless<U>
constexpr auto gse::operator/(const vec_t<T, N>& a, const U& scalar) -> vec_t<non_unitless_type<T, U>, N> {
	return vec_t<non_unitless_type<T, U>, N>(as_default_unit(a) / as_default_unit(scalar));
}

template <typename T, typename U, int N> requires gse::is_unitless<T> || gse::is_unitless<U>
constexpr auto gse::operator/(const T& scalar, const vec_t<U, N>& a) -> vec_t<non_unitless_type<T, U>, N> {
	return vec_t<non_unitless_type<T, U>, N>(as_default_unit(scalar) / as_default_unit(a));
}

template <typename T, typename U> requires gse::is_unitless<T> || gse::is_unitless<U>
constexpr auto gse::operator*(const T& a, const U& b) -> non_unitless_type<T, U> {
    return as_default_unit(a) * as_default_unit(b);
}

template <typename T, typename U> requires gse::is_unitless<T> || gse::is_unitless<U>
constexpr auto gse::operator/(const T & a, const U & b) -> non_unitless_type<T, U> {
	return as_default_unit(a) / as_default_unit(b);
}