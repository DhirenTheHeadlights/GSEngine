module;
//SIMD Support
#include <intrin.h> 

export module gse.physics.math.base_vec;

import std;

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

        constexpr vec_t() : storage{ { static_cast<T>(0), static_cast<T>(0), static_cast<T>(0) }} {}
		constexpr vec_t(const vec::storage<T, 3>& storage) : storage{ storage[0], storage[1], storage[2] } {}
        constexpr vec_t(const T& value) : storage{ { value, value, value }} {}
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

export namespace gse::internal {
    auto check_cpu_supports_avx() -> bool;
    auto check_cpu_supports_sse() -> bool;
    auto check_cpu_supports_sse2() -> bool;
    auto check_cpu_supports_avx2() -> bool;

    bool sse_supported = check_cpu_supports_sse();
    bool sse2_supported = check_cpu_supports_sse2();
    bool avx_supported = check_cpu_supports_avx();
    bool avx2_supported = check_cpu_supports_avx2();

    template <typename T, typename U, typename V, int N> auto add(const vec::storage<T, N>& lhs, const vec::storage<U, N>& rhs, vec::storage<V, N>& result) -> bool;
    template <typename T, typename U, typename V, int N> auto subtract(const vec::storage<T, N>& lhs, const vec::storage<U, N>& rhs, vec::storage<V, N>& result) -> bool;
    template <typename T, typename U, typename V, int N> auto multiply(const vec::storage<T, N>& lhs, const vec::storage<U, N>& rhs, vec::storage<V, N>& result) -> bool;
    template <typename T, typename U, typename V, int N> auto divide(const vec::storage<T, N>& lhs, const vec::storage<U, N>& rhs, vec::storage<V, N>& result) -> bool;

    template <typename T, typename U, typename V, int N> auto add(const T& lhs, const vec::storage<U, N>& rhs, vec::storage<V, N>& result) -> bool;
    template <typename T, typename U, typename V, int N> auto add(const vec::storage<U, N>& lhs, const T& rhs, vec::storage<V, N>& result) -> bool;
    template <typename T, typename U, typename V, int N> auto subtract(const T& lhs, const vec::storage<U, N>& rhs, vec::storage<V, N>& result) -> bool;
    template <typename T, typename U, typename V, int N> auto subtract(const vec::storage<T, N>& lhs, const U& rhs, vec::storage<V, N>& result) -> bool;
    template <typename T, typename U, typename V, int N> auto multiply(const T& lhs, const vec::storage<U, N>& rhs, vec::storage<V, N>& result) -> bool;
    template <typename T, typename U, typename V, int N> auto multiply(const vec::storage<T, N>& lhs, const U& rhs, vec::storage<V, N>& result) -> bool;
    template <typename T, typename U, typename V, int N> auto divide(const T& lhs, const vec::storage<U, N>& rhs, vec::storage<V, N>& result) -> bool;
    template <typename T, typename U, typename V, int N> auto divide(const vec::storage<T, N>& lhs, const U& rhs, vec::storage<V, N>& result) -> bool;

    template <typename T, typename U, typename V, int N> auto dot(const vec::storage<T, N>& lhs, const vec::storage<U, N>& rhs, V& result) -> bool;

};  

auto gse::internal::check_cpu_supports_sse() -> bool {
    int info[4];
    __cpuid(info, 1);
    return (info[3] & (1 << 25)) != 0; // SSE bit in EDX
}

auto gse::internal::check_cpu_supports_sse2() -> bool {
    int info[4];
    __cpuid(info, 1);
    return (info[3] & (1 << 26)) != 0; // SSE2 bit in EDX
}

auto gse::internal::check_cpu_supports_avx() -> bool {
    int info[4];
    __cpuid(info, 1);

    const bool os_uses_xsave = (info[2] & (1 << 27)) != 0;
    const bool cpu_supports_avx = (info[2] & (1 << 28)) != 0;

    if (os_uses_xsave && cpu_supports_avx) {
    	const std::uint64_t xcr_mask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
        return (xcr_mask & 0x6) == 0x6; // XMM (bit 1) and YMM (bit 2)
    }

    return false;
}

auto gse::internal::check_cpu_supports_avx2() -> bool {
    int info[4];

    // First check AVX and OS support
    __cpuid(info, 1);
    const bool os_uses_xsave = (info[2] & (1 << 27)) != 0;
    const bool cpu_supports_avx = (info[2] & (1 << 28)) != 0;

    if (!(os_uses_xsave && cpu_supports_avx)) {
        return false;
    }

    const std::uint64_t xcr_mask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
    const bool xmm_enabled = (xcr_mask & 0x2) != 0;
    const bool ymm_enabled = (xcr_mask & 0x4) != 0;

    if (!(xmm_enabled && ymm_enabled)) {
        return false;
    }

    // Then check AVX2 bit in CPUID.7:EBX
    __cpuidex(info, 7, 0);
    return (info[1] & (1 << 5)) != 0; // AVX2 bit in EBX
}

template <typename T, typename U, typename V, int N>
auto gse::internal::add(const vec::storage<T, N>& lhs, const vec::storage<U, N>& rhs, vec::storage<V, N>& result) -> bool {
	if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        // AVX2: 256-bit SIMD with integer operation
        if (avx2_supported && N == 8) {
        	const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data.data()));
            const __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.data.data()));
            const __m256i c = _mm256_add_epi32(a, b);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result.data.data()), c);
            return true;
        }
        // SSE2: 128-bit SIMD with integer operation
        if (sse2_supported && N == 4) {
        	const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data.data()));
	        const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs.data.data()));
	        const __m128i c = _mm_add_epi32(a, b);
	        _mm_storeu_si128(reinterpret_cast<__m128i*>(result.data.data()), c);
	        return true;
        }
	}
    else if constexpr (std::is_same_v<T, float> && std::is_same_v<U, float>) {
        // AVX: 256-bit SIMD with float operation
        if (avx_supported && N == 8) {
        	const __m256 a = _mm256_loadu_ps(lhs.data.data());
            const __m256 b = _mm256_loadu_ps(rhs.data.data());
            const __m256 c = _mm256_add_ps(a, b);
            _mm256_storeu_ps(result.data.data(), c);
            return true;
        }
        // SSE: 128-bit SIMD with float operation
        if (sse_supported && N == 4) {
        	const __m128 a = _mm_loadu_ps(lhs.data.data());
	        const __m128 b = _mm_loadu_ps(rhs.data.data());
	        const __m128 c = _mm_add_ps(a, b);
	        _mm_storeu_ps(result.data.data(), c);
	        return true;
        }
	}

    return false;
}

