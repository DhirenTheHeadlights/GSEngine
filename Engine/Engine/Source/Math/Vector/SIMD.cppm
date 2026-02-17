module;

#include <intrin.h> 

export module gse.math:simd;

import std;

template <typename T> concept is_int32  = std::same_as<std::int32_t, std::remove_cvref_t<T>>;
template <typename T> concept is_float  = std::same_as<float, std::remove_cvref_t<T>>;
template <typename T> concept is_double = std::same_as<double, std::remove_cvref_t<T>>;
template <typename T> concept simd_val  = is_int32<T> || is_float<T> || is_double<T>;

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
	auto abs(const span auto& v, span auto result) -> void;
	auto min(const span auto& lhs, const span auto& rhs, span auto result) -> void;
	auto max(const span auto& lhs, const span auto& rhs, span auto result) -> void;
	auto clamp(const span auto& v, const span auto& min_v, const span auto& max_v, span auto result) -> void;

	auto mul_mat4(const float* lhs, const float* rhs, float* result) -> void;
}

export namespace gse::simd {
	auto add_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void;
	auto sub_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void;
	auto mul_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void;
	auto div_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void;
	auto min_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void;
	auto max_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void;
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

	auto abs_i(const int* v, int* result, int size) -> void;
	auto abs_f(const float* v, float* result, int size) -> void;
	auto abs_d(const double* v, double* result, int size) -> void;

	auto min_i(const int* lhs, const int* rhs, int* result, int size) -> void;
	auto min_f(const float* lhs, const float* rhs, float* result, int size) -> void;
	auto min_d(const double* lhs, const double* rhs, double* result, int size) -> void;

	auto max_i(const int* lhs, const int* rhs, int* result, int size) -> void;
	auto max_f(const float* lhs, const float* rhs, float* result, int size) -> void;
	auto max_d(const double* lhs, const double* rhs, double* result, int size) -> void;
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

	bool sse3 = [] {
		int info[4];
		__cpuid(info, 1);
		return (info[2] & (1 << 0)) != 0; // SSE3 bit in ECX
	}();

	bool sse41 = [] {
		int info[4];
		__cpuid(info, 1);
		return (info[2] & (1 << 19)) != 0; // SSE4.1 bit in ECX
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

auto gse::simd::abs(const span auto& v, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(v[0])>;
	constexpr size_t size = std::decay_t<decltype(v)>::extent;

	if (!support::simd<type, size>) {
		for (size_t i = 0; i < size; ++i)
			result[i] = std::abs(v[i]);
		return;
	}

	if constexpr (is_int32<type>)
		abs_i(v.data(), result.data(), size);
	else if constexpr (is_float<type>)
		abs_f(v.data(), result.data(), size);
	else if constexpr (is_double<type>)
		abs_d(v.data(), result.data(), size);
}

auto gse::simd::min(const span auto& lhs, const span auto& rhs, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;
	constexpr size_t size = std::decay_t<decltype(lhs)>::extent;

	if (!support::simd<type, size>) {
		for (size_t i = 0; i < size; ++i)
			result[i] = std::min(lhs[i], rhs[i]);
		return;
	}

	if constexpr (is_int32<type>)
		min_i(lhs.data(), rhs.data(), result.data(), size);
	else if constexpr (is_float<type>)
		min_f(lhs.data(), rhs.data(), result.data(), size);
	else if constexpr (is_double<type>)
		min_d(lhs.data(), rhs.data(), result.data(), size);
}

auto gse::simd::max(const span auto& lhs, const span auto& rhs, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;
	constexpr size_t size = std::decay_t<decltype(lhs)>::extent;

	if (!support::simd<type, size>) {
		for (size_t i = 0; i < size; ++i)
			result[i] = std::max(lhs[i], rhs[i]);
		return;
	}

	if constexpr (is_int32<type>)
		max_i(lhs.data(), rhs.data(), result.data(), size);
	else if constexpr (is_float<type>)
		max_f(lhs.data(), rhs.data(), result.data(), size);
	else if constexpr (is_double<type>)
		max_d(lhs.data(), rhs.data(), result.data(), size);
}

auto gse::simd::clamp(const span auto& v, const span auto& min_v, const span auto& max_v, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(v[0])>;
	constexpr size_t size = std::decay_t<decltype(v)>::extent;

	if (!support::simd<type, size>) {
		for (size_t i = 0; i < size; ++i)
			result[i] = std::clamp(v[i], min_v[i], max_v[i]);
		return;
	}

	if constexpr (is_float<type>) {
		if (support::avx && size == 8) {
			const __m256 temp = _mm256_min_ps(_mm256_loadu_ps(v.data()), _mm256_loadu_ps(max_v.data()));
			_mm256_storeu_ps(result.data(), _mm256_max_ps(temp, _mm256_loadu_ps(min_v.data())));
		}
		else if (support::sse && size == 4) {
			const __m128 temp = _mm_min_ps(_mm_loadu_ps(v.data()), _mm_loadu_ps(max_v.data()));
			_mm_storeu_ps(result.data(), _mm_max_ps(temp, _mm_loadu_ps(min_v.data())));
		}
	}
	else if constexpr (is_double<type>) {
		if (support::avx && size == 4) {
			const __m256d temp = _mm256_min_pd(_mm256_loadu_pd(v.data()), _mm256_loadu_pd(max_v.data()));
			_mm256_storeu_pd(result.data(), _mm256_max_pd(temp, _mm256_loadu_pd(min_v.data())));
		}
		else if (support::sse2 && size == 2) {
			const __m128d temp = _mm_min_pd(_mm_loadu_pd(v.data()), _mm_loadu_pd(max_v.data()));
			_mm_storeu_pd(result.data(), _mm_max_pd(temp, _mm_loadu_pd(min_v.data())));
		}
	}
	else if constexpr (is_int32<type>) {
		if (support::avx2 && size == 8) {
			const __m256i temp = _mm256_min_epi32(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(v.data())), _mm256_loadu_si256(reinterpret_cast<const __m256i*>(max_v.data())));
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(result.data()), _mm256_max_epi32(temp, _mm256_loadu_si256(reinterpret_cast<const __m256i*>(min_v.data()))));
		}
		else if (support::sse41 && size == 4) {
			const __m128i temp = _mm_min_epi32(_mm_loadu_si128(reinterpret_cast<const __m128i*>(v.data())), _mm_loadu_si128(reinterpret_cast<const __m128i*>(max_v.data())));
			_mm_storeu_si128(reinterpret_cast<__m128i*>(result.data()), _mm_max_epi32(temp, _mm_loadu_si128(reinterpret_cast<const __m128i*>(min_v.data()))));
		}
	}
}

