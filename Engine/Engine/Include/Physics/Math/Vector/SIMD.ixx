module;

#include <intrin.h> 

export module gse.physics.math:simd;

import std;

template <typename T> concept is_int32 = std::same_as<std::int32_t, std::remove_cvref_t<T>>;
template <typename T> concept is_float = std::same_as<float, std::remove_cvref_t<T>>;
template <typename T> concept is_double = std::same_as<double, std::remove_cvref_t<T>>;
template <typename T> concept simd_val = is_int32<T> || is_float<T> || is_double<T>;

template <typename T>
concept span = requires {
	typename T::element_type;
	requires std::is_same_v<std::remove_cvref_t<T>, std::span<typename T::element_type, T::extent>>;
};

export namespace gse::simd {
	auto add(const span auto& lhs, const span auto& rhs, span auto result) -> void;
	auto sub(const span auto& lhs, const span auto& rhs, span auto result) -> void;
	auto mul(const span auto& lhs, const span auto& rhs, span auto result) -> void;
	auto div(const span auto& lhs, const span auto& rhs, span auto result) -> void;

	auto dot(const span auto& lhs, const span auto& rhs, simd_val auto& result) -> void;
}

export namespace gse::simd {
	auto add_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void;
	auto sub_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void;
	auto mul_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void;
	auto div_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void;
}

namespace gse::simd {
	auto add_i(const int* lhs, const int* rhs, int* result, int size) -> void;
	auto add_f(const float* lhs, const float* rhs, float* result, int size) -> void;
	auto add_d(const double* lhs, const double* rhs, double* result, int size) -> void;

	auto sub_i(const int* lhs, const int* rhs, int* result, int size) -> void;
	auto sub_f(const float* lhs, const float* rhs, float* result, int size) -> void;
	auto sub_d(const double* lhs, const double* rhs, double* result, int size) -> void;

	auto mul_i(const int* lhs, const int* rhs, int* result, int size) -> void;
	auto mul_f(const float* lhs, const float* rhs, float* result, int size) -> void;
	auto mul_d(const double* lhs, const double* rhs, double* result, int size) -> void;

	auto div_i(const int* lhs, const int* rhs, int* result, int size) -> void;
	auto div_f(const float* lhs, const float* rhs, float* result, int size) -> void;
	auto div_d(const double* lhs, const double* rhs, double* result, int size) -> void;

	auto add_i(const int* lhs, int rhs, int* result, int size) -> void;
	auto add_f(const float* lhs, float rhs, float* result, int size) -> void;
	auto add_d(const double* lhs, double rhs, double* result, int size) -> void;

	auto sub_i(const int* lhs, int rhs, int* result, int size) -> void;
	auto sub_f(const float* lhs, float rhs, float* result, int size) -> void;
	auto sub_d(const double* lhs, double rhs, double* result, int size) -> void;

	auto mul_i(const int* lhs, int rhs, int* result, int size) -> void;
	auto mul_f(const float* lhs, float rhs, float* result, int size) -> void;
	auto mul_d(const double* lhs, double rhs, double* result, int size) -> void;

	auto div_i(const int* lhs, int rhs, int* result, int size) -> void;
	auto div_f(const float* lhs, float rhs, float* result, int size) -> void;
	auto div_d(const double* lhs, double rhs, double* result, int size) -> void;

	auto dot_i(const int* lhs, const int* rhs, int& result, int size) -> void;
	auto dot_f(const float* lhs, const float* rhs, float& result, int size) -> void;
	auto dot_d(const double* lhs, const double* rhs, double& result, int size) -> void;
}

export namespace gse::simd::support {
	bool sse = [] {
		int info[4];
		__cpuid(info, 1);
		return (info[3] & (1 << 25)) != 0; // SSE bit in EDX
		}();

	bool sse2 = [] {
		int info[4];
		__cpuid(info, 1);
		return (info[3] & (1 << 26)) != 0; // SSE2 bit in EDX
		}();

