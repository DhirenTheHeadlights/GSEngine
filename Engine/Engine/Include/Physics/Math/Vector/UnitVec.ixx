export module gse.physics.math.unit_vec;

import std;
import glm;

import gse.physics.math.units.quant;
import gse.physics.math.base_vec;
import gse.physics.math.unitless_vec;

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

        template <internal::is_quantity U>
		constexpr vec_t(const vec_t<U, N>& other) : internal::vec_t<vec_t, Q, N>() {
	        for (size_t i = 0; i < N; ++i) {
		        this->storage[i] = Q(static_cast<value_type>(other[i].as_default_unit()));
	        }
        }

		template <typename U>
		constexpr vec_t(const unitless::vec_t<U, N>& other) : internal::vec_t<vec_t, Q, N>() {
			for (size_t i = 0; i < N; ++i) {
				this->storage[i] = Q(static_cast<value_type>(other[i]));
			}
		}

		template <typename U> requires std::is_arithmetic_v<U>
		constexpr vec_t(const vec::storage<U, N> other) : internal::vec_t<vec_t, Q, N>() {
			for (size_t i = 0; i < N; ++i) {
				this->storage[i] = Q(static_cast<value_type>(other[i]));
			}
		}

        template <typename U>
			requires internal::is_unit<U> && has_same_tag<Q, U>
        [[nodiscard]] constexpr auto as() const -> unitless::vec_t<value_type, N> {
			unitless::vec_t<value_type, N> result;
            for (int i = 0; i < N; ++i) {
                result[i] = this->storage[i].as_default_unit() * U::conversion_factor;
            }
			return result;
        }
    };
}

