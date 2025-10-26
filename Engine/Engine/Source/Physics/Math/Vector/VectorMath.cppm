export module gse.physics.math:vector_math;

import std;

import :vector;
import :units;

export namespace gse {
	template <is_vec V1, is_vec V2> requires (V1::extent == V2::extent)
	constexpr auto dot(
		const V1& lhs,
		const V2& rhs
	);

	template <is_vec V>
	constexpr auto magnitude(
		const V& v
	);

	template <is_vec V>
	constexpr auto normalize(
		const V& v
	);

	template <is_vec V1, is_vec V2> requires (V1::extent == V2::extent)
	constexpr auto distance(
		const V1& a,
		const V2& b
	);

	template <is_vec V1, is_vec V2> requires (V1::extent == 3 && V2::extent == 3)
	constexpr auto cross(
		const V1& lhs,
		const V2& rhs
	);

	template <is_vec V1, is_vec V2> requires (V1::extent == V2::extent)
	constexpr auto project(
		const V1& a,
		const V2& b
	);

	template <is_vec V1, is_vec V2> requires (V1::extent == V2::extent)
	constexpr auto reflect(
		const V1& v,
		const V2& n
	);

	template <is_vec V1, is_vec V2> requires (V1::extent == V2::extent)
	constexpr auto angle_between(
		const V1& a,
		const V2& b
	) -> V1::storage_type;

	template <is_vec V1, is_vec V2> requires (V1::extent == V2::extent)
	constexpr auto lerp(
		const V1& a,
		const V2& b,
		typename V1::storage_type t
	);

	template <is_vec V>
	constexpr auto is_zero(
		const V& v
	) -> bool;

	template <is_vec V>
	constexpr auto is_zero(
		const V& v,
		typename V::storage_type epsilon
	) -> bool;

	template <typename T, std::size_t N>
	constexpr auto abs(
		const unitless::vec_t<T, N>& v
	) -> unitless::vec_t<T, N>;

	template <is_vec V1, is_vec V2> requires (V1::extent == V2::extent)
	constexpr auto min(
		const V1& a,
		const V2& b
	);

	template <is_vec V1, is_vec V2> requires (V1::extent == V2::extent)
	constexpr auto max(
		const V1& a,
		const V2& b
	);

	template <typename T, std::size_t N>
	constexpr auto clamp(
		const unitless::vec_t<T, N>& v,
		const unitless::vec_t<T, N>& min_v,
		const unitless::vec_t<T, N>& max_v
	) -> unitless::vec_t<T, N>;

	template <typename T, std::size_t N>
	constexpr auto min(
		const unitless::vec_t<T, N>& v,
		T s
	) -> unitless::vec_t<T, N>;

	template <typename T, std::size_t N>
	constexpr auto max(
		const unitless::vec_t<T, N>& v,
		T s
	) -> unitless::vec_t<T, N>;

	template <typename T, std::size_t N>
	constexpr auto hadamard_product(
		const unitless::vec_t<T, N>& a,
		const unitless::vec_t<T, N>& b
	) -> unitless::vec_t<T, N>;

	template <typename T, std::size_t N>
	constexpr auto approx_equal(
		const unitless::vec_t<T, N>& a,
		const unitless::vec_t<T, N>& b,
		T epsilon
	) -> bool;

	template <typename T, std::size_t N>
	constexpr auto reflect_across(
		const unitless::vec_t<T, N>& a,
		const unitless::vec_t<T, N>& b
	) -> unitless::vec_t<T, N>;

	template <typename T, std::size_t N>
	constexpr auto sin(
		const unitless::vec_t<T, N>& v
	) -> unitless::vec_t<T, N>;

	template <typename T, std::size_t N>
	constexpr auto cos(
		const unitless::vec_t<T, N>& v
	) -> unitless::vec_t<T, N>;

	template <typename T, std::size_t N>
	constexpr auto exp(
		const unitless::vec_t<T, N>& v
	) -> unitless::vec_t<T, N>;

	template <typename T, std::size_t N>
	constexpr auto log(
		const unitless::vec_t<T, N>& v
	) -> unitless::vec_t<T, N>;

	template <typename T, std::size_t N>
	constexpr auto pow(
		const unitless::vec_t<T, N>& v,
		T exponent
	) -> unitless::vec_t<T, N>;

	template <typename T, std::size_t N, typename S>
	constexpr auto pow(
		const vec_t<T, N>& v,
		S exponent
	) -> vec_t<decltype(T{} * T{}), N > ;