auto gse::simd::add_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;

	if constexpr (constexpr int size = std::decay_t<decltype(lhs)>::extent; support::simd<type, size>) {
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

	if constexpr (constexpr int size = std::decay_t<decltype(lhs)>::extent; support::simd<type, size>) {
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

	if constexpr (constexpr int size = std::decay_t<decltype(lhs)>::extent; support::simd<type, size>) {
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

auto gse::simd::min_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;

	if constexpr (constexpr int size = std::decay_t<decltype(lhs)>::extent; support::simd<type, size>) {
		if constexpr (is_int32<type>)
			min_i(lhs.data(), scalar, result.data(), size);
		else if constexpr (is_float<type>)
			min_f(lhs.data(), scalar, result.data(), size);
		else if constexpr (is_double<type>)
			min_d(lhs.data(), scalar, result.data(), size);
	}
	else {
		for (size_t i = 0; i < size; ++i) {
			result[i] = std::min(lhs[i], scalar);
		}
	}
}

auto gse::simd::max_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;

	if constexpr (constexpr int size = std::decay_t<decltype(lhs)>::extent; support::simd<type, size>) {
		if constexpr (is_int32<type>)
			max_i(lhs.data(), scalar, result.data(), size);
		else if constexpr (is_float<type>)
			max_f(lhs.data(), scalar, result.data(), size);
		else if constexpr (is_double<type>)
			max_d(lhs.data(), scalar, result.data(), size);
	}
	else {
		for (size_t i = 0; i < size; ++i) {
			result[i] = std::max(lhs[i], scalar);
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

		const __m128 low = _mm256_castps256_ps128(mul);
		const __m128 high = _mm256_extractf128_ps(mul, 1);
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

		const __m128d low = _mm256_castpd256_pd128(mul);
		const __m128d high = _mm256_extractf128_pd(mul, 1);
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

auto gse::simd::abs_i(const int* v, int* result, const int size) -> void {
	if (support::avx2 && size == 8) {
		const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(v));
		const __m256i c = _mm256_abs_epi32(a);
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(result), c);
	}
	else if (support::sse3 && size == 4) { 
		const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(v));
		const __m128i c = _mm_abs_epi32(a);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(result), c);
	}
}

auto gse::simd::abs_f(const float* v, float* result, const int size) -> void {
	if (support::avx && size == 8) {
		const __m256i mask = _mm256_set1_epi32(0x7FFFFFFF);
		const __m256 a = _mm256_loadu_ps(v);
		const __m256 c = _mm256_and_ps(a, _mm256_castsi256_ps(mask));
		_mm256_storeu_ps(result, c);
	}
	else if (support::sse && size == 4) {
		const __m128i mask = _mm_set1_epi32(0x7FFFFFFF);
		const __m128 a = _mm_loadu_ps(v);
		const __m128 c = _mm_and_ps(a, _mm_castsi128_ps(mask));
		_mm_storeu_ps(result, c);
	}
}