template <typename T, typename U, typename V, int N>
auto gse::internal::subtract(const vec::storage<T, N>& lhs, const vec::storage<U, N>& rhs, vec::storage<V, N>& result) -> bool {
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        // AVX2: 256-bit SIMD with integer operation
        if (avx2_supported && N == 8) {
            __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data.data()));
            __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.data.data()));
            __m256i c = _mm256_sub_epi32(a, b);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result.data.data()), c);
            return true;
        }
        // SSE2: 128-bit SIMD with integer operation
        if (sse2_supported && N == 4) {
	        __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data.data()));
	        __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs.data.data()));
	        __m128i c = _mm_sub_epi32(a, b);
	        _mm_storeu_si128(reinterpret_cast<__m128i*>(result.data.data()), c);
	        return true;
        }
    }
    else if constexpr (std::is_same_v<T, float> && std::is_same_v<U, float>) {
        // AVX: 256-bit SIMD with float operation
        if (avx_supported && N == 8) {
            __m256 a = _mm256_loadu_ps(lhs.data.data());
            __m256 b = _mm256_loadu_ps(rhs.data.data());
            __m256 c = _mm256_sub_ps(a, b);
            _mm256_storeu_ps(result.data.data(), c);
            return true;
        }
        // SSE: 128-bit SIMD with float operation
        if (sse_supported && N == 4) {
	        __m128 a = _mm_loadu_ps(lhs.data.data());
	        __m128 b = _mm_loadu_ps(rhs.data.data());
	        __m128 c = _mm_sub_ps(a, b);
	        _mm_storeu_ps(result.data.data(), c);
	        return true;
        }
    }
    else if constexpr (std::is_same_v<T, double> && std::is_same_v<U, float>) {
		// AVX: 256-bit SIMD with double operation
		if (avx_supported && N == 4) {
			__m256d a = _mm256_loadu_pd(lhs.data.data());
			__m256d b = _mm256_loadu_pd(rhs.data.data());
			__m256d c = _mm256_sub_pd(a, b);
			_mm256_storeu_pd(result.data.data(), c);
			return true;
		}
		// SSE: 128-bit SIMD with double operation
		if (sse_supported && N == 2) {
			__m128d a = _mm_loadu_pd(lhs.data.data());
			__m128d b = _mm_loadu_pd(rhs.data.data());
			__m128d c = _mm_sub_pd(a, b);
			_mm_storeu_pd(result.data.data(), c);
			return true;
		}
    }

    return false;
}

template <typename T, typename U, typename V, int N>
auto gse::internal::multiply(const vec::storage<T, N>& lhs, const vec::storage<U, N>& rhs, vec::storage<V, N>& result) -> bool {
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        // AVX2: 256-bit SIMD with integer operation
        if (avx2_supported && N == 8) {
        	const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data.data()));
            const __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.data.data()));
            const __m256i c = _mm256_mul_epi32(a, b);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result.data.data()), c);
            return true;
        }
        // SSE2: 128-bit SIMD with integer operation
        if (sse2_supported && N == 4) {
        	const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data.data()));
	        const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs.data.data()));
	        const __m128i c = _mm_mul_epi32(a, b);
	        _mm_storeu_si128(reinterpret_cast<__m128i*>(result.data.data()), c);
	        return true;
        }
    }
    else if constexpr (std::is_same_v<T, float> && std::is_same_v<U, float>) {
        // AVX: 256-bit SIMD with float operation
        if (avx_supported && N == 8) {
        	const __m256 a = _mm256_loadu_ps(lhs.data.data());
            const __m256 b = _mm256_loadu_ps(rhs.data.data());
            const __m256 c = _mm256_mul_ps(a, b);
            _mm256_storeu_ps(result.data.data(), c);
            return true;
        }
        // SSE: 128-bit SIMD with float operation
        if (sse_supported && N == 4) {
        	const __m128 a = _mm_loadu_ps(lhs.data.data());
	        const __m128 b = _mm_loadu_ps(rhs.data.data());
	        const __m128 c = _mm_mul_ps(a, b);
	        _mm_storeu_ps(result.data.data(), c);
	        return true;
        }
    }
    return false;
}

template <typename T, typename U, typename V, int N>
auto gse::internal::divide(const vec::storage<T, N>& lhs, const vec::storage<U, N>& rhs, vec::storage<V, N>& result) -> bool {
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        // AVX2: 256-bit SIMD with integer operation
        if (avx2_supported && N == 8) {
        	const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data.data()));
            const __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.data.data()));
            const __m256i c = _mm256_div_epi32(a, b);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result.data.data()), c);
            return true;
        }
        // SSE2: 128-bit SIMD with integer operation
        if (sse2_supported && N == 4) {
        	const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data.data()));
	        const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs.data.data()));
	        const __m128i c = _mm_div_epi32(a, b);
	        _mm_storeu_si128(reinterpret_cast<__m128i*>(result.data.data()), c);
	        return true;
        }
    }
    else if constexpr (std::is_same_v<T, float> && std::is_same_v<U, float>) {
        // AVX: 256-bit SIMD with float operation
        if (avx_supported && N == 8) {
        	const __m256 a = _mm256_loadu_ps(lhs.data.data());
            const __m256 b = _mm256_loadu_ps(rhs.data.data());
            const __m256 c = _mm256_div_ps(a, b);
            _mm256_storeu_ps(result.data.data(), c);
            return true;
        }
        // SSE: 128-bit SIMD with float operation
        if (sse_supported && N == 4) {
        	const __m128 a = _mm_loadu_ps(lhs.data.data());
	        const __m128 b = _mm_loadu_ps(rhs.data.data());
	        const __m128 c = _mm_div_ps(a, b);
	        _mm_storeu_ps(result.data.data(), c);
	        return true;
        }
    }
    return false;
}