	bool avx = [] {
		int info[4];
		__cpuid(info, 1);
		const bool os_uses_xsave = (info[2] & 1 << 27) != 0;
		const bool cpu_supports_avx = (info[2] & 1 << 28) != 0;
		if (os_uses_xsave && cpu_supports_avx) {
			const std::uint64_t xcr_mask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
			return (xcr_mask & 0x6) == 0x6; // XMM (bit 1) and YMM (bit 2)
		}
		return false;
		}();

	bool avx2 = [] {
		int info[4];

		__cpuid(info, 1);
		const bool os_uses_xsave = (info[2] & 1 << 27) != 0;
		const bool cpu_supports_avx = (info[2] & 1 << 28) != 0;

		if (!(os_uses_xsave && cpu_supports_avx)) {
			return false;
		}

		const std::uint64_t xcr_mask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
		const bool xmm_enabled = (xcr_mask & 0x2) != 0;
		const bool ymm_enabled = (xcr_mask & 0x4) != 0;

		if (!(xmm_enabled && ymm_enabled)) {
			return false;
		}

		__cpuidex(info, 7, 0);
		return (info[1] & (1 << 5)) != 0; // AVX2 bit in EBX
		}();

	template <typename T, int N>
	concept simd = requires {
		requires
		(is_int32<T>
		&& ((N == 4 && sse2)
		|| (N == 8 && avx2)))

		|| (is_float
		&& ((N == 4 && sse)
		|| (N == 8 && avx)))

		|| (is_double
		&& ((N == 2 && sse)
		|| (N == 4 && avx)));
	};
}

auto gse::simd::add(const span auto& lhs, const span auto& rhs, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0] + rhs[0])>;
	constexpr size_t size = std::decay_t<decltype(lhs)>::extent;

	if (!support::simd<type, size>) {
		for (size_t i = 0; i < size; ++i)
			result[i] = lhs[i] + rhs[i];
		return;
	}

	if constexpr (is_int32<type>)
		add_i(lhs.data(), rhs.data(), result.data(), size);
	else if constexpr (is_float<type>)
		add_f(lhs.data(), rhs.data(), result.data(), size);
	else if constexpr (is_double<type>)
		add_d(lhs.data(), rhs.data(), result.data(), size);
}

auto gse::simd::sub(const span auto& lhs, const span auto& rhs, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0] - rhs[0])>;
	constexpr size_t size = std::decay_t<decltype(lhs)>::extent;

	if (!support::simd<type, size>) {
		for (size_t i = 0; i < size; ++i)
			result[i] = lhs[i] - rhs[i];
		return;
	}

	if constexpr (is_int32<type>)
		sub_i(lhs.data(), rhs.data(), result.data(), size);
	else if constexpr (is_float<type>)
		sub_f(lhs.data(), rhs.data(), result.data(), size);
	else if constexpr (is_double<type>)
		sub_d(lhs.data(), rhs.data(), result.data(), size);
}

auto gse::simd::mul(const span auto& lhs, const span auto& rhs, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0] * rhs[0])>;
	constexpr size_t size = std::decay_t<decltype(lhs)>::extent;

	if (!support::simd<type, size>) {
		for (size_t i = 0; i < size; ++i)
			result[i] = lhs[i] * rhs[i];
		return;
	}

	if constexpr (is_int32<type>)
		mul_i(lhs.data(), rhs.data(), result.data(), size);
	else if constexpr (is_float<type>)
		mul_f(lhs.data(), rhs.data(), result.data(), size);
	else if constexpr (is_double<type>)
		mul_d(lhs.data(), rhs.data(), result.data(), size);
}

auto gse::simd::div(const span auto& lhs, const span auto& rhs, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0] / rhs[0])>;
	constexpr size_t size = std::decay_t<decltype(lhs)>::extent;

	if (!support::simd<type, size>) {
		for (size_t i = 0; i < size; ++i)
			result[i] = lhs[i] / rhs[i];
		return;
	}

	if constexpr (is_int32<type>)
		div_i(lhs.data(), rhs.data(), result.data(), size);
	else if constexpr (is_float<type>)
		div_f(lhs.data(), rhs.data(), result.data(), size);
	else if constexpr (is_double<type>)
		div_d(lhs.data(), rhs.data(), result.data(), size);
}

