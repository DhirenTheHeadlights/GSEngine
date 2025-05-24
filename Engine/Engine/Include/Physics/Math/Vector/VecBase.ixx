module;

#include <intrin.h> 

export module gse.physics.math.base_vec;

import std;

import gse.physics.math.simd;

export namespace gse::vec {
    template <typename T, int N>
    struct storage {
        std::array<T, N> data = {};

        constexpr auto operator[](std::size_t index) -> T& {
            return data[index];
        }

        constexpr auto operator[](std::size_t index) const -> const T& {
            return data[index];
        }
    };

    using raw2i = storage<int, 2>;
    using raw3i = storage<int, 3>;
    using raw4i = storage<int, 4>;

    using raw2f = storage<float, 2>;
    using raw3f = storage<float, 3>;
    using raw4f = storage<float, 4>;

    using raw2d = storage<double, 2>;
    using raw3d = storage<double, 3>;
    using raw4d = storage<double, 4>;
}

export namespace gse::internal {
    template <typename Derived, typename T, int N>
    struct vec_t;

    template <typename Derived, typename T, int N>
    struct vec_from_lesser {
        constexpr vec_from_lesser() = default;

        template <typename LesserDerived, int U, typename... Args>
            requires (U <= N)
        constexpr vec_from_lesser(const vec_t<LesserDerived, T, U>& other, Args... args) {
            for (int i = 0; i < U; ++i) {
                static_cast<Derived*>(this)->storage[i] = other[i];
            }
            T extra_values[] = { static_cast<T>(args)... };
            int idx = U;
            for (auto v : extra_values) {
                static_cast<Derived*>(this)->storage[idx++] = T(v);
            }
            for (; idx < N; ++idx) {
                static_cast<Derived*>(this)->storage[idx] = T(0);
            }
        }
    };

    template <typename Derived, typename T, int N>
    struct vec_base {
        constexpr auto operator[](std::size_t index) -> T& {
            return static_cast<Derived*>(this)->storage[index];
        }

        constexpr auto operator[](std::size_t index) const -> const T& {
            return static_cast<const Derived*>(this)->storage[index];
        }

        template <typename E> requires std::is_enum_v<E>
        constexpr auto operator[](E index) -> T& {
            return static_cast<Derived*>(this)->storage[static_cast<std::size_t>(index)];
        }

        template <typename E> requires std::is_enum_v<E>
        constexpr auto operator[](E index) const -> const T& {
            return static_cast<const Derived*>(this)->storage[static_cast<std::size_t>(index)];
        }

        constexpr auto data() -> auto& {
            return static_cast<Derived*>(this)->storage;
        }

        constexpr auto data() const -> const auto& {
            return static_cast<const Derived*>(this)->storage;
        }
    };

    template <typename Derived, typename T, int N = 1>
    struct vec_t : vec_base<Derived, T, N>, vec_from_lesser<Derived, T, N> {
        using vec_from_lesser<Derived, T, N>::vec_from_lesser;

        vec::storage<T, N> storage;

        constexpr vec_t() : storage{} {}
        constexpr vec_t(const vec::storage<T, N>& storage) : storage(storage) {}
        constexpr vec_t(const T& value) {
            for (int i = 0; i < N; ++i) {
                storage[i] = value;
            }
        }
        constexpr vec_t(const T* values) {
            for (int i = 0; i < N; ++i) {
                storage[i] = values[i];
            }
        }
    };

    template <typename Derived, typename T>
    struct vec_t<Derived, T, 2> : vec_base<Derived, T, 2>, vec_from_lesser<Derived, T, 2> {
        using vec_from_lesser<Derived, T, 2>::vec_from_lesser;

        union {
            vec::storage<T, 2> storage;
            struct { T x, y; };
        };

        constexpr vec_t() : storage{ { static_cast<T>(0), static_cast<T>(0) } } {}
        constexpr vec_t(const vec::storage<T, 2>& storage) : storage(storage) {}
        constexpr vec_t(const T& value) : storage{ { value, value } } {}
        constexpr vec_t(const T* values) : storage{ { values[0], values[1] } } {}
        constexpr vec_t(const T& x, const T& y) : storage{ { x, y } } {}
    };

