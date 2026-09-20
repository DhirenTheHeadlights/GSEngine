module;

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <type_traits>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <immintrin.h>
#endif

export module gse.math:simd;

template <typename T>
concept is_int32 = std::same_as<std::int32_t, std::remove_cvref_t<T>>;

template <typename T>
concept is_float = std::same_as<float, std::remove_cvref_t<T>>;

template <typename T>
concept is_double = std::same_as<double, std::remove_cvref_t<T>>;

template <typename T>
concept simd_val = is_int32<T> || is_float<T> || is_double<T>;

template <typename T>
concept span = requires {
	typename T::element_type;
	requires std::is_same_v<std::remove_cvref_t<T>, std::span<typename T::element_type, T::extent>>; };

export namespace gse::simd {
	auto add(
		const span auto& lhs,
		const span auto& rhs,
		span auto result
	) -> void;

	auto sub(
		const span auto& lhs,
		const span auto& rhs,
		span auto result
	) -> void;

	auto mul(
		const span auto& lhs,
		const span auto& rhs,
		span auto result
	) -> void;

	auto div(
		const span auto& lhs,
		const span auto& rhs,
		span auto result
	) -> void;

	auto dot(
		const span auto& lhs,
		const span auto& rhs,
		simd_val auto& result
	) -> void;

	auto abs(
		const span auto& v,
		span auto result
	) -> void;

	auto min(
		const span auto& lhs,
		const span auto& rhs,
		span auto result
	) -> void;

	auto max(
		const span auto& lhs,
		const span auto& rhs,
		span auto result
	) -> void;

	auto clamp(
		const span auto& v,
		const span auto& min_v,
		const span auto& max_v,
		span auto result
	) -> void;

	auto mul_mat4(
		const float* lhs,
		const float* rhs,
		float* result
	) -> void;

	auto add_s(
		const span auto& lhs,
		const simd_val auto& scalar,
		span auto result
	) -> void;

	auto sub_s(
		const span auto& lhs,
		const simd_val auto& scalar,
		span auto result
	) -> void;

	auto mul_s(
		const span auto& lhs,
		const simd_val auto& scalar,
		span auto result
	) -> void;

	auto div_s(
		const span auto& lhs,
		const simd_val auto& scalar,
		span auto result
	) -> void;

	auto min_s(
		const span auto& lhs,
		const simd_val auto& scalar,
		span auto result
	) -> void;

	auto max_s(
		const span auto& lhs,
		const simd_val auto& scalar,
		span auto result
	) -> void;
}

auto gse::simd::add(const span auto& lhs, const span auto& rhs, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0] + rhs[0])>;
	constexpr std::size_t size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (is_float<type> && size == 8) {
		_mm256_storeu_ps(result.data(), _mm256_add_ps(_mm256_loadu_ps(lhs.data()), _mm256_loadu_ps(rhs.data())));
	}
	else if constexpr (is_float<type> && size == 4) {
		_mm_storeu_ps(result.data(), _mm_add_ps(_mm_loadu_ps(lhs.data()), _mm_loadu_ps(rhs.data())));
	}
	else if constexpr (is_double<type> && size == 4) {
		_mm256_storeu_pd(result.data(), _mm256_add_pd(_mm256_loadu_pd(lhs.data()), _mm256_loadu_pd(rhs.data())));
	}
	else if constexpr (is_double<type> && size == 2) {
		_mm_storeu_pd(result.data(), _mm_add_pd(_mm_loadu_pd(lhs.data()), _mm_loadu_pd(rhs.data())));
	}
	else if constexpr (is_int32<type> && size == 8) {
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(result.data()),
			_mm256_add_epi32(
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data())),
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.data()))
			)
		);
	}
	else if constexpr (is_int32<type> && size == 4) {
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(result.data()),
			_mm_add_epi32(
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data())),
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs.data()))
			)
		);
	}
	else {
		for (std::size_t i = 0; i < size; ++i) {
			result[i] = lhs[i] + rhs[i];
		}
	}
}