auto gse::simd::dot(const span auto& lhs, const span auto& rhs, simd_val auto& result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0] * rhs[0])>;
	constexpr size_t size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (support::simd<type, size>) {
		if constexpr (is_int32<type>)
			dot_i(lhs.data(), rhs.data(), result, size);
		else if constexpr (is_float<type>)
			dot_f(lhs.data(), rhs.data(), result, size);
		else if constexpr (is_double<type>)
			dot_d(lhs.data(), rhs.data(), result, size);
	}
	else {
		result = 0;
		for (size_t i = 0; i < size; ++i)
			result += lhs[i] * rhs[i];
	}
}

auto gse::simd::add_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;
	constexpr int size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (support::simd<type, size>) {
		if constexpr (is_int32<type>)
			add_i(lhs.data(), scalar, result.data(), size);
		else if constexpr (is_float<type>)
			add_f(lhs.data(), scalar, result.data(), size);
		else if constexpr (is_double<type>)
			add_d(lhs.data(), scalar, result.data(), size);
	}
	else {
		for (size_t i = 0; i < size; ++i) {
			result[i] = lhs[i] + scalar;
		}
	}
}

auto gse::simd::sub_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;
	constexpr int size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (support::simd<type, size>) {
		if constexpr (is_int32<type>)
			sub_i(lhs.data(), scalar, result.data(), size);
		else if constexpr (is_float<type>)
			sub_f(lhs.data(), scalar, result.data(), size);
		else if constexpr (is_double<type>)
			sub_d(lhs.data(), scalar, result.data(), size);
	}
	else {
		for (size_t i = 0; i < size; ++i) {
			result[i] = lhs[i] - scalar;
		}
	}
}

auto gse::simd::mul_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;
	constexpr int size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (support::simd<type, size>) {
		if constexpr (is_int32<type>)
			mul_i(lhs.data(), scalar, result.data(), size);
		else if constexpr (is_float<type>)
			mul_f(lhs.data(), scalar, result.data(), size);
		else if constexpr (is_double<type>)
			mul_d(lhs.data(), scalar, result.data(), size);
	}
	else {
		for (size_t i = 0; i < size; ++i) {
			result[i] = lhs[i] * scalar;
		}
	}
}

auto gse::simd::div_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;
	constexpr int size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (support::simd<type, size>) {
		if constexpr (is_int32<type>)
			div_i(lhs.data(), scalar, result.data(), size);
		else if constexpr (is_float<type>)
			div_f(lhs.data(), scalar, result.data(), size);
		else if constexpr (is_double<type>)
			div_d(lhs.data(), scalar, result.data(), size);
	}
	else {
		for (size_t i = 0; i < size; ++i) {
			result[i] = lhs[i] / scalar;
		}
	}
}

auto gse::simd::add_i(const int* lhs, const int* rhs, int* result, const int size) -> void {
	if (support::avx2 && size == 8) {
		const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs));
		const __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs));
		const __m256i c = _mm256_add_epi32(a, b);
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(result), c);
	}
	else if (support::sse2 && size == 4) {
		const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs));
		const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs));
		const __m128i c = _mm_add_epi32(a, b);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(result), c);
	}
}

auto gse::simd::add_f(const float* lhs, const float* rhs, float* result, const int size) -> void {
	if (support::avx && size == 8) {
		const __m256 a = _mm256_loadu_ps(lhs);
		const __m256 b = _mm256_loadu_ps(rhs);
		const __m256 c = _mm256_add_ps(a, b);
		_mm256_storeu_ps(result, c);
	}
	else if (support::sse && size == 4) {
		const __m128 a = _mm_loadu_ps(lhs);
		const __m128 b = _mm_loadu_ps(rhs);
		const __m128 c = _mm_add_ps(a, b);
		_mm_storeu_ps(result, c);
	}
}

