export module vec;

import std;

namespace gse::internal {
    export template<typename... Units>
    struct unit_list {
        using type = std::tuple<Units...>;
    };

    export template <typename QuantityTagType, float ConversionFactorType, const char UnitNameType[]>
    struct unit {
        using quantity_tag = QuantityTagType;
        static constexpr float conversion_factor = ConversionFactorType;
        static constexpr const char* unit_name = UnitNameType;
    };

    template <typename T>
    concept is_unit = requires {
        typename std::remove_cvref_t<T>::quantity_tag;
        { std::remove_cvref_t<T>::unit_name } -> std::convertible_to<const char*>;
        { std::remove_cvref_t<T>::conversion_factor } -> std::convertible_to<float>;
    };

    template <typename UnitType, typename ValidUnits>
    constexpr auto is_valid_unit_for_quantity() -> bool {
        return std::apply([]<typename... Units>(Units... u) {
            return ((std::is_same_v<typename UnitType::quantity_tag, typename Units::quantity_tag>) || ...);
        }, typename ValidUnits::type{});
    }

    export template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    class quantity {
    public:
        using value_type = ArithmeticType;
        using quantity_tag = QuantityTagType;
        using default_unit = DefaultUnitType;
        using units = ValidUnits;

        constexpr quantity() = default;
		constexpr quantity(ArithmeticType value) : m_val(value) {}

        template <is_unit UnitType>
        constexpr auto set(ArithmeticType value) -> void;

        template <is_unit UnitType>
        constexpr auto as() const -> float;

        constexpr auto as_default_unit() const -> ArithmeticType;

        constexpr auto operator=(const quantity& other) -> quantity&;
        constexpr auto operator+(const quantity& other) const -> quantity;
        constexpr auto operator-(const quantity& other) const -> quantity;
        constexpr auto operator*(ArithmeticType scalar) const -> quantity;
        constexpr auto operator/(ArithmeticType scalar) const -> quantity;

        constexpr auto operator+=(const quantity& other) -> quantity&;
        constexpr auto operator-=(const quantity& other) -> quantity&;
        constexpr auto operator*=(ArithmeticType scalar) -> quantity&;
        constexpr auto operator/=(ArithmeticType scalar) -> quantity&;

        constexpr auto operator==(const quantity& other) const -> bool;
        constexpr auto operator!=(const quantity& other) const -> bool;
        constexpr auto operator<(const quantity& other) const -> bool;
        constexpr auto operator>(const quantity& other) const -> bool;
        constexpr auto operator<=(const quantity& other) const -> bool;
        constexpr auto operator>=(const quantity& other) const -> bool;

        constexpr auto operator-() const -> quantity;

        template <is_unit UnitType>
        constexpr static auto from(ArithmeticType value) -> quantity;
    private:
        template <is_unit UnitType>
        constexpr auto get_converted_value(float value) const -> float;

        ArithmeticType m_val = static_cast<ArithmeticType>(0);
    };