auto gse::simd::sub(const span auto& lhs, const span auto& rhs, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0] - rhs[0])>;
	constexpr std::size_t size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (is_float<type> && size == 8) {
		_mm256_storeu_ps(result.data(), _mm256_sub_ps(_mm256_loadu_ps(lhs.data()), _mm256_loadu_ps(rhs.data())));
	}
	else if constexpr (is_float<type> && size == 4) {
		_mm_storeu_ps(result.data(), _mm_sub_ps(_mm_loadu_ps(lhs.data()), _mm_loadu_ps(rhs.data())));
	}
	else if constexpr (is_double<type> && size == 4) {
		_mm256_storeu_pd(result.data(), _mm256_sub_pd(_mm256_loadu_pd(lhs.data()), _mm256_loadu_pd(rhs.data())));
	}
	else if constexpr (is_double<type> && size == 2) {
		_mm_storeu_pd(result.data(), _mm_sub_pd(_mm_loadu_pd(lhs.data()), _mm_loadu_pd(rhs.data())));
	}
	else if constexpr (is_int32<type> && size == 8) {
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(result.data()),
			_mm256_sub_epi32(
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data())),
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.data()))
			)
		);
	}
	else if constexpr (is_int32<type> && size == 4) {
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(result.data()),
			_mm_sub_epi32(
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data())),
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs.data()))
			)
		);
	}
	else {
		for (std::size_t i = 0; i < size; ++i) {
			result[i] = lhs[i] - rhs[i];
		}
	}
}

auto gse::simd::mul(const span auto& lhs, const span auto& rhs, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0] * rhs[0])>;
	constexpr std::size_t size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (is_float<type> && size == 8) {
		_mm256_storeu_ps(result.data(), _mm256_mul_ps(_mm256_loadu_ps(lhs.data()), _mm256_loadu_ps(rhs.data())));
	}
	else if constexpr (is_float<type> && size == 4) {
		_mm_storeu_ps(result.data(), _mm_mul_ps(_mm_loadu_ps(lhs.data()), _mm_loadu_ps(rhs.data())));
	}
	else if constexpr (is_double<type> && size == 4) {
		_mm256_storeu_pd(result.data(), _mm256_mul_pd(_mm256_loadu_pd(lhs.data()), _mm256_loadu_pd(rhs.data())));
	}
	else if constexpr (is_double<type> && size == 2) {
		_mm_storeu_pd(result.data(), _mm_mul_pd(_mm_loadu_pd(lhs.data()), _mm_loadu_pd(rhs.data())));
	}
	else if constexpr (is_int32<type> && size == 8) {
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(result.data()),
			_mm256_mullo_epi32(
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data())),
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.data()))
			)
		);
	}
	else if constexpr (is_int32<type> && size == 4) {
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(result.data()),
			_mm_mullo_epi32(
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data())),
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs.data()))
			)
		);
	}
	else {
		for (std::size_t i = 0; i < size; ++i) {
			result[i] = lhs[i] * rhs[i];
		}
	}
}

auto gse::simd::div(const span auto& lhs, const span auto& rhs, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0] / rhs[0])>;
	constexpr std::size_t size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (is_float<type> && size == 8) {
		_mm256_storeu_ps(result.data(), _mm256_div_ps(_mm256_loadu_ps(lhs.data()), _mm256_loadu_ps(rhs.data())));
	}
	else if constexpr (is_float<type> && size == 4) {
		_mm_storeu_ps(result.data(), _mm_div_ps(_mm_loadu_ps(lhs.data()), _mm_loadu_ps(rhs.data())));
	}
	else if constexpr (is_double<type> && size == 4) {
		_mm256_storeu_pd(result.data(), _mm256_div_pd(_mm256_loadu_pd(lhs.data()), _mm256_loadu_pd(rhs.data())));
	}
	else if constexpr (is_double<type> && size == 2) {
		_mm_storeu_pd(result.data(), _mm_div_pd(_mm_loadu_pd(lhs.data()), _mm_loadu_pd(rhs.data())));
	}
	else {
		for (std::size_t i = 0; i < size; ++i) {
			result[i] = lhs[i] / rhs[i];
		}
	}
}

