export module gse.physics.math.base_vec;

import std;

export namespace gse::internal {
    template <typename Derived, typename T, int N>
    struct vec;

    template <typename Derived, typename T, int N>
    struct vec_from_lesser {
        constexpr vec_from_lesser() = default;

        template <typename LesserDerived, int U, typename... Args>
            requires (U <= N)
        constexpr vec_from_lesser(const vec<LesserDerived, T, U>& other, Args... args) {
            for (int i = 0; i < U; ++i) {
                static_cast<Derived*>(this)->storage.data[i] = other[i];
            }
            T extra_values[] = { static_cast<T>(args)... };
            int idx = U;
            for (auto v : extra_values) {
                static_cast<Derived*>(this)->storage.data[idx++] = T(v);
            }
            for (; idx < N; ++idx) {
                static_cast<Derived*>(this)->storage.data[idx] = T(0);
            }
        }
    };

    template <typename Derived, typename T, int N>
    struct vec_base {
        constexpr auto operator[](std::size_t index) -> T& {
            return static_cast<Derived*>(this)->storage.data[index];
        }

        constexpr auto operator[](std::size_t index) const -> const T& {
            return static_cast<const Derived*>(this)->storage.data[index];
        }

        template <typename E> requires std::is_enum_v<E>
        constexpr auto operator[](E index) -> T& {
            return static_cast<Derived*>(this)->storage.data[static_cast<std::size_t>(index)];
        }

        template <typename E> requires std::is_enum_v<E>
        constexpr auto operator[](E index) const -> const T& {
            return static_cast<const Derived*>(this)->storage.data[static_cast<std::size_t>(index)];
        }
    };

    template <typename Derived, typename T, int N = 1>
    struct vec : vec_base<Derived, T, N>, vec_from_lesser<Derived, T, N> {
        using vec_from_lesser<Derived, T, N>::vec_from_lesser;

        T data[N];

        constexpr vec() : data{} {}
        constexpr vec(const T& value) {
            for (int i = 0; i < N; ++i) {
                data[i] = value;
            }
        }
        constexpr vec(const T* values) {
            for (int i = 0; i < N; ++i) {
                data[i] = values[i];
            }
        }
    };

    template <typename T, int N>
    struct alignas(N * sizeof(T)) vec_storage {
        T data[N];
    };

    template <typename Derived, typename T>
    struct vec<Derived, T, 2> : vec_base<Derived, T, 2>, vec_from_lesser<Derived, T, 2> {
        using vec_from_lesser<Derived, T, 2>::vec_from_lesser;

        union {
            vec_storage<T, 2> storage;
            struct { T x, y; };
        };

        constexpr vec() : storage{ { static_cast<T>(0), static_cast<T>(0) } } {}
        constexpr vec(const T& value) : storage{ { value, value } } {}
        constexpr vec(const T* values) : storage{ { values[0], values[1] } } {}
        constexpr vec(const T& x, const T& y) : storage{ { x, y } } {}
    };

    template <typename Derived, typename T>
    struct vec<Derived, T, 3> : vec_base<Derived, T, 3>, vec_from_lesser<Derived, T, 3> {
        using vec_from_lesser<Derived, T, 3>::vec_from_lesser;

        union {
            vec_storage<T, 4> storage;
            struct { T x, y, z, _pad; }; // _pad ensures 16-byte alignment.
        };

        constexpr vec() : storage{ { static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(0) } } {}
        constexpr vec(const T& value) : storage{ { value, value, value, static_cast<T>(0) } } {}
        constexpr vec(const T* values) : storage{ { values[0], values[1], values[2], static_cast<T>(0) } } {}
        constexpr vec(const T& x, const T& y, const T& z) : storage{ { x, y, z, static_cast<T>(0) } } {}
    };

    template <typename Derived, typename T>
    struct vec<Derived, T, 4> : vec_base<Derived, T, 4>, vec_from_lesser<Derived, T, 4> {
        using vec_from_lesser<Derived, T, 4>::vec_from_lesser;

        union {
            vec_storage<T, 4> storage;
            struct { T x, y, z, w; };
        };

        constexpr vec() : storage{ { static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(0) } } {}
        constexpr vec(const T& value) : storage{ { value, value, value, value } } {}
        constexpr vec(const T* values) : storage{ { values[0], values[1], values[2], values[3] } } {}
        constexpr vec(const T& x, const T& y, const T& z, const T& w) : storage{ { x, y, z, w } } {}
    };

}