    template <typename Derived, typename T>
    struct vec_t<Derived, T, 3> : vec_base<Derived, T, 3>, vec_from_lesser<Derived, T, 3> {
        using vec_from_lesser<Derived, T, 3>::vec_from_lesser;

        union {
            vec::storage<T, 3> storage;
            struct { T x, y, z; };
        };

        constexpr vec_t() : storage{ { static_cast<T>(0), static_cast<T>(0), static_cast<T>(0) } } {}
        constexpr vec_t(const vec::storage<T, 3>& storage) : storage{ storage[0], storage[1], storage[2] } {}
        constexpr vec_t(const T& value) : storage{ { value, value, value } } {}
        constexpr vec_t(const T* values) : storage{ { values[0], values[1], values[2] } } {}
        constexpr vec_t(const T& x, const T& y, const T& z) : storage{ { x, y, z } } {}
    };

    template <typename Derived, typename T>
    struct vec_t<Derived, T, 4> : vec_base<Derived, T, 4>, vec_from_lesser<Derived, T, 4> {
        using vec_from_lesser<Derived, T, 4>::vec_from_lesser;

        union {
            vec::storage<T, 4> storage;
            struct { T x, y, z, w; };
        };

        constexpr vec_t() : storage{ { static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(0) } } {}
        constexpr vec_t(const vec::storage<T, 4>& storage) : storage(storage) {}
        constexpr vec_t(const T& value) : storage{ { value, value, value, value } } {}
        constexpr vec_t(const T* values) : storage{ { values[0], values[1], values[2], values[3] } } {}
        constexpr vec_t(const T& x, const T& y, const T& z, const T& w) : storage{ { x, y, z, w } } {}
    };
}

export namespace gse {
    template <typename T, int N> constexpr auto value_ptr(const vec::storage<T, N>& storage) -> const T*;
}

template <typename T, int N>
constexpr auto gse::value_ptr(const vec::storage<T, N>& storage) -> const T* {
    return &storage[0];
}

template <typename T, int N>
concept ref_t = sizeof(T) * N > 16;

template <typename T, int N>
concept val_t = !ref_t<T, N>;

export namespace gse::vec {
    template <typename T, int N> requires ref_t<T, N> constexpr auto operator+(const storage<T, N>& lhs, const storage<T, N>& rhs) -> storage<T, N>;
    template <typename T, int N> requires ref_t<T, N> constexpr auto operator-(const storage<T, N>& lhs, const storage<T, N>& rhs) -> storage<T, N>;
    template <typename T, int N> requires ref_t<T, N> constexpr auto operator*(const storage<T, N>& lhs, const storage<T, N>& rhs) -> storage<T, N>;
	template <typename T, int N> requires ref_t<T, N> constexpr auto operator/(const storage<T, N>& lhs, const storage<T, N>& rhs) -> storage<T, N>;

    template <typename T, int N> requires val_t<T, N> constexpr auto operator+(storage<T, N> lhs, storage<T, N> rhs) -> storage<T, N>;
    template <typename T, int N> requires val_t<T, N> constexpr auto operator-(storage<T, N> lhs, storage<T, N> rhs) -> storage<T, N>;
    template <typename T, int N> requires val_t<T, N> constexpr auto operator*(storage<T, N> lhs, storage<T, N> rhs) -> storage<T, N>;
    template <typename T, int N> requires val_t<T, N> constexpr auto operator/(storage<T, N> lhs, storage<T, N> rhs) -> storage<T, N>;

    template <typename T, int N> requires ref_t<T, N> constexpr auto operator+=(storage<T, N>& lhs, const storage<T, N>& rhs) -> storage<T, N>&;
    template <typename T, int N> requires ref_t<T, N> constexpr auto operator-=(storage<T, N>& lhs, const storage<T, N>& rhs) -> storage<T, N>&;
    template <typename T, int N> requires ref_t<T, N> constexpr auto operator*=(storage<T, N>& lhs, const storage<T, N>& rhs) -> storage<T, N>&;
    template <typename T, int N> requires ref_t<T, N> constexpr auto operator/=(storage<T, N>& lhs, const storage<T, N>& rhs) -> storage<T, N>&;