    // Definitions with constexpr

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    template <is_unit UnitType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::set(
	    ArithmeticType value) -> void {
        static_assert(is_valid_unit_for_quantity<UnitType, ValidUnits>(), "Invalid unit type for assignment");
        m_val = get_converted_value<UnitType>(value);
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    template <is_unit UnitType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::as() const -> float {
        static_assert(is_valid_unit_for_quantity<UnitType, ValidUnits>(), "Invalid unit type for conversion");
        return m_val / UnitType::conversion_factor;
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType,
                            ValidUnits>::as_default_unit() const -> ArithmeticType {
        return m_val;
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator=(
	    const quantity& other) -> quantity& {
        m_val = other.m_val;
        return *this;
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator+(
	    const quantity& other) const -> quantity {
        return quantity(m_val + other.m_val);
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator-(
	    const quantity& other) const -> quantity {
        return quantity(m_val - other.m_val);
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator*(
	    ArithmeticType scalar) const -> quantity {
        return quantity(m_val * scalar);
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator/(
	    ArithmeticType scalar) const -> quantity {
        return quantity(m_val / scalar);
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator+=(
	    const quantity& other) -> quantity& {
        m_val += other.m_val;
        return *this;
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator-=(
	    const quantity& other) -> quantity& {
        m_val -= other.m_val;
        return *this;
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator*=(
	    ArithmeticType scalar) -> quantity& {
        m_val *= scalar;
        return *this;
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator/=(
	    ArithmeticType scalar) -> quantity& {
        m_val /= scalar;
        return *this;
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator==(
	    const quantity& other) const -> bool {
        return m_val == other.m_val;
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator!=(
	    const quantity& other) const -> bool {
        return m_val != other.m_val;
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator<(
	    const quantity& other) const -> bool {
        return m_val < other.m_val;
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator>(
	    const quantity& other) const -> bool {
        return m_val > other.m_val;
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator<=(
	    const quantity& other) const -> bool {
        return m_val <= other.m_val;
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator>=(
	    const quantity& other) const -> bool {
        return m_val >= other.m_val;
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator
    -() const -> quantity {
        return quantity(-m_val);
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    template <is_unit UnitType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::from(
	    ArithmeticType value) -> quantity {
        static_assert(is_valid_unit_for_quantity<UnitType, ValidUnits>(), "Invalid unit type for conversion");
        quantity result;
        result.m_val = result.template get_converted_value<UnitType>(value);
        return result;
    }

    template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    template <is_unit UnitType>
    constexpr auto quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::get_converted_value(
	    float value) const -> float {
        return value * UnitType::conversion_factor;
    }
}

namespace gse::internal {
    template <typename Derived, int N, typename T>
    struct vec_base {
        constexpr auto operator[](std::size_t index) -> T& {
            return *(&static_cast<Derived*>(this)->x + index);
        }

        constexpr auto operator[](std::size_t index) const -> const T& {
            return *(&static_cast<const Derived*>(this)->x + index);
        }
    };

    template <typename T, int N = 1>
    struct vec : vec_base<vec<T, N>, N, T> {
        T x;

		constexpr vec() : x(static_cast<T>(0)) {}
        constexpr vec(const T& value) : x(value) {}
        constexpr vec(const T* values) : x(values[0]) {}
    };

    template <typename T>
    struct vec<T, 2> : vec_base<vec<T, 2>, 2, T> {
		T x, y;

		constexpr vec() : x(static_cast<T>(0)), y(static_cast<T>(0)) {}
		constexpr vec(const T& value) : x(value), y(value) {}
		constexpr vec(const T* values) : x(values[0]), y(values[1]) {}
		constexpr vec(const T& x, const T& y) : x(x), y(y) {}
	};

    template <typename T>
    struct vec<T, 3> : vec_base<vec<T, 3>, 3, T> {
		T x, y, z;

		constexpr vec() : x(static_cast<T>(0)), y(static_cast<T>(0)), z(static_cast<T>(0)) {}
		constexpr vec(const T& value) : x(value), y(value), z(value) {}
		constexpr vec(const T* values) : x(values[0]), y(values[1]), z(values[2]) {}
		constexpr vec(const T& x, const T& y, const T& z) : x(x), y(y), z(z) {}
    };

    template <typename T>
    struct vec<T, 4> : vec_base<vec<T, 4>, 4, T> {
		T x, y, z, w;

		constexpr vec() : x(static_cast<T>(0)), y(static_cast<T>(0)), z(static_cast<T>(0)), w(static_cast<T>(0)) {}
		constexpr vec(const T& value) : x(value), y(value), z(value), w(value) {}
		constexpr vec(const T* values) : x(values[0]), y(values[1]), z(values[2]), w(values[3]) {}
		constexpr vec(const T& x, const T& y, const T& z, const T& w) : x(x), y(y), z(z), w(w) {}
    };

	template <typename T> using vec2 = vec<T, 2>;
	template <typename T> using vec3 = vec<T, 3>;
	template <typename T> using vec4 = vec<T, 4>;
}

export namespace gse::unitless_vec {
    using vec2 = internal::vec2<float>;
    using vec3 = internal::vec3<float>;
    using vec4 = internal::vec4<float>;

    using vec2d = internal::vec2<double>;
    using vec3d = internal::vec3<double>;
    using vec4d = internal::vec4<double>;

    using vec2i = internal::vec2<int>;
    using vec3i = internal::vec3<int>;
    using vec4i = internal::vec4<int>;

    auto test() -> void {
        constexpr vec3 v1(1.0f, 2.0f, 3.0f);
        constexpr vec3i v2(1, 2, 3);
        constexpr vec3d v3(1.01, 2.01, 3.01);
        constexpr vec2 v4(1.0f, 2.0f);

        // Access named members
    	constexpr float x = v1.x;
        constexpr int y = v2.y;
        constexpr double z = v3.z;

        // Access by index
        const float a = v1[0];
        const int b = v2[1];
        const double c = v3[2];

		std::cout << x << " " << y << " " << z << " " << a << " " << b << " " << c << std::endl;
    }
}

namespace gse::unit_vec {
    template <typename Q>
    concept is_quantity = std::derived_from <
        std::remove_cvref_t<Q>,
        internal::quantity<
        typename std::remove_cvref_t<Q>::value_type,
        typename std::remove_cvref_t<Q>::quantity_tag,
        typename std::remove_cvref_t<Q>::default_unit,
        typename std::remove_cvref_t<Q>::units
        >
    >;

    template <typename T>
    concept is_quantity_or_unit = is_quantity<T> || internal::is_unit<T>;

    template <typename T, typename U>
    concept has_same_tag = std::is_same_v<typename T::quantity_tag, typename U::quantity_tag>;

    struct unitless;

    template <typename T>
    struct unit_to_quantity;

    template <typename T>
    struct get_quantity {
        using type = typename unit_to_quantity<std::remove_cvref_t<T>>::type;
    };

	template <typename Q, typename T>
    using convertible_type = std::conditional_t<
        internal::is_unit<Q>,
        typename Q::value_type,
        T
    >;
}

namespace gse::unit_vec3 {
	template <typename Q = unit_vec::unitless, typename T = float>
	struct vec3_t : internal::vec<typename Q::value_type, 3> {
		using value_type = typename Q::value_type;

        template <typename... Args>
            requires ((std::is_convertible_v<Args, typename Q::value_type> || unit_vec::is_quantity<Args>) && ...)
        constexpr vec3_t(Args... args);

        template <typename Other>
            requires unit_vec::has_same_tag<Other, Q>
        constexpr vec3_t(const vec3_t<Other>& other);

        template <typename U>
            requires internal::is_unit<U>
        [[nodiscard]] constexpr auto as() const -> vec3_t<>;
    };

	template <typename Q, typename T>
	constexpr auto get_value(const T& value) -> Q {
		if constexpr (unit_vec::is_quantity<T>) {
			return value.as_default_unit();
		}
        else {
            return value;
        }
	}

	template <typename Q, typename T>
	template <typename ... Args> requires ((std::is_convertible_v<Args, typename Q::value_type> || unit_vec::is_quantity<Args>) && ...)
	constexpr vec3_t<Q, T>::vec3_t(Args... args) : internal::vec<value_type, 3>(get_value<value_type>(args)...) {}

	template <typename Q, typename T>
	template <typename Other> requires unit_vec::has_same_tag<Other, Q>
	constexpr vec3_t<Q, T>::vec3_t(const vec3_t<Other>& other) {
        this->x = other.x;
		this->y = other.y;
		this->z = other.z;
	}

	template <typename Q, typename T>
	template <typename U> requires internal::is_unit<U>
	[[nodiscard]] constexpr auto vec3_t<Q, T>::as() const -> vec3_t<> {
		return vec3_t<>{this->x / U::conversion_factor * Q::conversion_factor,
						this->y / U::conversion_factor * Q::conversion_factor,
						this->z / U::conversion_factor * Q::conversion_factor};
	}
}

export namespace gse {
	template <unit_vec::is_quantity_or_unit Q, typename T = float>
	struct vec3 : unit_vec3::vec3_t<typename unit_vec::get_quantity<Q>::type, T> {
		using value_type = typename unit_vec::get_quantity<Q>::type::value_type;

		template <typename... Args>
		constexpr vec3(Args... args);
    };

	template <typename VectorType, typename ValueType, typename ArgumentType>
		requires std::is_arithmetic_v<ValueType>
	constexpr auto get_value_from_unit(const ArgumentType& value) -> ValueType {
		if constexpr (unit_vec::is_quantity<VectorType>) {
			if constexpr (unit_vec::is_quantity<ArgumentType>) {
                return value.as_default_unit();
			}
			else {
				return value; // Assume default unit
			}
		}
		else if constexpr (internal::is_unit<VectorType>) {
			if constexpr (unit_vec::is_quantity<ArgumentType>) {
				return value.as_default_unit();
			}
			else {
				return value * VectorType::conversion_factor;
			}
		}
		else {
			return value; // Should not reach here
		}
	}

	template <unit_vec::is_quantity_or_unit Q, typename T>
    template <typename ... Args>
	constexpr vec3<Q, T>::vec3(Args... args)
		: unit_vec3::vec3_t<typename unit_vec::get_quantity<Q>::type, T>(get_value_from_unit<Q, value_type>(args)...) {}
}

namespace gse::unit_vec2 {
	/*template <typename Q = unit_vec::unitless, typename T = float>
	struct vec2_t : internal::vec<unit_vec::convertible_type<Q, T>, 2> {
		using value_type = unit_vec::convertible_type<Q, T>;

        template <typename... Args>
            requires ((unit_vec::is_quantity<Args> || std::is_same_v<Args, typename Q::value_type>) && ...)
        constexpr vec2_t(Args... args);

        template <typename Other>
            requires unit_vec::has_same_tag<Other, Q>
        constexpr vec2_t(const vec2_t<Other>& other);

        template <typename U>
            requires internal::is_unit<U>
        [[nodiscard]] constexpr auto as() const->vec2_t<U>;
    };

	template <typename Q, typename T>
    template <typename ... Args> requires ((unit_vec::is_quantity<Args> || std::is_same_v<Args, typename Q::value_type>) && ...)
		constexpr vec2_t<Q, T>::vec2_t(Args... args) : internal::vec<value_type, 2>(get_value(args)...) {}

	template <typename Q, typename T>
    template <typename Other> requires unit_vec::has_same_tag<Other, Q>
    constexpr vec2_t<Q, T>::vec2_t(const vec2_t<Other>& other) {
        this->x = other.x;
        this->y = other.y;
    }

	template <typename Q, typename T>
    template <typename U> requires internal::is_unit<U>
    [[nodiscard]] constexpr auto vec2_t<Q, T>::as() const -> vec2_t<U> {
		return vec2_t<U>{this->x / U::conversion_factor * Q::conversion_factor,
						 this->y / U::conversion_factor * Q::conversion_factor};
    }*/
}