template <typename T, typename U, typename V, int N>
auto gse::internal::add(const T& lhs, const vec::storage<U, N>& rhs, vec::storage<V, N>& result) -> bool {
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        // AVX2: 256-bit SIMD with integer operation
        if (avx2_supported && N == 8) {
        	const __m256i a = _mm256_set1_epi32(lhs);
            const __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.data.data()));
            const __m256i c = _mm256_add_epi32(a, b);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result.data.data()), c);
            return true;
        }
        // SSE2: 128-bit SIMD with integer operation
        if (sse2_supported && N == 4) {
        	const __m128i a = _mm_set1_epi32(lhs);
	        const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs.data.data()));
	        const __m128i c = _mm_add_epi32(a, b);
	        _mm_storeu_si128(reinterpret_cast<__m128i*>(result.data.data()), c);
	        return true;
        }
    }
    else if constexpr (std::is_same_v<T, float> && std::is_same_v<U, float>) {
        // AVX: 256-bit SIMD with float operation
        if (avx_supported && N == 8) {
        	const __m256 a = _mm256_set1_ps(lhs);
            const __m256 b = _mm256_loadu_ps(rhs.data.data());
            const __m256 c = _mm256_add_ps(a, b);
            _mm256_storeu_ps(result.data.data(), c);
            return true;
        }

        // SSE: 128-bit SIMD with float operation
        if (sse_supported && N == 4) {
        	const __m128 a = _mm_set1_ps(lhs);
	        const __m128 b = _mm_loadu_ps(rhs.data.data());
	        const __m128 c = _mm_add_ps(a, b);
	        _mm_storeu_ps(result.data.data(), c);
	        return true;
        }
    }
    return false;
}

template <typename T, typename U, typename V, int N>
auto gse::internal::add(const vec::storage<U, N>& lhs, const T& rhs, vec::storage<V, N>& result) -> bool {
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        // AVX2: 256-bit SIMD with integer operation
        if (avx2_supported && N == 8) {
        	const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data.data()));
            const __m256i b = _mm256_set1_epi32(rhs);
            const __m256i c = _mm256_add_epi32(a, b);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result.data.data()), c);
            return true;
        }
        // SSE2: 128-bit SIMD with integer operation
        if (sse2_supported && N == 4) {
        	const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data.data()));
	        const __m128i b = _mm_set1_epi32(rhs);
	        const __m128i c = _mm_add_epi32(a, b);
	        _mm_storeu_si128(reinterpret_cast<__m128i*>(result.data.data()), c);
	        return true;
        }
    }
    else if constexpr (std::is_same_v<T, float> && std::is_same_v<U, float>) {
        // AVX: 256-bit SIMD with float operation
        if (avx_supported && N == 8) {
        	const __m256 a = _mm256_loadu_ps(lhs.data.data());
            const __m256 b = _mm256_set1_ps(rhs);
            const __m256 c = _mm256_add_ps(a, b);
            _mm256_storeu_ps(result.data.data(), c);
            return true;
        }

        // SSE: 128-bit SIMD with float operation
        if (sse_supported && N == 4) {
        	const __m128 a = _mm_loadu_ps(lhs.data.data());
	        const __m128 b = _mm_set1_ps(rhs);
	        const __m128 c = _mm_add_ps(a, b);
	        _mm_storeu_ps(result.data.data(), c);
	        return true;
        }
    }
    return false;
}

template <typename T, typename U, typename V, int N>
auto gse::internal::subtract(const T& lhs, const vec::storage<U, N>& rhs, vec::storage<V, N>& result) -> bool {
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        // AVX2: 256-bit SIMD with integer operation
        if (avx2_supported && N == 8) {
        	const __m256i a = _mm256_set1_epi32(lhs);
            const __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.data.data()));
            const __m256i c = _mm256_sub_epi32(a, b);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result.data.data()), c);
            return true;
        }
        // SSE2: 128-bit SIMD with integer operation
        if (sse2_supported && N == 4) {
        	const __m128i a = _mm_set1_epi32(lhs);
	        const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs.data.data()));
	        const __m128i c = _mm_sub_epi32(a, b);
	        _mm_storeu_si128(reinterpret_cast<__m128i*>(result.data.data()), c);
	        return true;
        }
    }
    else if constexpr ( std::is_same_v<T, float> && std::is_same_v<U, float>) {
        // AVX: 256-bit SIMD with float operation
        if (avx_supported && N == 8) {
        	const __m256 a = _mm256_set1_ps(lhs);
            const __m256 b = _mm256_loadu_ps(rhs.data.data());
            const __m256 c = _mm256_sub_ps(a, b);
            _mm256_storeu_ps(result.data.data(), c);
            return true;
        }

        // SSE: 128-bit SIMD with float operation
        if (sse_supported && N == 4) {
        	const __m128 a = _mm_set1_ps(lhs);
	        const __m128 b = _mm_loadu_ps(rhs.data.data());
	        const __m128 c = _mm_sub_ps(a, b);
	        _mm_storeu_ps(result.data.data(), c);
	        return true;
        }
    }

    return false;
}