	template <typename T, int N> requires val_t<T, N> constexpr auto operator+=(storage<T, N>& lhs, storage<T, N> rhs) -> storage<T, N>&;
	template <typename T, int N> requires val_t<T, N> constexpr auto operator-=(storage<T, N>& lhs, storage<T, N> rhs) -> storage<T, N>&;
	template <typename T, int N> requires val_t<T, N> constexpr auto operator*=(storage<T, N>& lhs, storage<T, N> rhs) -> storage<T, N>&;
	template <typename T, int N> requires val_t<T, N> constexpr auto operator/=(storage<T, N>& lhs, storage<T, N> rhs) -> storage<T, N>&;

    template <typename T, int N> requires ref_t<T, N> constexpr auto operator*(const storage<T, N>& lhs, T rhs) -> storage<T, N>;
    template <typename T, int N> requires ref_t<T, N> constexpr auto operator*(T lhs, const storage<T, N>& rhs) -> storage<T, N>;
    template <typename T, int N> requires ref_t<T, N> constexpr auto operator/(const storage<T, N>& lhs, T rhs) -> storage<T, N>;
    template <typename T, int N> requires ref_t<T, N> constexpr auto operator/(T lhs, const storage<T, N>& rhs) -> storage<T, N>;

	template <typename T, int N> requires val_t<T, N> constexpr auto operator*(storage<T, N> lhs, T rhs) -> storage<T, N>;
	template <typename T, int N> requires val_t<T, N> constexpr auto operator*(T lhs, storage<T, N> rhs) -> storage<T, N>;
	template <typename T, int N> requires val_t<T, N> constexpr auto operator/(storage<T, N> lhs, T rhs) -> storage<T, N>;
	template <typename T, int N> requires val_t<T, N> constexpr auto operator/(T lhs, storage<T, N> rhs) -> storage<T, N>;

    template <typename T, int N> constexpr auto operator*=(storage<T, N>& lhs, T rhs) -> storage<T, N>&;
    template <typename T, int N> constexpr auto operator/=(storage<T, N>& lhs, T rhs) -> storage<T, N>&;

    template <typename T, int N> constexpr auto operator+(const storage<T, N>& v) -> storage<T, N>;
    template <typename T, int N> constexpr auto operator-(const storage<T, N>& v) -> storage<T, N>;

    template <typename T, int N> constexpr auto operator==(const storage<T, N>& lhs, const storage<T, N>& rhs) -> bool;
    template <typename T, int N> constexpr auto operator!=(const storage<T, N>& lhs, const storage<T, N>& rhs) -> bool;

    template <typename T, int N> constexpr auto operator>(const storage<T, N>& lhs, const storage<T, N>& rhs) -> bool;
    template <typename T, int N> constexpr auto operator>=(const storage<T, N>& lhs, const storage<T, N>& rhs) -> bool;
    template <typename T, int N> constexpr auto operator<(const storage<T, N>& lhs, const storage<T, N>& rhs) -> bool;
    template <typename T, int N> constexpr auto operator<=(const storage<T, N>& lhs, const storage<T, N>& rhs) -> bool;

	template <typename T, int N> constexpr auto dot(const storage<T, N>& lhs, const storage<T, N>& rhs) -> T;

}

template <typename T, int N>  requires ref_t<T, N>
constexpr auto gse::vec::operator+(const storage<T, N>& lhs, const storage<T, N>& rhs) -> storage<T, N> {
    storage<T, N> result{};

	simd::add(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(result.data));

    return result;
}

template <typename T, int N> requires ref_t<T, N>
constexpr auto gse::vec::operator-(const storage<T, N>& lhs, const storage<T, N>& rhs) -> storage<T, N> {
    storage<T, N> result{};

    simd::sub(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(result.data));

    return result;
}

template <typename T, int N> requires ref_t<T, N>
constexpr auto gse::vec::operator*(const storage<T, N>& lhs, const storage<T, N>& rhs) -> storage<T, N> {
    storage<T, N> result{};

	simd::mul(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(result.data));

    return result;
}

template <typename T, int N> requires ref_t<T, N>
constexpr auto gse::vec::operator/(const storage<T, N>& lhs, const storage<T, N>& rhs) -> storage<T, N> {
    storage<T, N> result{};

	simd::div(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(result.data));

    return result;
}

