export module gse.physics.math:base_vec;

import std;

import :simd;

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

        constexpr auto operator<=>(const storage&) const = default;
    };
}

export namespace gse::internal {
    template <typename T, int N>
    constexpr auto from_value(const T& value) -> vec::storage<T, N> {
        const auto make_storage = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            return vec::storage<T, N>{ { (void(Is), value)... } };
        };

        return make_storage(std::make_index_sequence<N>{});
    }

    template <typename T, int N>
    constexpr auto from_pointer(const T* values) -> vec::storage<T, N> {
        const auto make_storage = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            return vec::storage<T, N>{ { values[Is]... } };
        };

        return make_storage(std::make_index_sequence<N>{});
    }

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
    struct vec_t_common : vec_from_lesser<Derived, T, N> {
        using vec_from_lesser<Derived, T, N>::vec_from_lesser;

        constexpr vec_t_common() = default;

        constexpr vec_t_common(const vec::storage<T, N>& s) {
            static_cast<Derived*>(this)->storage = s;
        }

        constexpr vec_t_common(const T& value) {
            static_cast<Derived*>(this)->storage = from_value<T, N>(value);
        }

        constexpr vec_t_common(const T* values) {
            static_cast<Derived*>(this)->storage = from_pointer<T, N>(values);
        }

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

    template <typename Derived, typename T, int N>
    struct vec_t : vec_t_common<Derived, T, N> {
        using vec_t_common<Derived, T, N>::vec_t_common;

        union {
            vec::storage<T, N> storage;
        };

        constexpr vec_t() : storage{} {}

        constexpr auto operator<=>(const vec_t& other) const {
            return this->storage <=> other.storage;
        }
    };

    template <typename Derived, typename T>
    struct vec_t<Derived, T, 2> : vec_t_common<Derived, T, 2> {
        using vec_t_common<Derived, T, 2>::vec_t_common;

        union {
            vec::storage<T, 2> storage;
            struct { T x, y; };
        };

        constexpr vec_t() : storage{} {}
        constexpr vec_t(T x, T y) : x(x), y(y) {}
    };

    template <typename Derived, typename T>
    struct vec_t<Derived, T, 3> : vec_t_common<Derived, T, 3> {
        using vec_t_common<Derived, T, 3>::vec_t_common;

        union {
            vec::storage<T, 3> storage;
            struct { T x, y, z; };
        };

        constexpr vec_t(T x, T y, T z) : x(x), y(y), z(z) {}
        constexpr vec_t() : storage{} {}
    };

    template <typename Derived, typename T>
    struct vec_t<Derived, T, 4> : vec_t_common<Derived, T, 4> {
        using vec_t_common<Derived, T, 4>::vec_t_common;

        union {
            vec::storage<T, 4> storage;
            struct { T x, y, z, w; };
        };

        constexpr vec_t(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
        constexpr vec_t() : storage{} {}
    };
}

export namespace gse {
    template <typename T, int N> constexpr auto value_ptr(const vec::storage<T, N>& storage) -> const T*;
}

template <typename T, int N>
constexpr auto gse::value_ptr(const vec::storage<T, N>& storage) -> const T* {
    return &storage[0];
}

export template <typename T, int N, typename CharT>
struct std::formatter<gse::vec::storage<T, N>, CharT> {
    std::formatter<T, CharT> element_formatter;

    constexpr auto parse(std::format_parse_context& ctx) {
        return element_formatter.parse(ctx);
    }

    template <typename FormatContext>
    auto format(const gse::vec::storage<T, N>& v, FormatContext& ctx) const {
        auto out = ctx.out();
        out = std::format_to(out, "(");
        for (int i = 0; i < N; ++i) {
            if (i > 0) out = std::format_to(out, ", ");
            out = element_formatter.format(v[i], ctx);
        }
        return std::format_to(out, ")");
    }
};

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
constexpr auto gse::vec::dot(const storage<T, N>& lhs, const storage<T, N>& rhs) -> T {
	T result{};
	simd::dot(std::span<const T, N>(lhs.data), std::span<const T, N>(rhs.data), result);
	return result;
}