template <typename T, typename U, typename V, int N>
auto gse::internal::subtract(const vec::storage<T, N>& lhs, const U& rhs, vec::storage<V, N>& result) -> bool {
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        // AVX2: 256-bit SIMD with integer operation
        if (avx2_supported && N == 8) {
        	const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data.data()));
            const __m256i b = _mm256_set1_epi32(rhs);
            const __m256i c = _mm256_sub_epi32(a, b);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result.data.data()), c);
            return true;
        }
        // SSE2: 128-bit SIMD with integer operation
        if (sse2_supported && N == 4) {
        	const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data.data()));
	        const __m128i b = _mm_set1_epi32(rhs);
	        const __m128i c = _mm_sub_epi32(a, b);
	        _mm_storeu_si128(reinterpret_cast<__m128i*>(result.data.data()), c);
	        return true;
        }
    }
    else if constexpr ( std::is_same_v<T, float> && std::is_same_v<U, float>) {
        // AVX: 256-bit SIMD with float operation
        if (avx_supported && N == 8) {
        	const __m256 a = _mm256_loadu_ps(lhs.data.data());
            const __m256 b = _mm256_set1_ps(rhs);
            const __m256 c = _mm256_sub_ps(a, b);
            _mm256_storeu_ps(result.data.data(), c);
            return true;
        }

        // SSE: 128-bit SIMD with float operation
        if (sse_supported && N == 4) {
        	const __m128 a = _mm_loadu_ps(lhs.data.data());
	        const __m128 b = _mm_set1_ps(rhs);
	        const __m128 c = _mm_sub_ps(a, b);
	        _mm_storeu_ps(result.data.data(), c);
	        return true;
        }
    }
    return false;
}

template <typename T, typename U, typename V, int N>
auto gse::internal::multiply(const T& lhs, const vec::storage<U, N>& rhs, vec::storage<V, N>& result) -> bool {
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        // AVX2: 256-bit SIMD with integer operation
        if (avx2_supported && N == 8) {
        	const __m256i a = _mm256_set1_epi32(reinterpret_cast<int>(lhs));
            const __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.data.data()));
            const __m256i c = _mm256_mul_epi32(a, b);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result.data.data()), c);
            return true;
        }
        // SSE2: 128-bit SIMD with integer operation
        if (sse2_supported && N == 4) {
        	const __m128i a = _mm_set1_epi32(lhs);
	        const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs.data.data()));
	        const __m128i c = _mm_mul_epi32(a, b);
	        _mm_storeu_si128(reinterpret_cast<__m128i*>(result.data.data()), c);
	        return true;
        }
    }
    else if constexpr ( std::is_same_v<T, float> && std::is_same_v<U, float>) {
        // AVX: 256-bit SIMD with float operation
        if (avx_supported && N == 8) {
        	const __m256 a = _mm256_set1_ps(lhs);
            const __m256 b = _mm256_loadu_ps(rhs.data.data());
            const __m256 c = _mm256_mul_ps(a, b);
            _mm256_storeu_ps(result.data.data(), c);
            return true;
        }

        // SSE: 128-bit SIMD with float operation
        if (sse_supported && N == 4) {
        	const __m128 a = _mm_set1_ps(lhs);
	        const __m128 b = _mm_loadu_ps(rhs.data.data());
	        const __m128 c = _mm_mul_ps(a, b);
	        _mm_storeu_ps(result.data.data(), c);
	        return true;
        }
    }
    return false;
}

template <typename T, typename U, typename V, int N>
auto gse::internal::multiply(const vec::storage<T, N>& lhs, const U& rhs, vec::storage<V, N>& result) -> bool {
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        // AVX2: 256-bit SIMD with integer operation
        if (avx2_supported && N == 8) {
        	const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data.data()));
            const __m256i b = _mm256_set1_epi32(reinterpret_cast<int>(rhs));
            const __m256i c = _mm256_mul_epi32(a, b);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result.data.data()), c);
            return true;
        }
        // SSE2: 128-bit SIMD with integer operation
        if (sse2_supported && N == 4) {
        	const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data.data()));
	        const __m128i b = _mm_set1_epi32(rhs);
	        const __m128i c = _mm_mul_epi32(a, b);
	        _mm_storeu_si128(reinterpret_cast<__m128i*>(result.data.data()), c);
	        return true;
        }
    }
    else if constexpr (std::is_same_v<T, float> && std::is_same_v<U, float>) {
        // AVX: 256-bit SIMD with float operation
        if (avx_supported && N == 8) {
        	const __m256 a = _mm256_loadu_ps(lhs.data.data());
            const __m256 b = _mm256_set1_ps(rhs);
            const __m256 c = _mm256_mul_ps(a, b);
            _mm256_storeu_ps(result.data.data(), c);
            return true;
        }

        // SSE: 128-bit SIMD with float operation
        if (sse_supported && N == 4) {
        	const __m128 a = _mm_loadu_ps(lhs.data.data());
	        const __m128 b = _mm_set1_ps(rhs);
	        const __m128 c = _mm_mul_ps(a, b);
	        _mm_storeu_ps(result.data.data(), c);
	        return true;
        }
    }
    return false;
}

template <typename T, typename U, typename V, int N>
auto gse::internal::divide(const T& lhs, const vec::storage<U, N>& rhs, vec::storage<V, N>& result) -> bool {
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        // AVX2: 256-bit SIMD with integer operation
        if (avx2_supported && N == 8) {
        	const __m256i a = _mm256_set1_epi32(lhs);
            const __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.data.data()));
            const __m256i c = _mm256_div_epi32(a, b);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result.data.data()), c);
            return true;
        }
        // SSE2: 128-bit SIMD with integer operation
        if (sse2_supported && N == 4) {
        	const __m128i a = _mm_set1_epi32(lhs);
	        const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs.data.data()));
	        const __m128i c = _mm_div_epi32(a, b);
	        _mm_storeu_si128(reinterpret_cast<__m128i*>(result.data.data()), c);
	        return true;
        }
    }
    else if constexpr ( std::is_same_v<T, float> && std::is_same_v<U, float>) {
        // AVX: 256-bit SIMD with float operation
        if (avx_supported && N == 8) {
        	const __m256 a = _mm256_set1_ps(lhs);
            const __m256 b = _mm256_loadu_ps(rhs.data.data());
            const __m256 c = _mm256_div_ps(a, b);
            _mm256_storeu_ps(result.data.data(), c);
            return true;
        }

        // SSE: 128-bit SIMD with float operation
        if (sse_supported && N == 4) {
        	const __m128 a = _mm_set1_ps(lhs);
	        const __m128 b = _mm_loadu_ps(rhs.data.data());
	        const __m128 c = _mm_div_ps(a, b);
	        _mm_storeu_ps(result.data.data(), c);
	        return true;
        }
    }

    return false;
}