template <typename T, int N> requires val_t<T, N>
constexpr auto gse::vec::operator+(storage<T, N> lhs, storage<T, N> rhs) -> storage<T, N> {
	storage<T, N> result{};

	simd::add(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(result.data));

	return result;
}

template <typename T, int N> requires val_t<T, N>
constexpr auto gse::vec::operator-(storage<T, N> lhs, storage<T, N> rhs) -> storage<T, N> {
	storage<T, N> result{};

	simd::sub(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(result.data));

	return result;
}

template <typename T, int N> requires val_t<T, N>
constexpr auto gse::vec::operator*(storage<T, N> lhs, storage<T, N> rhs) -> storage<T, N> {
	storage<T, N> result{};

	simd::mul(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(result.data));

	return result;
}

template <typename T, int N> requires val_t<T, N>
constexpr auto gse::vec::operator/(storage<T, N> lhs, storage<T, N> rhs) -> storage<T, N> {
	storage<T, N> result{};

	simd::div(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(result.data));

	return result;
}

template <typename T, int N> requires ref_t<T, N>
constexpr auto gse::vec::operator+=(storage<T, N>& lhs, const storage<T, N>& rhs) -> storage<T, N>& {
	simd::add(std::span<T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(lhs.data));
    return lhs;
}

template <typename T, int N> requires ref_t<T, N>
constexpr auto gse::vec::operator-=(storage<T, N>& lhs, const storage<T, N>& rhs) -> storage<T, N>& {
	simd::sub(std::span<T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(lhs.data));
    return lhs;
}

template <typename T, int N> requires ref_t<T, N>
constexpr auto gse::vec::operator*=(storage<T, N>& lhs, const storage<T, N>& rhs) -> storage<T, N>& {
	simd::mul(std::span<T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(lhs.data));
    return lhs;
}

template <typename T, int N> requires ref_t<T, N>
constexpr auto gse::vec::operator/=(storage<T, N>& lhs, const storage<T, N>& rhs) -> storage<T, N>& {
	simd::div(std::span<T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(lhs.data));
    return lhs;
}

template <typename T, int N> requires val_t<T, N>
constexpr auto gse::vec::operator+=(storage<T, N>& lhs, storage<T, N> rhs) -> storage<T, N>& {
	simd::add(std::span<T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(lhs.data));
	return lhs;
}

template <typename T, int N> requires val_t<T, N>
constexpr auto gse::vec::operator-=(storage<T, N>& lhs, storage<T, N> rhs) -> storage<T, N>& {
	simd::sub(std::span<T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(lhs.data));
	return lhs;
}

template <typename T, int N> requires val_t<T, N>
constexpr auto gse::vec::operator*=(storage<T, N>& lhs, storage<T, N> rhs) -> storage<T, N>& {
	simd::mul(std::span<T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(lhs.data));
	return lhs;
}

template <typename T, int N> requires val_t<T, N>
constexpr auto gse::vec::operator/=(storage<T, N>& lhs, storage<T, N> rhs) -> storage<T, N>& {
	simd::div(std::span<T, N>(lhs.data), std::span<const T, N>(rhs.data), std::span<T, N>(lhs.data));
	return lhs;
}

template <typename T, int N> requires ref_t<T, N>
constexpr auto gse::vec::operator*(const storage<T, N>& lhs, T rhs) -> storage<T, N> {
    storage<T, N> result{};

	simd::mul_s(std::span<const T, N>(lhs.data), rhs, std::span<T, N>(result.data));

    return result;
}

template <typename T, int N> requires ref_t<T, N>
constexpr auto gse::vec::operator*(T lhs, const storage<T, N>& rhs) -> storage<T, N> {
    storage<T, N> result{};

	simd::mul_s(std::span<const T, N>(rhs.data), lhs, std::span<T, N>(result.data));

    return result;
}

template <typename T, int N> requires ref_t<T, N>
constexpr auto gse::vec::operator/(const storage<T, N>& lhs, T rhs) -> storage<T, N> {
    storage<T, N> result{};

	simd::div_s(std::span<const T, N>(lhs.data), rhs, std::span<T, N>(result.data));

    return result;
}