	template <is_vec V1, is_vec V2> requires (V1::extent == V2::extent)
	constexpr auto epsilon_equal_index(
		const V1& a,
		const V2& b,
		std::size_t index
	) -> bool;

	template <typename T, int N>
	constexpr auto rotate(
		const unitless::vec_t<T, N>& v,
		angle_t<T> angle,
		std::size_t i = 0,
		std::size_t j = 1
	) -> unitless::vec_t<T, N>;

	template <typename T>
	constexpr auto barycentric(
		const unitless::vec3_t<T>& p,
		const unitless::vec3_t<T>& a,
		const unitless::vec3_t<T>& b,
		const unitless::vec3_t<T>& c
	) -> unitless::vec3_t<T>;

	template <typename T>
	constexpr auto barycentric(
		const vec3<length_t<T>>& p,
		const vec3<length_t<T>>& a,
		const vec3<length_t<T>>& b,
		const vec3<length_t<T>>& c
	) -> unitless::vec3_t<T>;
}

template <gse::is_vec V1, gse::is_vec V2> requires (V1::extent == V2::extent)
constexpr auto gse::dot(const V1& lhs, const V2& rhs) {
	using t1 = V1::value_type;
	using t2 = V2::value_type;
	using result_type = decltype(std::declval<t1>() * std::declval<t2>());
	using storage_type = internal::vec_storage_type_t<result_type>;

	storage_type sum{};
	simd::dot(lhs.as_storage_span(), rhs.as_storage_span(), sum);

	if constexpr (gse::internal::is_arithmetic_wrapper<result_type>) {
		return result_type(sum);
	} else {
		return sum;
	}
}

template <gse::is_vec V>
constexpr auto gse::magnitude(const V& v) {
	using storage_type = V::storage_type;
	auto d = dot(v, v);

	const auto d_val = [&] {
		if constexpr (internal::is_arithmetic_wrapper<decltype(d)>) {
			using dot_storage_t = internal::vec_storage_type_t<decltype(d)>;
			return *reinterpret_cast<const dot_storage_t*>(&d);
		} else {
			return d;
		}
	}();

	storage_type m = std::sqrt(d_val);

	if constexpr (gse::internal::is_arithmetic_wrapper<typename V::value_type>) {
		return typename V::value_type(m);
	} else {
		return m;
	}
}

template <gse::is_vec V>
constexpr auto gse::normalize(const V& v) {
	using storage_type = V::storage_type;
	auto mag = magnitude(v);

	const auto m = [&] {
		if constexpr (gse::internal::is_arithmetic_wrapper<decltype(mag)>) {
			return *reinterpret_cast<const storage_type*>(&mag);
		} else {
			return mag;
		}
	}();

	unitless::vec_t<storage_type, V::extent> out{};
	if (m == storage_type(0)) return out;
	simd::div_s(v.as_storage_span(), m, out.as_storage_span());
	return out;
}

template <gse::is_vec V1, gse::is_vec V2> requires (V1::extent == V2::extent)
constexpr auto gse::distance(const V1& a, const V2& b) {
	return magnitude(a - b);
}

template <gse::is_vec V1, gse::is_vec V2> requires (V1::extent == 3 && V2::extent == 3)
constexpr auto gse::cross(const V1& lhs, const V2& rhs) {
	using T1 = V1::value_type;
	using T2 = V2::value_type;
	using result_type = decltype(std::declval<T1>() * std::declval<T2>());
	using result_vec = vector_type_for_element_t<result_type, 3>;

	result_vec out{};
	auto out_span = out.as_storage_span();
	auto lhs_span = lhs.as_storage_span();
	auto rhs_span = rhs.as_storage_span();

	out_span[0] = lhs_span[1] * rhs_span[2] - lhs_span[2] * rhs_span[1];
	out_span[1] = lhs_span[2] * rhs_span[0] - lhs_span[0] * rhs_span[2];
	out_span[2] = lhs_span[0] * rhs_span[1] - lhs_span[1] * rhs_span[0];

	return out;
}

template <gse::is_vec V1, gse::is_vec V2> requires (V1::extent == V2::extent)
constexpr auto gse::project(const V1& a, const V2& b) {
	auto bb = dot(b, b);
	const auto bb_val = [&] {
		if constexpr (internal::is_arithmetic_wrapper<decltype(bb)>) {
			return *reinterpret_cast<const decltype(bb)::value_type*>(&bb);
		} else {
			return bb;
		}
	}();

	if (bb_val == 0) return V2{};

	auto ab = dot(a, b);
	const auto ab_val = [&] {
		if constexpr (internal::is_arithmetic_wrapper<decltype(ab)>) {
			return *reinterpret_cast<const decltype(ab)::value_type*>(&ab);
		} else {
			return ab;
		}
	}();

	return b * (ab_val / bb_val);
}