template <typename T, typename U, typename V, int N>
auto gse::internal::divide(const vec::storage<T, N>& lhs, const U& rhs, vec::storage<V, N>& result) -> bool {
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        // AVX2: 256-bit SIMD with integer operation
        if (avx2_supported && N == 8) {
        	const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data.data()));
            const __m256i b = _mm256_set1_epi32(rhs);
            const __m256i c = _mm256_div_epi32(a, b);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(result.data.data()), c);
            return true;
        }
        // SSE2: 128-bit SIMD with integer operation
        if (sse2_supported && N == 4) {
        	const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data.data()));
	        const __m128i b = _mm_set1_epi32(rhs);
	        const __m128i c = _mm_div_epi32(a, b);
	        _mm_storeu_si128(reinterpret_cast<__m128i*>(result.data.data()), c);
	        return true;
        }
    }
    else if constexpr ( std::is_same_v<T, float> && std::is_same_v<U, float>) {
        // AVX: 256-bit SIMD with float operation
        if (avx_supported && N == 8) {
        	const __m256 a = _mm256_loadu_ps(lhs.data.data());
            const __m256 b = _mm256_set1_ps(rhs);
            const __m256 c = _mm256_div_ps(a, b);
            _mm256_storeu_ps(result.data.data(), c);
            return true;
        }

        // SSE: 128-bit SIMD with float operation
        if (sse_supported && N == 4) {
        	const __m128 a = _mm_loadu_ps(lhs.data.data());
	        const __m128 b = _mm_set1_ps(rhs);
	        const __m128 c = _mm_div_ps(a, b);
	        _mm_storeu_ps(result.data.data(), c);
	        return true;
        }
    }

    return false;
}

template <typename T, typename U, typename V, int N>
auto gse::internal::dot(const vec::storage<T, N>& lhs, const vec::storage<U, N>& rhs, V& result) -> bool {
    result = V{};

    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        if constexpr (std::is_same_v<V, std::int32_t> || std::is_same_v<V, std::int32_t>) {
            if (avx2_supported && N == 8) {
                __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data.data()));
                __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.data.data()));
                __m256i product = _mm256_mullo_epi32(a, b);

                alignas(32) int tmp[8];
                _mm256_store_si256(reinterpret_cast<__m256i*>(tmp), product);
                for (int i = 0; i < 8; ++i) result += tmp[i];

                return true;
            }
            if (sse2_supported && N == 4) {
	            __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data.data()));
	            __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs.data.data()));
	            __m128i product = _mm_mullo_epi32(a, b);

	            alignas(16) int tmp[4];
	            _mm_store_si128(reinterpret_cast<__m128i*>(tmp), product);
	            for (int i = 0; i < 4; ++i) result += tmp[i];

	            return true;
            }
        }
    }

    else if constexpr (std::is_same_v<T, float> && std::is_same_v<U, float> && std::is_same_v<V, float>) {
        if (avx_supported && N == 8) {
            __m256 a = _mm256_loadu_ps(lhs.data.data());
            __m256 b = _mm256_loadu_ps(rhs.data.data());
            __m256 product = _mm256_mul_ps(a, b);

            __m128 low = _mm256_castps256_ps128(product);
            __m128 high = _mm256_extractf128_ps(product, 1);
            __m128 sum128 = _mm_add_ps(low, high);
            sum128 = _mm_hadd_ps(sum128, sum128);
            sum128 = _mm_hadd_ps(sum128, sum128);

            result = _mm_cvtss_f32(sum128);
            return true;
        }
        if (sse_supported && N == 4) {
	        __m128 a = _mm_loadu_ps(lhs.data.data());
	        __m128 b = _mm_loadu_ps(rhs.data.data());
	        __m128 product = _mm_mul_ps(a, b);
	        product = _mm_hadd_ps(product, product);
	        product = _mm_hadd_ps(product, product);

	        result = _mm_cvtss_f32(product);
	        return true;
        }
    }

    else if constexpr (std::is_same_v<T, double> && std::is_same_v<U, double> && std::is_same_v<V, double>) {
        if (avx_supported && N == 4) {
            __m256d a = _mm256_loadu_pd(lhs.data.data());
            __m256d b = _mm256_loadu_pd(rhs.data.data());
            __m256d product = _mm256_mul_pd(a, b);

            __m128d low = _mm256_castpd256_pd128(product);
            __m128d high = _mm256_extractf128_pd(product, 1);
            __m128d sum128 = _mm_add_pd(low, high);
            sum128 = _mm_hadd_pd(sum128, sum128);

            result = _mm_cvtsd_f64(sum128);
            return true;
        }
        if (sse_supported && N == 2) {
	        __m128d a = _mm_loadu_pd(lhs.data.data());
	        __m128d b = _mm_loadu_pd(rhs.data.data());
	        __m128d product = _mm_mul_pd(a, b);
	        product = _mm_hadd_pd(product, product);

	        result = _mm_cvtsd_f64(product);
	        return true;
        }
    }
    return false;
}