auto gse::simd::add_d(const double* lhs, const double* rhs, double* result, const int size) -> void {
	if (support::avx && size == 4) {
		const __m256d a = _mm256_loadu_pd(lhs);
		const __m256d b = _mm256_loadu_pd(rhs);
		const __m256d c = _mm256_add_pd(a, b);
		_mm256_storeu_pd(result, c);
	}
	else if (support::sse && size == 2) {
		const __m128d a = _mm_loadu_pd(lhs);
		const __m128d b = _mm_loadu_pd(rhs);
		const __m128d c = _mm_add_pd(a, b);
		_mm_storeu_pd(result, c);
	}
}

auto gse::simd::sub_i(const int* lhs, const int* rhs, int* result, const int size) -> void {
	if (support::avx2 && size == 8) {
		const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs));
		const __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs));
		const __m256i c = _mm256_sub_epi32(a, b);
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(result), c);
	}
	else if (support::sse2 && size == 4) {
		const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs));
		const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs));
		const __m128i c = _mm_sub_epi32(a, b);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(result), c);
	}
}

auto gse::simd::sub_f(const float* lhs, const float* rhs, float* result, const int size) -> void {
	if (support::avx && size == 8) {
		const __m256 a = _mm256_loadu_ps(lhs);
		const __m256 b = _mm256_loadu_ps(rhs);
		const __m256 c = _mm256_sub_ps(a, b);
		_mm256_storeu_ps(result, c);
	}
	else if (support::sse && size == 4) {
		const __m128 a = _mm_loadu_ps(lhs);
		const __m128 b = _mm_loadu_ps(rhs);
		const __m128 c = _mm_sub_ps(a, b);
		_mm_storeu_ps(result, c);
	}
}

auto gse::simd::sub_d(const double* lhs, const double* rhs, double* result, const int size) -> void {
	if (support::avx && size == 4) {
		const __m256d a = _mm256_loadu_pd(lhs);
		const __m256d b = _mm256_loadu_pd(rhs);
		const __m256d c = _mm256_sub_pd(a, b);
		_mm256_storeu_pd(result, c);
	}
	else if (support::sse && size == 2) {
		const __m128d a = _mm_loadu_pd(lhs);
		const __m128d b = _mm_loadu_pd(rhs);
		const __m128d c = _mm_sub_pd(a, b);
		_mm_storeu_pd(result, c);
	}
}

auto gse::simd::mul_i(const int* lhs, const int* rhs, int* result, const int size) -> void {
	if (support::avx2 && size == 8) {
		const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs));
		const __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs));
		const __m256i c = _mm256_mullo_epi32(a, b);
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(result), c);
	}
	else if (support::sse2 && size == 4) {
		const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs));
		const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs));
		const __m128i c = _mm_mullo_epi32(a, b);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(result), c);
	}
}

auto gse::simd::mul_f(const float* lhs, const float* rhs, float* result, const int size) -> void {
	if (support::avx && size == 8) {
		const __m256 a = _mm256_loadu_ps(lhs);
		const __m256 b = _mm256_loadu_ps(rhs);
		const __m256 c = _mm256_mul_ps(a, b);
		_mm256_storeu_ps(result, c);
	}
	else if (support::sse && size == 4) {
		const __m128 a = _mm_loadu_ps(lhs);
		const __m128 b = _mm_loadu_ps(rhs);
		const __m128 c = _mm_mul_ps(a, b);
		_mm_storeu_ps(result, c);
	}
}

auto gse::simd::mul_d(const double* lhs, const double* rhs, double* result, const int size) -> void {
	if (support::avx && size == 4) {
		const __m256d a = _mm256_loadu_pd(lhs);
		const __m256d b = _mm256_loadu_pd(rhs);
		const __m256d c = _mm256_mul_pd(a, b);
		_mm256_storeu_pd(result, c);
	}
	else if (support::sse && size == 2) {
		const __m128d a = _mm_loadu_pd(lhs);
		const __m128d b = _mm_loadu_pd(rhs);
		const __m128d c = _mm_mul_pd(a, b);
		_mm_storeu_pd(result, c);
	}
}

