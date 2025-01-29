export module gse.physics.math.vec;

import std;

namespace gse::internal {
    template <typename Derived, int N, typename T>
    struct vec_base {
        T data[];

        constexpr auto operator[](std::size_t index) -> T& {
			return data[index];
        }

        constexpr auto operator[](std::size_t index) const -> const T& {
			return data[index];
        }

        constexpr auto operator==(const Derived& other) const -> bool;
        constexpr auto operator!=(const Derived& other) const -> bool;

        constexpr auto operator+(const Derived& other) const -> Derived;
        constexpr auto operator-(const Derived& other) const -> Derived;
        constexpr auto operator*(T scalar) const -> Derived;
        constexpr auto operator/(T scalar) const -> Derived;

        constexpr auto operator+=(const Derived& other) -> Derived&;
        constexpr auto operator-=(const Derived& other) -> Derived&;
        constexpr auto operator*=(T scalar) -> Derived&;
        constexpr auto operator/=(T scalar) -> Derived&;

        constexpr auto operator-() const->Derived;
    };

    template <typename T, int N = 1>
    struct vec : vec_base<vec<T, N>, N, T> {
		T data[N];

		constexpr vec() : data{} {}
		constexpr vec(const T& value) : data(&value) {}
		constexpr vec(const T* values) : data(values) {}
    };

    template <typename T>
    struct vec<T, 2> : vec_base<vec<T, 2>, 2, T> {
		union {
			struct { T x, y; };
			T data[2];
		};

        constexpr vec() : x(static_cast<T>(0)), y(static_cast<T>(0)) {}
        constexpr vec(const T& value) : x(value), y(value) {}
        constexpr vec(const T* values) : x(values[0]), y(values[1]) {}
        constexpr vec(const T& x, const T& y) : x(x), y(y) {}
    };

    template <typename T>
    struct vec<T, 3> : vec_base<vec<T, 3>, 3, T> {
		union {
			struct { T x, y, z; };
			T data[3];
		};

        constexpr vec() : x(static_cast<T>(0)), y(static_cast<T>(0)), z(static_cast<T>(0)) {}
        constexpr vec(const T& value) : x(value), y(value), z(value) {}
        constexpr vec(const T* values) : x(values[0]), y(values[1]), z(values[2]) {}
        constexpr vec(const T& x, const T& y, const T& z) : x(x), y(y), z(z) {}
    };

    template <typename T>
    struct vec<T, 4> : vec_base<vec<T, 4>, 4, T> {
        union {
            struct { T x, y, z, w; };
            T data[4];
        };

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
    template <typename T, int N = 1> using vec_t = internal::vec<T, N>;

	template <typename T> using vec2_t = vec_t<T, 2>;
	template <typename T> using vec3_t = vec_t<T, 3>;
	template <typename T> using vec4_t = vec_t<T, 4>;

	using vec2 = vec2_t<float>;
	using vec3 = vec3_t<float>;
	using vec4 = vec4_t<float>;

	using vec2d = vec2_t<double>;
    using vec3d = vec3_t<double>;
	using vec4d = vec4_t<double>;

	using vec2i = vec2_t<int>;
	using vec3i = vec3_t<int>;
	using vec4i = vec4_t<int>;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator==(const Derived& other) const -> bool {
    auto const& self = *static_cast<const Derived*>(this);
    for (int i = 0; i < N; ++i) {
        if (self.data[i] != other.data[i]) {
            return false;
        }
    }
    return true;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator!=(const Derived& other) const -> bool {
    return !(*this == other);
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator+(const Derived& other) const -> Derived {
    auto const& self = *static_cast<const Derived*>(this);
    Derived result{};
    for (int i = 0; i < N; ++i) {
        result.data[i] = self.data[i] + other.data[i];
    }
    return result;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator-(const Derived& other) const -> Derived {
    auto const& self = *static_cast<const Derived*>(this);
    Derived result{};
    for (int i = 0; i < N; ++i) {
        result.data[i] = self.data[i] - other.data[i];
    }
    return result;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator*(const T scalar) const -> Derived {
    auto const& self = *static_cast<const Derived*>(this);
    Derived result{};
    for (int i = 0; i < N; ++i) {
        result.data[i] = self.data[i] * scalar;
    }
    return result;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator/(const T scalar) const -> Derived {
    auto const& self = *static_cast<const Derived*>(this);
    Derived result{};
    for (int i = 0; i < N; ++i) {
        result.data[i] = self.data[i] / scalar;
    }
    return result;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator+=(const Derived& other) -> Derived& {
    auto& self = *static_cast<Derived*>(this);
    for (int i = 0; i < N; ++i) {
        self.data[i] += other.data[i];
    }
    return self;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator-=(const Derived& other) -> Derived& {
    auto& self = *static_cast<Derived*>(this);
    for (int i = 0; i < N; ++i) {
        self.data[i] -= other.data[i];
    }
    return self;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator*=(const T scalar) -> Derived& {
    auto& self = *static_cast<Derived*>(this);
    for (int i = 0; i < N; ++i) {
        self.data[i] *= scalar;
    }
    return self;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator/=(const T scalar) -> Derived& {
    auto& self = *static_cast<Derived*>(this);
    for (int i = 0; i < N; ++i) {
        self.data[i] /= scalar;
    }
    return self;
}

template <typename Derived, int N, typename T>
constexpr auto gse::internal::vec_base<Derived, N, T>::operator-() const -> Derived {
    auto const& self = *static_cast<const Derived*>(this);
    Derived result{};
    for (int i = 0; i < N; ++i) {
        result.data[i] = -self.data[i];
    }
    return result;
}