template <typename T, int N>
constexpr auto gse::value_ptr(const vec::storage<T, N>& storage) -> const T* {
    return &storage[0];
}

template <typename T, int N>
using pass_t = std::conditional_t<sizeof(T) * N <= 16, gse::vec::storage<T, N>, const gse::vec::storage<T, N>&>;

export namespace gse::vec {
    template <typename T, int N> constexpr auto operator+(pass_t<T, N> lhs, pass_t<T, N> rhs) -> storage<T, N>;
    template <typename T, int N> constexpr auto operator-(pass_t<T, N> lhs, pass_t<T, N> rhs) -> storage<T, N>;
    template <typename T, int N> constexpr auto operator*(pass_t<T, N> lhs, pass_t<T, N> rhs) -> storage<decltype(lhs[0] * rhs[0]), N>;
    template <typename T, int N> constexpr auto operator/(pass_t<T, N> lhs, pass_t<T, N> rhs) -> storage<decltype(lhs[0] / rhs[0]), N>;

    template <typename T, typename U, int N> [[deprecated("Mixing types in vector operations loses SIMD & is not recommended in hot paths")]]
	constexpr auto operator+(pass_t<T, N> lhs, pass_t<U, N> rhs) -> storage<decltype(lhs[0] + rhs[0]), N>;
	template <typename T, typename U, int N> [[deprecated("Mixing types in vector operations loses SIMD & is not recommended in hot paths")]]
	constexpr auto operator-(pass_t<T, N> lhs, pass_t<U, N> rhs) -> storage<decltype(lhs[0] - rhs[0]), N>;
	template <typename T, typename U, int N> [[deprecated("Mixing types in vector operations loses SIMD & is not recommended in hot paths")]]
	constexpr auto operator*(pass_t<T, N> lhs, pass_t<U, N> rhs) -> storage<decltype(lhs[0] * rhs[0]), N>;
	template <typename T, typename U, int N> [[deprecated("Mixing types in vector operations loses SIMD & is not recommended in hot paths")]]
	constexpr auto operator/(pass_t<T, N> lhs, pass_t<U, N> rhs) -> storage<decltype(lhs[0] / rhs[0]), N>;

    template <typename T, int N> constexpr auto operator+=(storage<T, N>& lhs, pass_t<T, N> rhs) -> storage<T, N>&;
    template <typename T, int N> constexpr auto operator-=(storage<T, N>& lhs, pass_t<T, N> rhs) -> storage<T, N>&;
    template <typename T, int N> constexpr auto operator*=(storage<T, N>& lhs, pass_t<T, N> rhs) -> storage<T, N>&;
    template <typename T, int N> constexpr auto operator/=(storage<T, N>& lhs, pass_t<T, N> rhs) -> storage<T, N>&;

    template <typename T, typename U, int N> constexpr auto operator+=(storage<T, N>& lhs, pass_t<U, N> rhs) -> storage<T, N>&;
    template <typename T, typename U, int N> constexpr auto operator-=(storage<T, N>& lhs, pass_t<U, N> rhs) -> storage<T, N>&;
    template <typename T, typename U, int N> constexpr auto operator*=(storage<T, N>& lhs, pass_t<U, N> rhs) -> storage<T, N>&;
    template <typename T, typename U, int N> constexpr auto operator/=(storage<T, N>& lhs, pass_t<U, N> rhs) -> storage<T, N>&;

    template <typename T, int N> constexpr auto operator*(pass_t<T, N> lhs, const T& rhs) -> storage<T, N>;
    template <typename T, int N> constexpr auto operator*(const T& lhs, pass_t<T, N> rhs) -> storage<T, N>;
    template <typename T, int N> constexpr auto operator/(pass_t<T, N> lhs, const T& rhs) -> storage<T, N>;
    template <typename T, int N> constexpr auto operator/(const T& lhs, pass_t<T, N> rhs) -> storage<T, N>;

    template <typename T, typename U, int N> constexpr auto operator*(pass_t<T, N> lhs, const U& rhs) -> storage<decltype(lhs[0] * rhs), N>;
    template <typename T, typename U, int N> constexpr auto operator*(const U& lhs, pass_t<T, N> rhs) -> storage<decltype(lhs* rhs[0]), N>;
    template <typename T, typename U, int N> constexpr auto operator/(pass_t<T, N> lhs, const U& rhs) -> storage<decltype(lhs[0] / rhs), N>;
    template <typename T, typename U, int N> constexpr auto operator/(const U& lhs, pass_t<T, N> rhs) -> storage<decltype(lhs / rhs[0]), N>;

    template <typename T, typename U, int N> constexpr auto operator*=(storage<T, N>& lhs, const U& rhs) -> storage<T, N>&;
    template <typename T, typename U, int N> constexpr auto operator/=(storage<T, N>& lhs, const U& rhs) -> storage<T, N>&;

    template <typename T, int N> constexpr auto operator*=(storage<T, N>& lhs, const T& rhs) -> storage<T, N>&;
    template <typename T, int N> constexpr auto operator/=(storage<T, N>& lhs, const T& rhs) -> storage<T, N>&;

    template <typename T, int N> constexpr auto operator+(pass_t<T, N> v) -> storage<T, N>;
    template <typename T, int N> constexpr auto operator-(pass_t<T, N> v) -> storage<T, N>;

    template <typename T, int N> constexpr auto operator==(pass_t<T, N> lhs, pass_t<T, N> rhs) -> bool;
    template <typename T, int N> constexpr auto operator!=(pass_t<T, N> lhs, pass_t<T, N> rhs) -> bool;

    template <typename T, int N> constexpr auto operator>(pass_t<T, N> lhs, pass_t<T, N> rhs) -> bool;
    template <typename T, int N> constexpr auto operator>=(pass_t<T, N> lhs, pass_t<T, N> rhs) -> bool;
    template <typename T, int N> constexpr auto operator<(pass_t<T, N> lhs, pass_t<T, N> rhs) -> bool;
    template <typename T, int N> constexpr auto operator<=(pass_t<T, N> lhs, pass_t<T, N> rhs) -> bool;

}