auto gse::simd::dot(const span auto& lhs, const span auto& rhs, simd_val auto& result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0] * rhs[0])>;
	constexpr std::size_t size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (is_float<type> && size == 8) {
		const __m256 product = _mm256_mul_ps(_mm256_loadu_ps(lhs.data()), _mm256_loadu_ps(rhs.data()));
		__m128 sum = _mm_add_ps(_mm256_castps256_ps128(product), _mm256_extractf128_ps(product, 1));
		sum = _mm_hadd_ps(sum, sum);
		sum = _mm_hadd_ps(sum, sum);
		result = _mm_cvtss_f32(sum);
	}
	else if constexpr (is_float<type> && size == 4) {
		result = _mm_cvtss_f32(_mm_dp_ps(_mm_loadu_ps(lhs.data()), _mm_loadu_ps(rhs.data()), 0xF1));
	}
	else if constexpr (is_double<type> && size == 4) {
		const __m256d product = _mm256_mul_pd(_mm256_loadu_pd(lhs.data()), _mm256_loadu_pd(rhs.data()));
		__m128d sum = _mm_add_pd(_mm256_castpd256_pd128(product), _mm256_extractf128_pd(product, 1));
		sum = _mm_hadd_pd(sum, sum);
		result = _mm_cvtsd_f64(sum);
	}
	else if constexpr (is_double<type> && size == 2) {
		const __m128d product = _mm_mul_pd(_mm_loadu_pd(lhs.data()), _mm_loadu_pd(rhs.data()));
		result = _mm_cvtsd_f64(_mm_hadd_pd(product, product));
	}
	else if constexpr (is_int32<type> && size == 8) {
		const __m256i product = _mm256_mullo_epi32(
			_mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data())),
			_mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.data()))
		);
		__m128i sum = _mm_add_epi32(_mm256_castsi256_si128(product), _mm256_extracti128_si256(product, 1));
		sum = _mm_hadd_epi32(sum, sum);
		sum = _mm_hadd_epi32(sum, sum);
		result = _mm_cvtsi128_si32(sum);
	}
	else if constexpr (is_int32<type> && size == 4) {
		const __m128i product = _mm_mullo_epi32(
			_mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data())),
			_mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs.data()))
		);
		__m128i sum = _mm_hadd_epi32(product, product);
		sum = _mm_hadd_epi32(sum, sum);
		result = _mm_cvtsi128_si32(sum);
	}
	else {
		result = 0;
		for (std::size_t i = 0; i < size; ++i) {
			result += lhs[i] * rhs[i];
		}
	}
}

auto gse::simd::abs(const span auto& v, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(v[0])>;
	constexpr std::size_t size = std::decay_t<decltype(v)>::extent;

	if constexpr (is_float<type> && size == 8) {
		const __m256 mask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFF));
		_mm256_storeu_ps(result.data(), _mm256_and_ps(_mm256_loadu_ps(v.data()), mask));
	}
	else if constexpr (is_float<type> && size == 4) {
		const __m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
		_mm_storeu_ps(result.data(), _mm_and_ps(_mm_loadu_ps(v.data()), mask));
	}
	else if constexpr (is_double<type> && size == 4) {
		const __m256d mask = _mm256_castsi256_pd(_mm256_set1_epi64x(0x7FFFFFFFFFFFFFFF));
		_mm256_storeu_pd(result.data(), _mm256_and_pd(_mm256_loadu_pd(v.data()), mask));
	}
	else if constexpr (is_double<type> && size == 2) {
		const __m128d mask = _mm_castsi128_pd(_mm_set1_epi64x(0x7FFFFFFFFFFFFFFF));
		_mm_storeu_pd(result.data(), _mm_and_pd(_mm_loadu_pd(v.data()), mask));
	}
	else if constexpr (is_int32<type> && size == 8) {
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(result.data()),
			_mm256_abs_epi32(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(v.data())))
		);
	}
	else if constexpr (is_int32<type> && size == 4) {
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(result.data()),
			_mm_abs_epi32(_mm_loadu_si128(reinterpret_cast<const __m128i*>(v.data())))
		);
	}
	else {
		for (std::size_t i = 0; i < size; ++i) {
			result[i] = std::abs(v[i]);
		}
	}
}

auto gse::simd::min(const span auto& lhs, const span auto& rhs, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;
	constexpr std::size_t size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (is_float<type> && size == 8) {
		_mm256_storeu_ps(result.data(), _mm256_min_ps(_mm256_loadu_ps(lhs.data()), _mm256_loadu_ps(rhs.data())));
	}
	else if constexpr (is_float<type> && size == 4) {
		_mm_storeu_ps(result.data(), _mm_min_ps(_mm_loadu_ps(lhs.data()), _mm_loadu_ps(rhs.data())));
	}
	else if constexpr (is_double<type> && size == 4) {
		_mm256_storeu_pd(result.data(), _mm256_min_pd(_mm256_loadu_pd(lhs.data()), _mm256_loadu_pd(rhs.data())));
	}
	else if constexpr (is_double<type> && size == 2) {
		_mm_storeu_pd(result.data(), _mm_min_pd(_mm_loadu_pd(lhs.data()), _mm_loadu_pd(rhs.data())));
	}
	else if constexpr (is_int32<type> && size == 8) {
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(result.data()),
			_mm256_min_epi32(
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data())),
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.data()))
			)
		);
	}
	else if constexpr (is_int32<type> && size == 4) {
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(result.data()),
			_mm_min_epi32(
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data())),
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs.data()))
			)
		);
	}
	else {
		for (std::size_t i = 0; i < size; ++i) {
			result[i] = std::min(lhs[i], rhs[i]);
		}
	}
}