auto gse::simd::div_i(const int* lhs, const int* rhs, int* result, const int size) -> void {
	if (support::avx2 && size == 8) {
		const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs));
		const __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs));
		const __m256i c = _mm256_div_epi32(a, b);
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(result), c);
	}
	else if (support::sse2 && size == 4) {
		const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs));
		const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs));
		const __m128i c = _mm_div_epi32(a, b);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(result), c);
	}
}

auto gse::simd::div_f(const float* lhs, const float* rhs, float* result, const int size) -> void {
	if (support::avx && size == 8) {
		const __m256 a = _mm256_loadu_ps(lhs);
		const __m256 b = _mm256_loadu_ps(rhs);
		const __m256 c = _mm256_div_ps(a, b);
		_mm256_storeu_ps(result, c);
	}
	else if (support::sse && size == 4) {
		const __m128 a = _mm_loadu_ps(lhs);
		const __m128 b = _mm_loadu_ps(rhs);
		const __m128 c = _mm_div_ps(a, b);
		_mm_storeu_ps(result, c);
	}
}

auto gse::simd::div_d(const double* lhs, const double* rhs, double* result, const int size) -> void {
	if (support::avx && size == 4) {
		const __m256d a = _mm256_loadu_pd(lhs);
		const __m256d b = _mm256_loadu_pd(rhs);
		const __m256d c = _mm256_div_pd(a, b);
		_mm256_storeu_pd(result, c);
	}
	else if (support::sse && size == 2) {
		const __m128d a = _mm_loadu_pd(lhs);
		const __m128d b = _mm_loadu_pd(rhs);
		const __m128d c = _mm_div_pd(a, b);
		_mm_storeu_pd(result, c);
	}
}

auto gse::simd::add_i(const int* lhs, const int rhs, int* result, const int size) -> void {
	if (support::avx2 && size == 8) {
		const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs));
		const __m256i b = _mm256_set1_epi32(rhs);
		const __m256i c = _mm256_add_epi32(a, b);
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(result), c);
	}
	else if (support::sse2 && size == 4) {
		const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs));
		const __m128i b = _mm_set1_epi32(rhs);
		const __m128i c = _mm_add_epi32(a, b);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(result), c);
	}
}

auto gse::simd::add_f(const float* lhs, const float rhs, float* result, const int size) -> void {
	if (support::avx && size == 8) {
		const __m256 a = _mm256_loadu_ps(lhs);
		const __m256 b = _mm256_set1_ps(rhs);
		const __m256 c = _mm256_add_ps(a, b);
		_mm256_storeu_ps(result, c);
	}
	else if (support::sse && size == 4) {
		const __m128 a = _mm_loadu_ps(lhs);
		const __m128 b = _mm_set1_ps(rhs);
		const __m128 c = _mm_add_ps(a, b);
		_mm_storeu_ps(result, c);
	}
}

auto gse::simd::add_d(const double* lhs, const double rhs, double* result, const int size) -> void {
	if (support::avx && size == 4) {
		const __m256d a = _mm256_loadu_pd(lhs);
		const __m256d b = _mm256_set1_pd(rhs);
		const __m256d c = _mm256_add_pd(a, b);
		_mm256_storeu_pd(result, c);
	}
	else if (support::sse && size == 2) {
		const __m128d a = _mm_loadu_pd(lhs);
		const __m128d b = _mm_set1_pd(rhs);
		const __m128d c = _mm_add_pd(a, b);
		_mm_storeu_pd(result, c);
	}
}

auto gse::simd::sub_i(const int* lhs, const int rhs, int* result, const int size) -> void {
	if (support::avx2 && size == 8) {
		const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs));
		const __m256i b = _mm256_set1_epi32(rhs);
		const __m256i c = _mm256_sub_epi32(a, b);
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(result), c);
	}
	else if (support::sse2 && size == 4) {
		const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs));
		const __m128i b = _mm_set1_epi32(rhs);
		const __m128i c = _mm_sub_epi32(a, b);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(result), c);
	}
}