template <typename T, int N>
constexpr auto gse::vec::operator+(pass_t<T, N> lhs, pass_t<T, N> rhs) -> storage<T, N> {
    storage<T, N> result{};
    if (!internal::add(lhs, rhs, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = lhs[i] + rhs[i];
    }

    return result;
}

template <typename T, int N>
constexpr auto gse::vec::operator-(pass_t<T, N> lhs, pass_t<T, N> rhs) -> storage<T, N> {
    storage<T, N> result{};

    if (!internal::subtract(lhs, rhs, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = lhs[i] - rhs[i];
    }

    return result;
}

template <typename T, int N>
constexpr auto gse::vec::operator*(pass_t<T, N> lhs, pass_t<T, N> rhs) -> storage<decltype(lhs[0] * rhs[0]), N> {
    storage<decltype(lhs[0] * rhs[0]), N> result{};

    if (!internal::multiply(lhs, rhs, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = lhs[i] * rhs[i];
    }
    return result;
}

template <typename T, int N>
constexpr auto gse::vec::operator/(pass_t<T, N> lhs, pass_t<T, N> rhs) -> storage<decltype(lhs[0] / rhs[0]), N> {
    storage<decltype(lhs[0] / rhs[0]), N> result{};
    if (!internal::divide(lhs, rhs, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = lhs[i] / rhs[i];
    }
    return result;
}

template <typename T, typename U, int N>
constexpr auto gse::vec::operator+(pass_t<T, N> lhs, pass_t<U, N> rhs) -> storage<decltype(lhs[0] + rhs[0]), N> {
    storage<decltype(lhs[0] + rhs[0]), N> result{};

    if (!internal::add(lhs, rhs, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = lhs[i] + rhs[i];
    }
    return result;
}

template <typename T, typename U, int N>
constexpr auto gse::vec::operator-(pass_t<T, N> lhs, pass_t<U, N> rhs) -> storage<decltype(lhs[0] - rhs[0]), N> {
    storage<decltype(lhs[0] - rhs[0]), N> result{};

    if (!internal::subtract(lhs, rhs, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = lhs[i] - rhs[i];
    }
    return result;
}

template <typename T, typename U, int N>
constexpr auto gse::vec::operator*(pass_t<T, N> lhs, pass_t<U, N> rhs) -> storage<decltype(lhs[0] * rhs[0]), N> {
    storage<decltype(lhs[0] * rhs[0]), N> result{};

    if (!internal::multiply(lhs, rhs, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = lhs[i] * rhs[i];
    }
    return result;
}

template <typename T, typename U, int N>
constexpr auto gse::vec::operator/(pass_t<T, N> lhs, pass_t<U, N> rhs) -> storage<decltype(lhs[0] / rhs[0]), N> {
    storage<decltype(lhs[0] / rhs[0]), N> result{};

    if (!internal::divide(lhs, rhs, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = lhs[i] / rhs[i];
    }
    return result;
}

template <typename T, int N>
constexpr auto gse::vec::operator+=(storage<T, N>& lhs, pass_t<T, N> rhs) -> storage<T, N>& {
    if (!internal::add(lhs, rhs, lhs)) {
        for (int i = 0; i < N; ++i)
            lhs[i] += rhs[i];
    }
    return lhs;
}

template <typename T, int N>
constexpr auto gse::vec::operator-=(storage<T, N>& lhs, pass_t<T, N> rhs) -> storage<T, N>& {
    if (!internal::subtract(lhs, rhs, lhs)) {
        for (int i = 0; i < N; ++i)
            lhs[i] -= rhs[i];
    }
    return lhs;
}

template <typename T, int N>
constexpr auto gse::vec::operator*=(storage<T, N>& lhs, pass_t<T, N> rhs) -> storage<T, N>& {
    if (!internal::multiply(lhs, rhs, lhs)) {
        for (int i = 0; i < N; ++i)
            lhs[i] *= rhs[i];
    }
    return lhs;
}

template <typename T, int N>
constexpr auto gse::vec::operator/=(storage<T, N>& lhs, pass_t<T, N> rhs) -> storage<T, N>& {
    if (!internal::divide(lhs, rhs, lhs)) {
        for (int i = 0; i < N; ++i)
            lhs[i] /= rhs[i];
    }
    return lhs;
}

template <typename T, typename U, int N>
constexpr auto gse::vec::operator+=(storage<T, N>& lhs, pass_t<U, N> rhs) -> storage<T, N>& {
    if (!internal::add(lhs, rhs, lhs)) {
        for (int i = 0; i < N; ++i)
            lhs[i] += rhs[i];
    }
    return lhs;
}

template <typename T, typename U, int N>
constexpr auto gse::vec::operator-=(storage<T, N>& lhs, pass_t<U, N> rhs) -> storage<T, N>& {
    if (!internal::subtract(lhs, rhs, lhs)) {
        for (int i = 0; i < N; ++i)
            lhs[i] -= rhs[i];
    }
    return lhs;
}

template <typename T, typename U, int N>
constexpr auto gse::vec::operator*=(storage<T, N>& lhs, pass_t<U, N> rhs) -> storage<T, N>& {
    if (!internal::multiply(lhs, rhs, lhs)) {
        for (int i = 0; i < N; ++i)
            lhs[i] *= rhs[i];
    }
    return lhs;
}

template <typename T, typename U, int N>
constexpr auto gse::vec::operator/=(storage<T, N>& lhs, pass_t<U, N> rhs) -> storage<T, N>& {
    if (!internal::divide(lhs, rhs, lhs)) {
        for (int i = 0; i < N; ++i)
            lhs[i] /= rhs[i];
    }
    return lhs;
}

template <typename T, int N>
constexpr auto gse::vec::operator*(pass_t<T, N> lhs, const T& rhs) -> storage<T, N> {
    storage<decltype(lhs[0] * rhs), N> result{};

    if (!internal::multiply(rhs, lhs, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = lhs[i] * rhs;
    }
    return result;
}

template <typename T, int N>
constexpr auto gse::vec::operator*(const T& lhs, pass_t<T, N> rhs) -> storage<T, N> {
    storage<decltype(lhs* rhs[0]), N> result{};

    if (!internal::multiply(lhs, rhs, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = lhs * rhs[i];
    }
    return result;
}

template <typename T, int N>
constexpr auto gse::vec::operator/(pass_t<T, N> lhs, const T& rhs) -> storage<T, N> {
    storage<decltype(lhs[0] / rhs), N> result{};

    if (!internal::divide(lhs, rhs, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = lhs[i] / rhs;
    }
    return result;
}

template <typename T, int N>
constexpr auto gse::vec::operator/(const T& lhs, pass_t<T, N> rhs) -> storage<T, N> {
    storage<decltype(lhs / rhs[0]), N> result{};
    if (!internal::divide(lhs, rhs, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = lhs / rhs[i];
    }
    return result;
}

template <typename T, typename U, int N>
constexpr auto gse::vec::operator*(pass_t<T, N> lhs, const U& rhs) -> storage<decltype(lhs[0] * rhs), N> {
    storage<decltype(lhs[0] * rhs), N> result{};
    if (!internal::multiply(rhs, lhs, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = lhs[i] * rhs;
    }
    return result;
}

template <typename T, typename U, int N>
constexpr auto gse::vec::operator*(const U& lhs, pass_t<T, N> rhs) -> storage<decltype(lhs* rhs[0]), N> {
    storage<decltype(lhs* rhs[0]), N> result{};
    if (!internal::multiply(lhs, rhs, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = lhs * rhs[i];
    }
    return result;
}

template <typename T, typename U, int N>
constexpr auto gse::vec::operator/(pass_t<T, N> lhs, const U& rhs) -> storage<decltype(lhs[0] / rhs), N> {
    storage<decltype(lhs[0] / rhs), N> result{};
    if (!internal::divide(lhs, rhs, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = lhs[i] / rhs;
    }
    return result;
}

template <typename T, typename U, int N>
constexpr auto gse::vec::operator/(const U& lhs, pass_t<T, N> rhs) -> storage<decltype(lhs / rhs[0]), N> {
    storage<decltype(lhs / rhs[0]), N> result{};
    if (!internal::divide(lhs, rhs, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = lhs / rhs[i];
    }
    return result;
}

template <typename T, typename U, int N>
constexpr auto gse::vec::operator*=(storage<T, N>& lhs, const U& rhs) -> storage<T, N>& {
    if (!internal::multiply(rhs, lhs, lhs)) {
        for (int i = 0; i < N; ++i)
            lhs[i] *= rhs;
    }
    return lhs;
}

template <typename T, typename U, int N>
constexpr auto gse::vec::operator/=(storage<T, N>& lhs, const U& rhs) -> storage<T, N>& {
    if (!internal::divide(lhs, rhs, lhs)) {
        for (int i = 0; i < N; ++i)
            lhs[i] /= rhs;
    }
    return lhs;
}

template <typename T, int N>
constexpr auto gse::vec::operator*=(storage<T, N>& lhs, const T& rhs) -> storage<T, N>& {
    if (!internal::multiply(lhs, rhs, lhs)) {
        for (int i = 0; i < N; ++i)
            lhs[i] *= rhs;
    }
    return lhs;
}

template <typename T, int N>
constexpr auto gse::vec::operator/=(storage<T, N>& lhs, const T& rhs) -> storage<T, N>& {
    if (!internal::divide(lhs, rhs, lhs)) {
        for (int i = 0; i < N; ++i)
            lhs[i] /= rhs;
    }
    return lhs;
}

template <typename T, int N>
constexpr auto gse::vec::operator+(pass_t<T, N> v) -> storage<T, N> {
    return v;
}

template <typename T, int N>
constexpr auto gse::vec::operator-(pass_t<T, N> v) -> storage<T, N> {
    storage<T, N> result{};
    if (!internal::multiply(-1.0f, v, result)) {
        for (int i = 0; i < N; ++i)
            result[i] = -v[i];
    }
    return result;
}

template <typename T, int N>
constexpr auto gse::vec::operator==(pass_t<T, N> lhs, pass_t<T, N> rhs) -> bool {
    for (int i = 0; i < N; ++i)
        if (!(lhs[i] == rhs[i]))
            return false;
    return true;
}

template <typename T, int N>
constexpr auto gse::vec::operator!=(pass_t<T, N> lhs, pass_t<T, N> rhs) -> bool {
    return !(lhs == rhs);
}

template <typename T, int N>
constexpr auto gse::vec::operator>(pass_t<T, N> lhs, pass_t<T, N> rhs) -> bool {
    for (int i = 0; i < N; ++i)
        if (!(lhs[i] > rhs[i]))
            return false;
    return true;
}

template <typename T, int N>
constexpr auto gse::vec::operator>=(pass_t<T, N> lhs, pass_t<T, N> rhs) -> bool {
    for (int i = 0; i < N; ++i)
        if (!(lhs[i] >= rhs[i]))
            return false;
    return true;
}

template <typename T, int N>
constexpr auto gse::vec::operator<(pass_t<T, N> lhs, pass_t<T, N> rhs) -> bool {
    for (int i = 0; i < N; ++i)
        if (!(lhs[i] < rhs[i]))
            return false;
    return true;
}

template <typename T, int N>
constexpr auto gse::vec::operator<=(pass_t<T, N> lhs, pass_t<T, N> rhs) -> bool {
    for (int i = 0; i < N; ++i)
        if (!(lhs[i] <= rhs[i]))
            return false;
    return true;
}