auto gse::simd::max(const span auto& lhs, const span auto& rhs, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;
	constexpr std::size_t size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (is_float<type> && size == 8) {
		_mm256_storeu_ps(result.data(), _mm256_max_ps(_mm256_loadu_ps(lhs.data()), _mm256_loadu_ps(rhs.data())));
	}
	else if constexpr (is_float<type> && size == 4) {
		_mm_storeu_ps(result.data(), _mm_max_ps(_mm_loadu_ps(lhs.data()), _mm_loadu_ps(rhs.data())));
	}
	else if constexpr (is_double<type> && size == 4) {
		_mm256_storeu_pd(result.data(), _mm256_max_pd(_mm256_loadu_pd(lhs.data()), _mm256_loadu_pd(rhs.data())));
	}
	else if constexpr (is_double<type> && size == 2) {
		_mm_storeu_pd(result.data(), _mm_max_pd(_mm_loadu_pd(lhs.data()), _mm_loadu_pd(rhs.data())));
	}
	else if constexpr (is_int32<type> && size == 8) {
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(result.data()),
			_mm256_max_epi32(
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data())),
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.data()))
			)
		);
	}
	else if constexpr (is_int32<type> && size == 4) {
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(result.data()),
			_mm_max_epi32(
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data())),
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(rhs.data()))
			)
		);
	}
	else {
		for (std::size_t i = 0; i < size; ++i) {
			result[i] = std::max(lhs[i], rhs[i]);
		}
	}
}

auto gse::simd::clamp(const span auto& v, const span auto& min_v, const span auto& max_v, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(v[0])>;
	constexpr std::size_t size = std::decay_t<decltype(v)>::extent;

	if constexpr (is_float<type> && size == 8) {
		const __m256 upper = _mm256_min_ps(_mm256_loadu_ps(v.data()), _mm256_loadu_ps(max_v.data()));
		_mm256_storeu_ps(result.data(), _mm256_max_ps(upper, _mm256_loadu_ps(min_v.data())));
	}
	else if constexpr (is_float<type> && size == 4) {
		const __m128 upper = _mm_min_ps(_mm_loadu_ps(v.data()), _mm_loadu_ps(max_v.data()));
		_mm_storeu_ps(result.data(), _mm_max_ps(upper, _mm_loadu_ps(min_v.data())));
	}
	else if constexpr (is_double<type> && size == 4) {
		const __m256d upper = _mm256_min_pd(_mm256_loadu_pd(v.data()), _mm256_loadu_pd(max_v.data()));
		_mm256_storeu_pd(result.data(), _mm256_max_pd(upper, _mm256_loadu_pd(min_v.data())));
	}
	else if constexpr (is_double<type> && size == 2) {
		const __m128d upper = _mm_min_pd(_mm_loadu_pd(v.data()), _mm_loadu_pd(max_v.data()));
		_mm_storeu_pd(result.data(), _mm_max_pd(upper, _mm_loadu_pd(min_v.data())));
	}
	else if constexpr (is_int32<type> && size == 8) {
		const __m256i upper = _mm256_min_epi32(
			_mm256_loadu_si256(reinterpret_cast<const __m256i*>(v.data())),
			_mm256_loadu_si256(reinterpret_cast<const __m256i*>(max_v.data()))
		);
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(result.data()),
			_mm256_max_epi32(upper, _mm256_loadu_si256(reinterpret_cast<const __m256i*>(min_v.data())))
		);
	}
	else if constexpr (is_int32<type> && size == 4) {
		const __m128i upper = _mm_min_epi32(
			_mm_loadu_si128(reinterpret_cast<const __m128i*>(v.data())),
			_mm_loadu_si128(reinterpret_cast<const __m128i*>(max_v.data()))
		);
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(result.data()),
			_mm_max_epi32(upper, _mm_loadu_si128(reinterpret_cast<const __m128i*>(min_v.data())))
		);
	}
	else {
		for (std::size_t i = 0; i < size; ++i) {
			result[i] = std::clamp(v[i], min_v[i], max_v[i]);
		}
	}
}