auto gse::simd::sub_f(const float* lhs, const float rhs, float* result, const int size) -> void {
	if (support::avx && size == 8) {
		const __m256 a = _mm256_loadu_ps(lhs);
		const __m256 b = _mm256_set1_ps(rhs);
		const __m256 c = _mm256_sub_ps(a, b);
		_mm256_storeu_ps(result, c);
	}
	else if (support::sse && size == 4) {
		const __m128 a = _mm_loadu_ps(lhs);
		const __m128 b = _mm_set1_ps(rhs);
		const __m128 c = _mm_sub_ps(a, b);
		_mm_storeu_ps(result, c);
	}
}

auto gse::simd::sub_d(const double* lhs, const double rhs, double* result, const int size) -> void {
	if (support::avx && size == 4) {
		const __m256d a = _mm256_loadu_pd(lhs);
		const __m256d b = _mm256_set1_pd(rhs);
		const __m256d c = _mm256_sub_pd(a, b);
		_mm256_storeu_pd(result, c);
	}
	else if (support::sse && size == 2) {
		const __m128d a = _mm_loadu_pd(lhs);
		const __m128d b = _mm_set1_pd(rhs);
		const __m128d c = _mm_sub_pd(a, b);
		_mm_storeu_pd(result, c);
	}
}

auto gse::simd::mul_i(const int* lhs, const int rhs, int* result, const int size) -> void {
	if (support::avx2 && size == 8) {
		const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs));
		const __m256i b = _mm256_set1_epi32(rhs);
		const __m256i c = _mm256_mullo_epi32(a, b);
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(result), c);
	}
	else if (support::sse2 && size == 4) {
		const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs));
		const __m128i b = _mm_set1_epi32(rhs);
		const __m128i c = _mm_mullo_epi32(a, b);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(result), c);
	}
}

auto gse::simd::mul_f(const float* lhs, const float rhs, float* result, const int size) -> void {
	if (support::avx && size == 8) {
		const __m256 a = _mm256_loadu_ps(lhs);
		const __m256 b = _mm256_set1_ps(rhs);
		const __m256 c = _mm256_mul_ps(a, b);
		_mm256_storeu_ps(result, c);
	}
	else if (support::sse && size == 4) {
		const __m128 a = _mm_loadu_ps(lhs);
		const __m128 b = _mm_set1_ps(rhs);
		const __m128 c = _mm_mul_ps(a, b);
		_mm_storeu_ps(result, c);
	}
}

auto gse::simd::mul_d(const double* lhs, const double rhs, double* result, const int size) -> void {
	if (support::avx && size == 4) {
		const __m256d a = _mm256_loadu_pd(lhs);
		const __m256d b = _mm256_set1_pd(rhs);
		const __m256d c = _mm256_mul_pd(a, b);
		_mm256_storeu_pd(result, c);
	}
	else if (support::sse && size == 2) {
		const __m128d a = _mm_loadu_pd(lhs);
		const __m128d b = _mm_set1_pd(rhs);
		const __m128d c = _mm_mul_pd(a, b);
		_mm_storeu_pd(result, c);
	}
}

auto gse::simd::div_i(const int* lhs, const int rhs, int* result, const int size) -> void {
	if (support::avx2 && size == 8) {
		const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs));
		const __m256i b = _mm256_set1_epi32(rhs);
		const __m256i c = _mm256_div_epi32(a, b);
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(result), c);
	}
	else if (support::sse2 && size == 4) {
		const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs));
		const __m128i b = _mm_set1_epi32(rhs);
		const __m128i c = _mm_div_epi32(a, b);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(result), c);
	}
}

