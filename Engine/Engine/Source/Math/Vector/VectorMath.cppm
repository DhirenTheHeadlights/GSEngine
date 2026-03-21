export module gse.math:vector_math;

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
	) -> angle_t<typename V1::storage_type>;

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

	template <is_vec V>
	constexpr auto abs(
		const V& v
	) -> V;

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

	template <is_vec V>
	constexpr auto clamp(
		const V& v,
		const V& min_v,
		const V& max_v
	) -> V;

	template <is_vec V>
	constexpr auto min(
		const V& v,
		typename V::storage_type s
	) -> V;

	template <is_vec V>
	constexpr auto max(
		const V& v,
		typename V::storage_type s
	) -> V;

	template <is_vec V1, is_vec V2> requires (V1::extent == V2::extent)
	constexpr auto hadamard_product(
		const V1& a,
		const V2& b
	);

	template <is_vec V>
	constexpr auto approx_equal(
		const V& a,
		const V& b,
		typename V::storage_type epsilon
	) -> bool;

	template <is_vec V1, is_vec V2> requires (V1::extent == V2::extent)
	constexpr auto reflect_across(
		const V1& a,
		const V2& b
	);

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
	constexpr auto logarithm(
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

	template <is_vec V>
	constexpr auto rotate(
		const V& v,
		angle_t<typename V::storage_type> angle,
		std::size_t i = 0,
		std::size_t j = 1
	) -> V;

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
	return result_type(sum);
}

template <gse::is_vec V>
constexpr auto gse::magnitude(const V& v) {
	using storage_type = V::storage_type;
	auto d = dot(v, v);
	storage_type m = std::sqrt(internal::to_storage(d));
	return typename V::value_type(m);
}

template <gse::is_vec V>
constexpr auto gse::normalize(const V& v) {
	using storage_type = V::storage_type;
	auto mag = magnitude(v);
	const auto m = internal::to_storage(mag);

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
	vec::base<result_type, 3> out{};
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
	const auto bb_val = internal::to_storage(bb);
	if (bb_val == 0) return V2{};

	auto ab = dot(a, b);
	const auto ab_val = internal::to_storage(ab);
	return b * (ab_val / bb_val);
}

template <gse::is_vec V1, gse::is_vec V2> requires (V1::extent == V2::extent)
constexpr auto gse::reflect(const V1& v, const V2& n) {
	auto d = dot(v, n);
	return v - n * (2 * internal::to_storage(d));
}

template <gse::is_vec V1, gse::is_vec V2> requires (V1::extent == V2::extent)
constexpr auto gse::angle_between(const V1& a, const V2& b) -> angle_t<typename V1::storage_type> {
	using storage_type = V1::storage_type;
	const auto ma_val = internal::to_storage(magnitude(a));
	const auto mb_val = internal::to_storage(magnitude(b));
	if (ma_val == 0 || mb_val == 0) return angle_t<storage_type>(storage_type(0));

	const auto d_val = internal::to_storage(dot(a, b));
	auto cosine = std::clamp(d_val / (ma_val * mb_val), storage_type(-1), storage_type(1));
	return angle_t<storage_type>(std::acos(cosine));
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

template <gse::is_vec V>
constexpr auto gse::abs(const V& v) -> V {
	V out{};
	simd::abs(v.as_storage_span(), out.as_storage_span());
	return out;
}

template <gse::is_vec V1, gse::is_vec V2> requires (V1::extent == V2::extent)
constexpr auto gse::min(const V1& a, const V2& b) {
	using common_elem = internal::common_quantity_t<typename V1::value_type, typename V2::value_type>;
	vec::base<common_elem, V1::extent> aa{ a };
	vec::base<common_elem, V1::extent> bb{ b };
	vec::base<common_elem, V1::extent> out{};
	simd::min(aa.as_storage_span(), bb.as_storage_span(), out.as_storage_span());
	return out;
}

template <gse::is_vec V1, gse::is_vec V2> requires (V1::extent == V2::extent)
constexpr auto gse::max(const V1& a, const V2& b) {
	using common_elem = internal::common_quantity_t<typename V1::value_type, typename V2::value_type>;
	vec::base<common_elem, V1::extent> aa{ a };
	vec::base<common_elem, V1::extent> bb{ b };
	vec::base<common_elem, V1::extent> out{};
	simd::max(aa.as_storage_span(), bb.as_storage_span(), out.as_storage_span());
	return out;
}

template <gse::is_vec V>
constexpr auto gse::clamp(const V& v, const V& min_v, const V& max_v) -> V {
	V out{};
	simd::clamp(v.as_storage_span(), min_v.as_storage_span(), max_v.as_storage_span(), out.as_storage_span());
	return out;
}

template <gse::is_vec V>
constexpr auto gse::min(const V& v, typename V::storage_type s) -> V {
	V out{};
	simd::min_s(v.as_storage_span(), s, out.as_storage_span());
	return out;
}

template <gse::is_vec V>
constexpr auto gse::max(const V& v, typename V::storage_type s) -> V {
	V out{};
	simd::max_s(v.as_storage_span(), s, out.as_storage_span());
	return out;
}

template <gse::is_vec V1, gse::is_vec V2> requires (V1::extent == V2::extent)
constexpr auto gse::hadamard_product(const V1& a, const V2& b) {
	return a * b;
}

template <gse::is_vec V>
constexpr auto gse::approx_equal(const V& a, const V& b, typename V::storage_type epsilon) -> bool {
	auto a_span = a.as_storage_span();
	auto b_span = b.as_storage_span();
	for (std::size_t i = 0; i < V::extent; ++i) {
		if (std::abs(a_span[i] - b_span[i]) > epsilon) return false;
	}
	return true;
}

template <gse::is_vec V1, gse::is_vec V2> requires (V1::extent == V2::extent)
constexpr auto gse::reflect_across(const V1& a, const V2& b) {
	return a - 2 * internal::to_storage(dot(a, b)) * b;
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
constexpr auto gse::logarithm(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
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

template <gse::is_vec V>
constexpr auto gse::rotate(const V& v, angle_t<typename V::storage_type> angle, std::size_t i, std::size_t j) -> V {
	using storage_type = V::storage_type;
	if (i >= V::extent || j >= V::extent || i == j) {
		return v;
	}

	V result = v;
	const storage_type rad = angle.template as<radians>();

	const storage_type cos_theta = std::cos(rad);
	const storage_type sin_theta = std::sin(rad);

	auto v_span = v.as_storage_span();
	auto r_span = result.as_storage_span();

	r_span[i] = v_span[i] * cos_theta - v_span[j] * sin_theta;
	r_span[j] = v_span[i] * sin_theta + v_span[j] * cos_theta;

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