export namespace gse {
	template <internal::is_quantity T, int N>												constexpr auto operator+(const unit::vec_t<T, N>& lhs, const unit::vec_t<T, N>& rhs) -> unit::vec_t<T, N>;
	template <internal::is_quantity T, int N>												constexpr auto operator-(const unit::vec_t<T, N>& lhs, const unit::vec_t<T, N>& rhs) -> unit::vec_t<T, N>;
	template <internal::is_quantity T, typename U, int N> requires std::is_arithmetic_v<U>	constexpr auto operator*(const unit::vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<T, N>;
	template <typename T, internal::is_quantity U, int N> requires std::is_arithmetic_v<T>	constexpr auto operator*(const U& lhs, const unit::vec_t<T, N>& rhs) -> unit::vec_t<U, N>;
	template <internal::is_quantity T, typename U, int N> requires std::is_arithmetic_v<U>	constexpr auto operator/(const unit::vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<T, N>;
	template <typename T, internal::is_quantity U, int N> requires std::is_arithmetic_v<T>	constexpr auto operator/(const U& lhs, const unit::vec_t<T, N>& rhs) -> unit::vec_t<U, N>;

	template <internal::is_quantity T, int N>												constexpr auto operator+=(unit::vec_t<T, N>& lhs, const unit::vec_t<T, N>& rhs) -> unit::vec_t<T, N>&;
	template <internal::is_quantity T, int N>												constexpr auto operator-=(unit::vec_t<T, N>& lhs, const unit::vec_t<T, N>& rhs) -> unit::vec_t<T, N>&;
	template <internal::is_quantity T, typename U, int N> requires std::is_arithmetic_v<U>	constexpr auto operator*=(unit::vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<T, N>&;
	template <internal::is_quantity T, typename U, int N> requires std::is_arithmetic_v<U>	constexpr auto operator/=(unit::vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<T, N>&;

    template <internal::is_quantity T, typename U, int N>									constexpr auto operator+(const unit::vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> unit::vec_t<T, N>;
    template <typename T, internal::is_quantity U, int N>									constexpr auto operator+(const unitless::vec_t<T, N>& lhs, const unit::vec_t<U, N>& rhs) -> unit::vec_t<T, N>;
    template <internal::is_quantity T, typename U, int N>									constexpr auto operator-(const unit::vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> unit::vec_t<T, N>;
    template <typename T, internal::is_quantity U, int N>									constexpr auto operator-(const unitless::vec_t<T, N>& lhs, const unit::vec_t<U, N>& rhs) -> unit::vec_t<T, N>;
    template <internal::is_quantity T, typename U, int N>									constexpr auto operator*(const unit::vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> unit::vec_t<T, N>;
    template <typename T, internal::is_quantity U, int N>									constexpr auto operator*(const unitless::vec_t<T, N>& lhs, const unit::vec_t<U, N>& rhs) -> unit::vec_t<T, N>;
    template <internal::is_quantity T, typename U, int N>									constexpr auto operator/(const unit::vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> unit::vec_t<T, N>;
    template <typename T, internal::is_quantity U, int N>									constexpr auto operator/(const unitless::vec_t<T, N>& lhs, const unit::vec_t<U, N>& rhs) -> unit::vec_t<T, N>;

    template <typename T, int N, internal::is_quantity U>									constexpr auto operator*(const unit::vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<U, N>;
    template <typename T, int N, internal::is_quantity U>									constexpr auto operator*(const U& lhs, const unit::vec_t<T, N>& rhs) -> unit::vec_t<U, N>;
    template <typename T, int N, internal::is_quantity U>									constexpr auto operator/(const unit::vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<U, N>;
    template <typename T, int N, internal::is_quantity U>									constexpr auto operator/(const U& lhs, const unit::vec_t<T, N>& rhs) -> unit::vec_t<U, N>;

	template <typename T, int N, internal::is_quantity U>									constexpr auto operator*(const unitless::vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<U, N>;
	template <typename T, int N, internal::is_quantity U>									constexpr auto operator*(const U& lhs, const unitless::vec_t<T, N>& rhs) -> unit::vec_t<U, N>;
	template <typename T, int N, internal::is_quantity U>									constexpr auto operator/(const unitless::vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<U, N>;
	template <typename T, int N, internal::is_quantity U>									constexpr auto operator/(const U& lhs, const unitless::vec_t<T, N>& rhs) -> unit::vec_t<U, N>;

	template <internal::is_quantity T, int N>												constexpr auto operator-(const unit::vec_t<T, N>& value)->unit::vec_t<T, N>;
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
constexpr auto gse::operator+(const unit::vec_t<T, N>& lhs, const unit::vec_t<T, N>& rhs) -> unit::vec_t<T, N> {
	unit::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] + rhs[i];
	}
	return result;
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::operator-(const unit::vec_t<T, N>& lhs, const unit::vec_t<T, N>& rhs) -> unit::vec_t<T, N> {
	unit::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] - rhs[i];
	}
	return result;
}

template <gse::internal::is_quantity T, typename U, int N> requires std::is_arithmetic_v<U>
constexpr auto gse::operator*(const unit::vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<T, N> {
	unit::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] * rhs;
	}
	return result;
}

template <typename T, gse::internal::is_quantity U, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::operator*(const U& lhs, const unit::vec_t<T, N>& rhs) -> unit::vec_t<U, N> {
	unit::vec_t<U, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs * rhs[i];
	}
	return result;
}

template <gse::internal::is_quantity T, typename U, int N> requires std::is_arithmetic_v<U>
constexpr auto gse::operator/(const unit::vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<T, N> {
	unit::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] / rhs;
	}
	return result;
}

template <typename T, gse::internal::is_quantity U, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::operator/(const U& lhs, const unit::vec_t<T, N>& rhs) -> unit::vec_t<U, N> {
	unit::vec_t<U, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs / rhs[i];
	}
	return result;
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::operator+=(unit::vec_t<T, N>& lhs, const unit::vec_t<T, N>& rhs) -> unit::vec_t<T, N>& {
	for (int i = 0; i < N; ++i) {
		lhs[i] += rhs[i];
	}
	return lhs;
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::operator-=(unit::vec_t<T, N>& lhs, const unit::vec_t<T, N>& rhs) -> unit::vec_t<T, N>& {
	for (int i = 0; i < N; ++i) {
		lhs[i] -= rhs[i];
	}
	return lhs;
}

template <gse::internal::is_quantity T, typename U, int N> requires std::is_arithmetic_v<U>
constexpr auto gse::operator*=(unit::vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<T, N>& {
	for (int i = 0; i < N; ++i) {
		lhs[i] *= rhs;
	}
	return lhs;
}

template <gse::internal::is_quantity T, typename U, int N> requires std::is_arithmetic_v<U>
constexpr auto gse::operator/=(unit::vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<T, N>& {
	for (int i = 0; i < N; ++i) {
		lhs[i] /= rhs;
	}
	return lhs;
}

template <gse::internal::is_quantity T, typename U, int N>
constexpr auto gse::operator+(const unit::vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> unit::vec_t<T, N> {
	unit::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] + T(rhs[i]);
	}
	return result;
}

template <typename T, gse::internal::is_quantity U, int N>
constexpr auto gse::operator+(const unitless::vec_t<T, N>& lhs, const unit::vec_t<U, N>& rhs) -> unit::vec_t<T, N> {
	unit::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = U(lhs[i]) + rhs[i];
	}
	return result;
}

template <gse::internal::is_quantity T, typename U, int N>
constexpr auto gse::operator-(const unit::vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> unit::vec_t<T, N> {
	unit::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] - T(rhs[i]);
	}
	return result;
}

template <typename T, gse::internal::is_quantity U, int N>
constexpr auto gse::operator-(const unitless::vec_t<T, N>& lhs, const unit::vec_t<U, N>& rhs) -> unit::vec_t<T, N> {
	unit::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = U(lhs[i]) - rhs[i];
	}
	return result;
}

template <gse::internal::is_quantity T, typename U, int N>
constexpr auto gse::operator*(const unit::vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> unit::vec_t<T, N> {
	unit::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] * rhs[i];
	}
	return result;
}

template <typename T, gse::internal::is_quantity U, int N>
constexpr auto gse::operator*(const unitless::vec_t<T, N>& lhs, const unit::vec_t<U, N>& rhs) -> unit::vec_t<T, N> {
	unit::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] * rhs[i];
	}
	return result;
}

template <gse::internal::is_quantity T, typename U, int N>
constexpr auto gse::operator/(const unit::vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> unit::vec_t<T, N> {
	unit::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] / rhs[i];
	}
	return result;
}

template <typename T, gse::internal::is_quantity U, int N>
constexpr auto gse::operator/(const unitless::vec_t<T, N>& lhs, const unit::vec_t<U, N>& rhs) -> unit::vec_t<T, N> {
	unit::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] / rhs[i];
	}
	return result;
}