auto gse::simd::mul_mat4(const float* lhs, const float* rhs, float* result) -> void {
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

auto gse::simd::add_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;
	constexpr std::size_t size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (is_float<type> && size == 8) {
		_mm256_storeu_ps(result.data(), _mm256_add_ps(_mm256_loadu_ps(lhs.data()), _mm256_set1_ps(scalar)));
	}
	else if constexpr (is_float<type> && size == 4) {
		_mm_storeu_ps(result.data(), _mm_add_ps(_mm_loadu_ps(lhs.data()), _mm_set1_ps(scalar)));
	}
	else if constexpr (is_double<type> && size == 4) {
		_mm256_storeu_pd(result.data(), _mm256_add_pd(_mm256_loadu_pd(lhs.data()), _mm256_set1_pd(scalar)));
	}
	else if constexpr (is_double<type> && size == 2) {
		_mm_storeu_pd(result.data(), _mm_add_pd(_mm_loadu_pd(lhs.data()), _mm_set1_pd(scalar)));
	}
	else if constexpr (is_int32<type> && size == 8) {
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(result.data()),
			_mm256_add_epi32(
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data())),
				_mm256_set1_epi32(scalar)
			)
		);
	}
	else if constexpr (is_int32<type> && size == 4) {
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(result.data()),
			_mm_add_epi32(
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data())),
				_mm_set1_epi32(scalar)
			)
		);
	}
	else {
		for (std::size_t i = 0; i < size; ++i) {
			result[i] = lhs[i] + scalar;
		}
	}
}

auto gse::simd::sub_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;
	constexpr std::size_t size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (is_float<type> && size == 8) {
		_mm256_storeu_ps(result.data(), _mm256_sub_ps(_mm256_loadu_ps(lhs.data()), _mm256_set1_ps(scalar)));
	}
	else if constexpr (is_float<type> && size == 4) {
		_mm_storeu_ps(result.data(), _mm_sub_ps(_mm_loadu_ps(lhs.data()), _mm_set1_ps(scalar)));
	}
	else if constexpr (is_double<type> && size == 4) {
		_mm256_storeu_pd(result.data(), _mm256_sub_pd(_mm256_loadu_pd(lhs.data()), _mm256_set1_pd(scalar)));
	}
	else if constexpr (is_double<type> && size == 2) {
		_mm_storeu_pd(result.data(), _mm_sub_pd(_mm_loadu_pd(lhs.data()), _mm_set1_pd(scalar)));
	}
	else if constexpr (is_int32<type> && size == 8) {
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(result.data()),
			_mm256_sub_epi32(
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data())),
				_mm256_set1_epi32(scalar)
			)
		);
	}
	else if constexpr (is_int32<type> && size == 4) {
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(result.data()),
			_mm_sub_epi32(
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data())),
				_mm_set1_epi32(scalar)
			)
		);
	}
	else {
		for (std::size_t i = 0; i < size; ++i) {
			result[i] = lhs[i] - scalar;
		}
	}
}

auto gse::simd::mul_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;
	constexpr std::size_t size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (is_float<type> && size == 8) {
		_mm256_storeu_ps(result.data(), _mm256_mul_ps(_mm256_loadu_ps(lhs.data()), _mm256_set1_ps(scalar)));
	}
	else if constexpr (is_float<type> && size == 4) {
		_mm_storeu_ps(result.data(), _mm_mul_ps(_mm_loadu_ps(lhs.data()), _mm_set1_ps(scalar)));
	}
	else if constexpr (is_double<type> && size == 4) {
		_mm256_storeu_pd(result.data(), _mm256_mul_pd(_mm256_loadu_pd(lhs.data()), _mm256_set1_pd(scalar)));
	}
	else if constexpr (is_double<type> && size == 2) {
		_mm_storeu_pd(result.data(), _mm_mul_pd(_mm_loadu_pd(lhs.data()), _mm_set1_pd(scalar)));
	}
	else if constexpr (is_int32<type> && size == 8) {
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(result.data()),
			_mm256_mullo_epi32(
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data())),
				_mm256_set1_epi32(scalar)
			)
		);
	}
	else if constexpr (is_int32<type> && size == 4) {
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(result.data()),
			_mm_mullo_epi32(
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data())),
				_mm_set1_epi32(scalar)
			)
		);
	}
	else {
		for (std::size_t i = 0; i < size; ++i) {
			result[i] = lhs[i] * scalar;
		}
	}
}