auto gse::simd::div_f(const float* lhs, const float rhs, float* result, const int size) -> void {
	if (support::avx && size == 8) {
		const __m256 a = _mm256_loadu_ps(lhs);
		const __m256 b = _mm256_set1_ps(rhs);
		const __m256 c = _mm256_div_ps(a, b);
		_mm256_storeu_ps(result, c);
	}
	else if (support::sse && size == 4) {
		const __m128 a = _mm_loadu_ps(lhs);
		const __m128 b = _mm_set1_ps(rhs);
		const __m128 c = _mm_div_ps(a, b);
		_mm_storeu_ps(result, c);
	}
}

auto gse::simd::div_d(const double* lhs, const double rhs, double* result, const int size) -> void {
	if (support::avx && size == 4) {
		const __m256d a = _mm256_loadu_pd(lhs);
		const __m256d b = _mm256_set1_pd(rhs);
		const __m256d c = _mm256_div_pd(a, b);
		_mm256_storeu_pd(result, c);
	}
	else if (support::sse && size == 2) {
		const __m128d a = _mm_loadu_pd(lhs);
		const __m128d b = _mm_set1_pd(rhs);
		const __m128d c = _mm_div_pd(a, b);
		_mm_storeu_pd(result, c);
	}
}

auto gse::simd::dot_i(const int* lhs, const int* rhs, int& result, const int size) -> void {
	if (support::avx2 && size == 8) {
		const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs));
		const __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs));
		const __m256i mul = _mm256_mullo_epi32(a, b);

		__m128i low = _mm256_castsi256_si128(mul);
		__m128i high = _mm256_extracti128_si256(mul, 1);
		__m128i sum = _mm_add_epi32(low, high);
		sum = _mm_hadd_epi32(sum, sum);
		sum = _mm_hadd_epi32(sum, sum);
		result = _mm_cvtsi128_si32(sum);
	}
	else if (support::sse2 && size == 4) {
		const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs));
		const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs));
		const __m128i mul = _mm_mullo_epi32(a, b);
		__m128i sum = _mm_hadd_epi32(mul, mul);
		sum = _mm_hadd_epi32(sum, sum);
		result = _mm_cvtsi128_si32(sum);
	}
}

auto gse::simd::dot_f(const float* lhs, const float* rhs, float& result, const int size) -> void {
	if (support::avx && size == 8) {
		const __m256 a = _mm256_loadu_ps(lhs);
		const __m256 b = _mm256_loadu_ps(rhs);
		const __m256 mul = _mm256_mul_ps(a, b);

		__m128 low = _mm256_castps256_ps128(mul);
		__m128 high = _mm256_extractf128_ps(mul, 1);
		__m128 sum = _mm_add_ps(low, high);             
		sum = _mm_hadd_ps(sum, sum);
		sum = _mm_hadd_ps(sum, sum);
		result = _mm_cvtss_f32(sum);
	}
	else if (support::sse && size == 4) {
		const __m128 a = _mm_loadu_ps(lhs);
		const __m128 b = _mm_loadu_ps(rhs);
		const __m128 c = _mm_dp_ps(a, b, 0xF1);
		result = _mm_cvtss_f32(c);
	}
}

auto gse::simd::dot_d(const double* lhs, const double* rhs, double& result, const int size) -> void {
	if (support::avx && size == 4) {
		const __m256d a = _mm256_loadu_pd(lhs);
		const __m256d b = _mm256_loadu_pd(rhs);
		const __m256d mul = _mm256_mul_pd(a, b);

		__m128d low = _mm256_castpd256_pd128(mul);
		__m128d high = _mm256_extractf128_pd(mul, 1);
		__m128d sum = _mm_add_pd(low, high);
		sum = _mm_hadd_pd(sum, sum);
		result = _mm_cvtsd_f64(sum);
	}
	else if (support::sse && size == 2) {
		const __m128d a = _mm_loadu_pd(lhs);
		const __m128d b = _mm_loadu_pd(rhs);
		const __m128d mul = _mm_mul_pd(a, b);
		const __m128d sum = _mm_hadd_pd(mul, mul);
		result = _mm_cvtsd_f64(sum);
	}
}