template <gse::is_vec V1, gse::is_vec V2> requires (V1::extent == V2::extent)
constexpr auto gse::reflect(const V1& v, const V2& n) {
	auto d = dot(v, n);
	const auto d_val = [&] {
		if constexpr (internal::is_arithmetic_wrapper<decltype(d)>) {
			return *reinterpret_cast<const decltype(d)::value_type*>(&d);
		} else {
			return d;
		}
	}();

	return v - n * (2 * d_val);
}

template <gse::is_vec V1, gse::is_vec V2> requires (V1::extent == V2::extent)
constexpr auto gse::angle_between(const V1& a, const V2& b) -> V1::storage_type {
	using storage_type = V1::storage_type;
	auto ma = magnitude(a);
	auto mb = magnitude(b);

	const auto ma_val = [&] {
		if constexpr (internal::is_arithmetic_wrapper<decltype(ma)>) {
			return *reinterpret_cast<const storage_type*>(&ma);
		} else {
			return ma;
		}
	}();

	const auto mb_val = [&] {
		if constexpr (internal::is_arithmetic_wrapper<decltype(mb)>) {
			return *reinterpret_cast<const storage_type*>(&mb);
		} else {
			return mb;
		}
	}();

	if (ma_val == 0 || mb_val == 0) return 0;

	auto d = dot(a, b);
	const auto d_val = [&] {
		if constexpr (internal::is_arithmetic_wrapper<decltype(d)>) {
			return *reinterpret_cast<const decltype(d)::value_type*>(&d);
		} else {
			return d;
		}
	}();

	auto cosine = std::clamp(d_val / (ma_val * mb_val), storage_type(-1), storage_type(1));
	return std::acos(cosine);
}

template <gse::is_vec V1, gse::is_vec V2> requires (V1::extent == V2::extent)
constexpr auto gse::lerp(const V1& a, const V2& b, typename V1::storage_type t) {
	return a + (b - a) * t;
}

template <gse::is_vec V>
constexpr auto gse::is_zero(const V& v) -> bool {
	for (const auto& elem : v.as_storage_span()) {
		if (elem != 0) return false;
	}
	return true;
}

template <gse::is_vec V>
constexpr auto gse::is_zero(const V& v, typename V::storage_type epsilon) -> bool {
	for (const auto& elem : v.as_storage_span()) {
		if (std::abs(elem) > epsilon) return false;
	}
	return true;
}

template <typename T, std::size_t N>
constexpr auto gse::abs(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> out{};
	simd::abs(v.as_storage_span(), out.as_storage_span());
	return out;
}

template <gse::is_vec V1, gse::is_vec V2> requires (V1::extent == V2::extent)
constexpr auto gse::min(const V1& a, const V2& b) {
	using common_elem = std::common_type_t<typename V1::value_type, typename V2::value_type>;
	using Out = vector_type_for_element_t<common_elem, V1::extent>;

	Out aa{ a };
	Out bb{ b };
	Out out{};
	simd::min(aa.as_storage_span(), bb.as_storage_span(), out.as_storage_span());
	return out;
}

template <gse::is_vec V1, gse::is_vec V2> requires (V1::extent == V2::extent)
constexpr auto gse::max(const V1& a, const V2& b) {
	using common_elem = std::common_type_t<typename V1::value_type, typename V2::value_type>;
	using Out = vector_type_for_element_t<common_elem, V1::extent>;

	Out aa{ a };
	Out bb{ b };
	Out out{};
	simd::max(aa.as_storage_span(), bb.as_storage_span(), out.as_storage_span());
	return out;
}

template <typename T, std::size_t N>
constexpr auto gse::clamp(const unitless::vec_t<T, N>& v, const unitless::vec_t<T, N>& min_v, const unitless::vec_t<T, N>& max_v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> out{};
	simd::clamp(v.as_storage_span(), min_v.as_storage_span(), max_v.as_storage_span(), out.as_storage_span());
	return out;
}

template <typename T, std::size_t N>
constexpr auto gse::min(const unitless::vec_t<T, N>& v, T s) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> out{};
	simd::min_s(v.as_storage_span(), s, out.as_storage_span());
	return out;
}

template <typename T, std::size_t N>
constexpr auto gse::max(const unitless::vec_t<T, N>& v, T s) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> out{};
	simd::max_s(v.as_storage_span(), s, out.as_storage_span());
	return out;
}