auto gse::simd::div_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;
	constexpr std::size_t size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (is_float<type> && size == 8) {
		_mm256_storeu_ps(result.data(), _mm256_div_ps(_mm256_loadu_ps(lhs.data()), _mm256_set1_ps(scalar)));
	}
	else if constexpr (is_float<type> && size == 4) {
		_mm_storeu_ps(result.data(), _mm_div_ps(_mm_loadu_ps(lhs.data()), _mm_set1_ps(scalar)));
	}
	else if constexpr (is_double<type> && size == 4) {
		_mm256_storeu_pd(result.data(), _mm256_div_pd(_mm256_loadu_pd(lhs.data()), _mm256_set1_pd(scalar)));
	}
	else if constexpr (is_double<type> && size == 2) {
		_mm_storeu_pd(result.data(), _mm_div_pd(_mm_loadu_pd(lhs.data()), _mm_set1_pd(scalar)));
	}
	else {
		for (std::size_t i = 0; i < size; ++i) {
			result[i] = lhs[i] / scalar;
		}
	}
}

auto gse::simd::min_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;
	constexpr std::size_t size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (is_float<type> && size == 8) {
		_mm256_storeu_ps(result.data(), _mm256_min_ps(_mm256_loadu_ps(lhs.data()), _mm256_set1_ps(scalar)));
	}
	else if constexpr (is_float<type> && size == 4) {
		_mm_storeu_ps(result.data(), _mm_min_ps(_mm_loadu_ps(lhs.data()), _mm_set1_ps(scalar)));
	}
	else if constexpr (is_double<type> && size == 4) {
		_mm256_storeu_pd(result.data(), _mm256_min_pd(_mm256_loadu_pd(lhs.data()), _mm256_set1_pd(scalar)));
	}
	else if constexpr (is_double<type> && size == 2) {
		_mm_storeu_pd(result.data(), _mm_min_pd(_mm_loadu_pd(lhs.data()), _mm_set1_pd(scalar)));
	}
	else if constexpr (is_int32<type> && size == 8) {
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(result.data()),
			_mm256_min_epi32(
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data())),
				_mm256_set1_epi32(scalar)
			)
		);
	}
	else if constexpr (is_int32<type> && size == 4) {
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(result.data()),
			_mm_min_epi32(
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data())),
				_mm_set1_epi32(scalar)
			)
		);
	}
	else {
		for (std::size_t i = 0; i < size; ++i) {
			result[i] = std::min(lhs[i], scalar);
		}
	}
}

auto gse::simd::max_s(const span auto& lhs, const simd_val auto& scalar, span auto result) -> void {
	using type = std::remove_cvref_t<decltype(lhs[0])>;
	constexpr std::size_t size = std::decay_t<decltype(lhs)>::extent;

	if constexpr (is_float<type> && size == 8) {
		_mm256_storeu_ps(result.data(), _mm256_max_ps(_mm256_loadu_ps(lhs.data()), _mm256_set1_ps(scalar)));
	}
	else if constexpr (is_float<type> && size == 4) {
		_mm_storeu_ps(result.data(), _mm_max_ps(_mm_loadu_ps(lhs.data()), _mm_set1_ps(scalar)));
	}
	else if constexpr (is_double<type> && size == 4) {
		_mm256_storeu_pd(result.data(), _mm256_max_pd(_mm256_loadu_pd(lhs.data()), _mm256_set1_pd(scalar)));
	}
	else if constexpr (is_double<type> && size == 2) {
		_mm_storeu_pd(result.data(), _mm_max_pd(_mm_loadu_pd(lhs.data()), _mm_set1_pd(scalar)));
	}
	else if constexpr (is_int32<type> && size == 8) {
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(result.data()),
			_mm256_max_epi32(
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.data())),
				_mm256_set1_epi32(scalar)
			)
		);
	}
	else if constexpr (is_int32<type> && size == 4) {
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(result.data()),
			_mm_max_epi32(
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(lhs.data())),
				_mm_set1_epi32(scalar)
			)
		);
	}
	else {
		for (std::size_t i = 0; i < size; ++i) {
			result[i] = std::max(lhs[i], scalar);
		}
	}
}