template <typename T, int N, gse::internal::is_quantity U>
constexpr auto gse::operator*(const unit::vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<U, N> {
	unit::vec_t<U, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] * rhs;
	}
	return result;
}

template <typename T, int N, gse::internal::is_quantity U>
constexpr auto gse::operator*(const U& lhs, const unit::vec_t<T, N>& rhs) -> unit::vec_t<U, N> {
	unit::vec_t<U, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs * rhs[i];
	}
	return result;
}

template <typename T, int N, gse::internal::is_quantity U>
constexpr auto gse::operator/(const unit::vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<U, N> {
	unit::vec_t<U, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] / rhs;
	}
	return result;
}

template <typename T, int N, gse::internal::is_quantity U>
constexpr auto gse::operator/(const U& lhs, const unit::vec_t<T, N>& rhs) -> unit::vec_t<U, N> {
	unit::vec_t<U, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs / rhs[i];
	}
	return result;
}

template <typename T, int N, gse::internal::is_quantity U>
constexpr auto gse::operator*(const unitless::vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<U, N> {
	unit::vec_t<U, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] * rhs;
	}
	return result;
}

template <typename T, int N, gse::internal::is_quantity U>
constexpr auto gse::operator*(const U& lhs, const unitless::vec_t<T, N>& rhs) -> unit::vec_t<U, N> {
	unit::vec_t<U, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs * rhs[i];
	}
	return result;
}

template <typename T, int N, gse::internal::is_quantity U>
constexpr auto gse::operator/(const unitless::vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<U, N> {
	unit::vec_t<U, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs[i] / rhs;
	}
	return result;
}

template <typename T, int N, gse::internal::is_quantity U>
constexpr auto gse::operator/(const U& lhs, const unitless::vec_t<T, N>& rhs) -> unit::vec_t<U, N> {
	unit::vec_t<U, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = lhs / rhs[i];
	}
	return result;
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::operator-(const unit::vec_t<T, N>& value) -> unit::vec_t<T, N> {
	unit::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = -value[i];
	}
	return result;
}
