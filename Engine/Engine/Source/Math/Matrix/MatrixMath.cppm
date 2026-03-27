export module gse.math:matrix_math;

import std;

import :vector;
import :vector_math;
import :units;
import :matrix;
import :quat;
import :dimension;
import :quat_math;
import :simd;

export namespace gse {
	template <typename T>
	constexpr auto look_at(
		const vec3<length_t<T>>& position,
		const vec3<length_t<T>>& target,
		const vec3<T>& up
	) -> mat4<T>;

	template <typename T>
	constexpr auto perspective(
		angle_t<T> fov,
		T aspect,
		length_t<T> near,
		length_t<T> far
	) -> mat4<T>;

	template <typename T>
	constexpr auto orthographic(
		length_t<T> left,
		length_t<T> right,
		length_t<T> bottom,
		length_t<T> top,
		length_t<T> near,
		length_t<T> far
	) -> mat4<T>;

	template <typename T, internal::is_vec_element L>
	constexpr auto translate(
		const mat4<T>& matrix,
		const vec3<L>& translation
	) -> mat4<T>;

	template <typename T>
	constexpr auto rotate(
		const mat4<T>& matrix,
		axis axis,
		std::type_identity_t<angle_t<T>> angle
	) -> mat4<T>;

	template <typename T>
	constexpr auto scale(
		const mat4<T>& matrix,
		const vec3f& scale
	) -> mat4<T>;

	template <is_mat M>
	constexpr auto value_ptr(
		M& matrix
	) -> typename M::value_type*;

	template <is_mat M>
	constexpr auto value_ptr(
		const M& matrix
	) -> const typename M::value_type*;

	template <typename T, std::size_t N, std::size_t M>
	constexpr auto identity(
	) -> mat<T, N, M>;

	template <is_vec V1, is_vec V2>
		requires (std::same_as<typename V1::storage_type, typename V2::storage_type>)
	constexpr auto outer_product(
		const V1& a,
		const V2& b
	);
}

template <typename T>
constexpr auto gse::look_at(const vec3<length_t<T>>& position, const vec3<length_t<T>>& target, const vec3<T>& up) -> mat4<T> {
	const auto direction_axis = normalize(position - target);
	const auto right_axis = normalize(cross(up, direction_axis));
	const auto up_axis = cross(direction_axis, right_axis);

	auto ps = position.as_storage_span();
	vec3<T> pos{ ps[0], ps[1], ps[2] };

	return mat4<T>{
		{ right_axis.x(), up_axis.x(), direction_axis.x(), 0 },
		{ right_axis.y(),			up_axis.y(),		direction_axis.y(),			0 },
		{ right_axis.z(),			up_axis.z(),		direction_axis.z(),			0 },
		{ -dot(right_axis, pos),	-dot(up_axis, pos), -dot(direction_axis, pos),	1 }
	};
}

template <typename T>
constexpr auto gse::perspective(const angle_t<T> fov, const T aspect, length_t<T> near, length_t<T> far) -> mat4<T> {
	const auto tan_half_fov_y = gse::tan(fov / T(2));
	auto near_m = near.as<typename length_t<T>::default_unit>();
	auto far_m = far.as<typename length_t<T>::default_unit>();

	mat4<T> result;

	result[0][0] = T(1) / (aspect * tan_half_fov_y);
	result[1][1] = T(1) / (tan_half_fov_y);
	result[2][2] = far_m / (near_m - far_m);
	result[2][3] = T(-1);
	result[3][2] = -(far_m * near_m) / (far_m - near_m);
	result[1][1] *= -1;

	return result;
}

template <typename T>
constexpr auto gse::orthographic(length_t<T> left, length_t<T> right, length_t<T> bottom, length_t<T> top, length_t<T> near, length_t<T> far) -> mat4<T> {
	return mat4<T>{
		{ 2 / (right - left).template as<length::default_unit>(), 0, 0, 0 },
		{ 0, 2 / (bottom - top).template as<length::default_unit>(), 0, 0 },
		{ 0, 0, 1 / (near - far).template as<length::default_unit>(), 0 },
		{
			-(right + left) / (right - left),
			-(bottom + top) / (bottom - top),
			near / (near - far),
			1
		}
	};
}

template <typename T, gse::internal::is_vec_element L>
constexpr auto gse::translate(const mat4<T>& matrix, const vec3<L>& translation) -> mat4<T> {
	return matrix * mat4<T>{
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ translation.x().template as<length::default_unit>(), translation.y().template as<length::default_unit>(), translation.z().template as<length::default_unit>(), 1 }
	};
}

template <typename T>
constexpr auto gse::rotate(const mat4<T>& matrix, const axis axis, std::type_identity_t<angle_t<T>> angle) -> mat4<T> {
	auto a = normalize(to_axis_v<T>(axis));
	T half_angle = angle.template as<radians>() / 2;
	T s = std::sin(half_angle);
	T c = std::cos(half_angle);
	auto q = normalize(quat_t<T>(c, a.x() * s, a.y() * s, a.z() * s));
	mat4<T> rotation_matrix = mat4<T>(q);
	return matrix * rotation_matrix;
}

template <typename T>
constexpr auto gse::scale(const mat4<T>& matrix, const vec3f& scale) -> mat4<T> {
	return matrix * mat4<T>{
		{ scale.x(), 0, 0, 0 },
		{ 0,		scale.y(),	0,			0 },
		{ 0,		0,			scale.z(),	0 },
		{ 0,		0,			0,			1 }
	};
}

template <gse::is_mat M>
constexpr auto gse::value_ptr(M& matrix) -> typename M::value_type* {
	return matrix[0].as_storage_span().data();
}

template <gse::is_mat M>
constexpr auto gse::value_ptr(const M& matrix) -> const typename M::value_type* {
	return matrix[0].as_storage_span().data();
}

template <typename T, std::size_t N, std::size_t M>
constexpr auto gse::identity() -> mat<T, N, M> {
	mat<T, N, M> result;
	for (std::size_t i = 0; i < N; ++i) {
		result[i][i] = static_cast<T>(1);
	}
	return result;
}

template <gse::is_vec V1, gse::is_vec V2>
	requires (std::same_as<typename V1::storage_type, typename V2::storage_type>)
constexpr auto gse::outer_product(const V1& a, const V2& b) {
	constexpr auto N = V1::extent;
	constexpr auto M = V2::extent;
	using result_elem = gse::internal::mul_exposed_t<typename V1::value_type, typename V2::value_type>;

	mat<result_elem, M, N> result;
	for (std::size_t col = 0; col < M; ++col) {
		auto rc = result[col].as_storage_span();
		for (std::size_t row = 0; row < N; ++row) {
			rc[row] = a.as_storage_span()[row] * b.as_storage_span()[col];
		}
	}
	return result;
}