template <typename T, std::size_t N>
constexpr auto gse::hadamard_product(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	return a * b;
}

template <typename T, std::size_t N>
constexpr auto gse::approx_equal(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b, T epsilon) -> bool {
	for (std::size_t i = 0; i < N; ++i) {
		if (std::abs(a[i] - b[i]) > epsilon) return false;
	}
	return true;
}

template <typename T, std::size_t N>
constexpr auto gse::reflect_across(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	return a - 2 * dot(a, b) * b;
}

template <typename T, std::size_t N>
constexpr auto gse::sin(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> out{};
	for (std::size_t i = 0; i < N; ++i) out[i] = std::sin(v[i]);
	return out;
}

template <typename T, std::size_t N>
constexpr auto gse::cos(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> out{};
	for (std::size_t i = 0; i < N; ++i) out[i] = std::cos(v[i]);
	return out;
}

template <typename T, std::size_t N>
constexpr auto gse::exp(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> out{};
	for (std::size_t i = 0; i < N; ++i) out[i] = std::exp(v[i]);
	return out;
}

template <typename T, std::size_t N>
constexpr auto gse::log(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> out{};
	for (std::size_t i = 0; i < N; ++i) out[i] = std::log(v[i]);
	return out;
}

template <typename T, std::size_t N>
constexpr auto gse::pow(const unitless::vec_t<T, N>& v, T exponent) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> out{};
	for (std::size_t i = 0; i < N; ++i) out[i] = std::pow(v[i], exponent);
	return out;
}

template <typename T, std::size_t N, typename S>
constexpr auto gse::pow(const vec_t<T, N>& v, S exponent) -> vec_t<decltype(T{} * T{}), N> {
	return pow(v.template as<typename T::default_unit>(), static_cast<T>(exponent));
}

template <gse::is_vec V1, gse::is_vec V2> requires (V1::extent == V2::extent)
constexpr auto gse::epsilon_equal_index(const V1& a, const V2& b, std::size_t index) -> bool {
	if (index >= V1::extent) {
		throw std::out_of_range("Index out of range in epsilon_equal_index");
	}

	using storage_type = V1::storage_type;

	const storage_type val_a = a.as_storage_span()[index];
	const storage_type val_b = b.as_storage_span()[index];
	const storage_type eps_val = std::numeric_limits<storage_type>::epsilon();

	return std::abs(val_a - val_b) < eps_val;
}

template <typename T, int N>
constexpr auto gse::rotate(const unitless::vec_t<T, N>& v, angle_t<T> angle, std::size_t i, std::size_t j) -> unitless::vec_t<T, N> {
	if (i >= static_cast<std::size_t>(N) || j >= static_cast<std::size_t>(N) || i == j) {
		return v;
	}

	unitless::vec_t<T, N> result = v;
	const T rad = angle.template as<radians>();

	const T cos_theta = std::cos(rad);
	const T sin_theta = std::sin(rad);

	const T vi = v[i];
	const T vj = v[j];

	result[i] = vi * cos_theta - vj * sin_theta;
	result[j] = vi * sin_theta + vj * cos_theta;

	return result;
}

template <typename T>
constexpr auto gse::barycentric(const unitless::vec3_t<T>& p, const unitless::vec3_t<T>& a, const unitless::vec3_t<T>& b, const unitless::vec3_t<T>& c) -> unitless::vec3_t<T> {
	unitless::vec_t<T, 3> v0 = b - a, v1 = c - a, v2 = p - a;
	T d00 = dot(v0, v0);
	T d01 = dot(v0, v1);
	T d11 = dot(v1, v1);
	T d20 = dot(v2, v0);
	T d21 = dot(v2, v1);
	T denom = d00 * d11 - d01 * d01;

	if (denom == T(0)) {
		return { T(0), T(0), T(0) };
	}

	T v = (d11 * d20 - d01 * d21) / denom;
	T w = (d00 * d21 - d01 * d20) / denom;
	T u = T(1) - v - w;

	return { u, v, w };
}

template <typename T>
constexpr auto gse::barycentric(const vec3<length_t<T>>& p, const vec3<length_t<T>>& a, const vec3<length_t<T>>& b, const vec3<length_t<T>>& c) -> unitless::vec3_t<T> {
	return barycentric(
		p.template as<typename length_t<T>::default_unit>(),
		a.template as<typename length_t<T>::default_unit>(),
		b.template as<typename length_t<T>::default_unit>(),
		c.template as<typename length_t<T>::default_unit>()
	);
}