auto gse::simd::abs_d(const double* v, double* result, const int size) -> void {
	if (support::avx && size == 4) {
		const __m256i mask = _mm256_set1_epi64x(0x7FFFFFFFFFFFFFFF);
		const __m256d a = _mm256_loadu_pd(v);
		const __m256d c = _mm256_and_pd(a, _mm256_castsi256_pd(mask));
		_mm256_storeu_pd(result, c);
	}
	else if (support::sse2 && size == 2) {
		const __m128i mask = _mm_set1_epi64x(0x7FFFFFFFFFFFFFFF);
		const __m128d a = _mm_loadu_pd(v);
		const __m128d c = _mm_and_pd(a, _mm_castsi128_pd(mask));
		_mm_storeu_pd(result, c);
	}
}

auto gse::simd::min_i(const int* lhs, const int* rhs, int* result, const int size) -> void {
	if (support::avx2 && size == 8) {
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(result), _mm256_min_epi32(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs)), _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs))));
	}
	else if (support::sse41 && size == 4) { 
		_mm_storeu_si128(reinterpret_cast<__m128i*>(result), _mm_min_epi32(_mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs)), _mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs))));
	}
}

auto gse::simd::min_f(const float* lhs, const float* rhs, float* result, const int size) -> void {
	if (support::avx && size == 8) {
		_mm256_storeu_ps(result, _mm256_min_ps(_mm256_loadu_ps(lhs), _mm256_loadu_ps(rhs)));
	}
	else if (support::sse && size == 4) {
		_mm_storeu_ps(result, _mm_min_ps(_mm_loadu_ps(lhs), _mm_loadu_ps(rhs)));
	}
}

auto gse::simd::min_d(const double* lhs, const double* rhs, double* result, const int size) -> void {
	if (support::avx && size == 4) {
		_mm256_storeu_pd(result, _mm256_min_pd(_mm256_loadu_pd(lhs), _mm256_loadu_pd(rhs)));
	}
	else if (support::sse2 && size == 2) {
		_mm_storeu_pd(result, _mm_min_pd(_mm_loadu_pd(lhs), _mm_loadu_pd(rhs)));
	}
}

auto gse::simd::max_i(const int* lhs, const int* rhs, int* result, const int size) -> void {
	if (support::avx2 && size == 8) {
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(result), _mm256_max_epi32(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs)), _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs))));
	}
	else if (support::sse41 && size == 4) {
		_mm_storeu_si128(reinterpret_cast<__m128i*>(result), _mm_max_epi32(_mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs)), _mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs))));
	}
}

auto gse::simd::max_f(const float* lhs, const float* rhs, float* result, const int size) -> void {
	if (support::avx && size == 8) {
		_mm256_storeu_ps(result, _mm256_max_ps(_mm256_loadu_ps(lhs), _mm256_loadu_ps(rhs)));
	}
	else if (support::sse && size == 4) {
		_mm_storeu_ps(result, _mm_max_ps(_mm_loadu_ps(lhs), _mm_loadu_ps(rhs)));
	}
}

auto gse::simd::max_d(const double* lhs, const double* rhs, double* result, const int size) -> void {
	if (support::avx && size == 4) {
		_mm256_storeu_pd(result, _mm256_max_pd(_mm256_loadu_pd(lhs), _mm256_loadu_pd(rhs)));
	}
	else if (support::sse2 && size == 2) {
		_mm_storeu_pd(result, _mm_max_pd(_mm_loadu_pd(lhs), _mm_loadu_pd(rhs)));
	}
}

auto gse::simd::mul_mat4(const float* lhs, const float* rhs, float* result) -> void {
	if (support::sse) {
		const __m128 a0 = _mm_loadu_ps(lhs);
		const __m128 a1 = _mm_loadu_ps(lhs + 4);
		const __m128 a2 = _mm_loadu_ps(lhs + 8);
		const __m128 a3 = _mm_loadu_ps(lhs + 12);

		for (int col = 0; col < 4; ++col) {
			const float* b_col = rhs + col * 4;

			__m128 result_col = _mm_mul_ps(a0, _mm_set1_ps(b_col[0]));
			result_col = _mm_add_ps(result_col, _mm_mul_ps(a1, _mm_set1_ps(b_col[1])));
			result_col = _mm_add_ps(result_col, _mm_mul_ps(a2, _mm_set1_ps(b_col[2])));
			result_col = _mm_add_ps(result_col, _mm_mul_ps(a3, _mm_set1_ps(b_col[3])));

			_mm_storeu_ps(result + col * 4, result_col);
		}
	}
	else {
		for (int col = 0; col < 4; ++col) {
			for (int row = 0; row < 4; ++row) {
				float sum = 0.0f;
				for (int k = 0; k < 4; ++k) {
					sum += lhs[k * 4 + row] * rhs[col * 4 + k];
				}
				result[col * 4 + row] = sum;
			}
		}
	}
}