template <typename T, int N> requires ref_t<T, N>
constexpr auto gse::vec::operator/(T lhs, const storage<T, N>& rhs) -> storage<T, N> {
    storage<T, N> result{};

	simd::div_s(std::span<const T, N>(rhs.data), lhs, std::span<T, N>(result.data));

    return result;
}

template <typename T, int N> requires val_t<T, N>
constexpr auto gse::vec::operator*(storage<T, N> lhs, T rhs) -> storage<T, N> {
    storage<T, N> result{};
    simd::mul_s(std::span<const T, N>(lhs.data), rhs, std::span<T, N>(result.data));
    return result;
}

template <typename T, int N> requires val_t<T, N>
constexpr auto gse::vec::operator*(T lhs, storage<T, N> rhs) -> storage<T, N> {
	storage<T, N> result{};
	simd::mul_s(std::span<const T, N>(rhs.data), lhs, std::span<T, N>(result.data));
	return result;
}

template <typename T, int N> requires val_t<T, N>
constexpr auto gse::vec::operator/(storage<T, N> lhs, T rhs) -> storage<T, N> {
	storage<T, N> result{};
	simd::div_s(std::span<const T, N>(lhs.data), rhs, std::span<T, N>(result.data));
	return result;
}

template <typename T, int N> requires val_t<T, N>
constexpr auto gse::vec::operator/(T lhs, storage<T, N> rhs) -> storage<T, N> {
    storage<T, N> result{};
    simd::div_s(std::span<const T, N>(rhs.data), lhs, std::span<T, N>(result.data));
    return result;
}

template <typename T, int N>
constexpr auto gse::vec::operator*=(storage<T, N>& lhs, T rhs) -> storage<T, N>& {
	simd::mul_s(std::span<T, N>(lhs.data), rhs, std::span<T, N>(lhs.data));
    return lhs;
}

template <typename T, int N>
constexpr auto gse::vec::operator/=(storage<T, N>& lhs, T rhs) -> storage<T, N>& {
	simd::div_s(std::span<T, N>(lhs.data), rhs, std::span<T, N>(lhs.data));
    return lhs;
}

template <typename T, int N>
constexpr auto gse::vec::operator+(const storage<T, N>& v) -> storage<T, N> {
    return v;
}

template <typename T, int N>
constexpr auto gse::vec::operator-(const storage<T, N>& v) -> storage<T, N> {
    storage<T, N> result{};
	simd::mul_s(std::span<const T, N>(v.data), static_cast<T>(-1), std::span<T, N>(result.data));
    return result;
}

template <typename T, int N>
constexpr auto gse::vec::operator==(const storage<T, N>& lhs, const storage<T, N>& rhs) -> bool {
    for (int i = 0; i < N; ++i)
        if (!(lhs[i] == rhs[i]))
            return false;
    return true;
}

template <typename T, int N>
constexpr auto gse::vec::operator!=(const storage<T, N>& lhs, const storage<T, N>& rhs) -> bool {
    return !(lhs == rhs);
}

template <typename T, int N>
constexpr auto gse::vec::operator>(const storage<T, N>& lhs, const storage<T, N>& rhs) -> bool {
    for (int i = 0; i < N; ++i)
        if (!(lhs[i] > rhs[i]))
            return false;
    return true;
}

template <typename T, int N>
constexpr auto gse::vec::operator>=(const storage<T, N>& lhs, const storage<T, N>& rhs) -> bool {
    for (int i = 0; i < N; ++i)
        if (!(lhs[i] >= rhs[i]))
            return false;
    return true;
}

template <typename T, int N>
constexpr auto gse::vec::operator<(const storage<T, N>& lhs, const storage<T, N>& rhs) -> bool {
    for (int i = 0; i < N; ++i)
        if (!(lhs[i] < rhs[i]))
            return false;
    return true;
}

template <typename T, int N>
constexpr auto gse::vec::operator<=(const storage<T, N>& lhs, const storage<T, N>& rhs) -> bool {
    for (int i = 0; i < N; ++i)
        if (!(lhs[i] <= rhs[i]))
            return false;
    return true;
}

template <typename T, int N>
constexpr auto gse::vec::dot(const storage<T, N>& lhs, const storage<T, N>& rhs) -> T {
	T result{};
	simd::dot(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), result);
	return result;
}





