export module gse.math:matrix_math;

import std;

import :vector;
import :vector_math;
import :units;
import :matrix;
import :mixed_mat;
import :matrix_types;
import :quat;
import :dimension;
import :quat_math;
import :simd;

export namespace gse {
	template <typename T, gse::internal::is_quantity QPos, gse::internal::is_quantity QTgt>
        requires std::same_as<typename QPos::value_type, T> &&
                 std::same_as<typename QTgt::value_type, T> &&
                 gse::internal::same_unit_family_v<typename QPos::quantity_tag, gse::length_tag> &&
                 gse::internal::same_unit_family_v<typename QTgt::quantity_tag, gse::length_tag>
	constexpr auto look_at(
		const vec3<QPos>& position,
		const vec3<QTgt>& target,
		const vec3<T>& up
	) -> view_matrix;

	template <typename T>
	constexpr auto perspective(
		angle_t<T> fov,
		T aspect,
		length_t<T> near,
		length_t<T> far
	) -> projection_matrix;

	template <typename T>
	constexpr auto orthographic(
		length_t<T> left,
		length_t<T> right,
		length_t<T> bottom,
		length_t<T> top,
		length_t<T> near,
		length_t<T> far
	) -> projection_matrix;

	template <typename T, gse::internal::is_quantity Q>
        requires std::same_as<typename Q::value_type, T> &&
                 gse::internal::same_unit_family_v<typename Q::quantity_tag, gse::length_tag>
	constexpr auto translate(
		const mat4<T>& matrix,
		const vec3<Q>& translation
	) -> mat4<T>;

	template <typename T>
	constexpr auto rotate(
		const mat4<T>& matrix,
		axis axis,
		std::type_identity_t<angle_t<T>> angle
	) -> mat4<T>;

	template <typename T, is_vec V> requires (V::extent == 3)
	constexpr auto scale(
		const mat4<T>& matrix,
		const V& scale
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

template <typename T, gse::internal::is_quantity QPos, gse::internal::is_quantity QTgt>
    requires std::same_as<typename QPos::value_type, T> &&
             std::same_as<typename QTgt::value_type, T> &&
             gse::internal::same_unit_family_v<typename QPos::quantity_tag, gse::length_tag> &&
             gse::internal::same_unit_family_v<typename QTgt::quantity_tag, gse::length_tag>
constexpr auto gse::look_at(const vec3<QPos>& position, const vec3<QTgt>& target, const vec3<T>& up) -> view_matrix {
	const auto direction_axis = normalize(position - target);
	const auto right_axis = normalize(cross(up, direction_axis));
	const auto up_axis = cross(direction_axis, right_axis);
	const auto d = position - vec3<QPos>{};

	view_matrix result;
	result.set<0, 0>(right_axis.x());     result.set<1, 0>(up_axis.x());     result.set<2, 0>(direction_axis.x());
	result.set<0, 1>(right_axis.y());     result.set<1, 1>(up_axis.y());     result.set<2, 1>(direction_axis.y());
	result.set<0, 2>(right_axis.z());     result.set<1, 2>(up_axis.z());     result.set<2, 2>(direction_axis.z());
	result.set<3, 0>(-dot(right_axis, d));
	result.set<3, 1>(-dot(up_axis, d));
	result.set<3, 2>(-dot(direction_axis, d));
	result.set<3, 3>(T(1));
	return result;
}

template <typename T>
constexpr auto gse::perspective(const angle_t<T> fov, const T aspect, length_t<T> near, length_t<T> far) -> projection_matrix {
	const auto tan_half_fov_y = tan(fov / T(2));
	const auto range = far - near;

	mat4<T> result;

	result[0][0] = T(1) / (aspect * tan_half_fov_y);
	result[1][1] = T(-1) / tan_half_fov_y;
	result[2][2] = far / (near - far);
	result[2][3] = T(-1);
	result[3][2] = -(far / range) * internal::to_storage(near);

	return projection_matrix(result);
}

template <typename T>
constexpr auto gse::orthographic(length_t<T> left, length_t<T> right, length_t<T> bottom, length_t<T> top, length_t<T> near, length_t<T> far) -> projection_matrix {
	return projection_matrix(mat4<T>{
		{ 2 / internal::to_storage(right - left), 0, 0, 0 },
		{ 0, 2 / internal::to_storage(bottom - top), 0, 0 },
		{ 0, 0, 1 / internal::to_storage(near - far), 0 },
		{
			-(right + left) / (right - left),
			-(bottom + top) / (bottom - top),
			near / (near - far),
			1
		}
	});
}

template <typename T, gse::internal::is_quantity Q>
    requires std::same_as<typename Q::value_type, T> &&
             gse::internal::same_unit_family_v<typename Q::quantity_tag, gse::length_tag>
constexpr auto gse::translate(const mat4<T>& matrix, const vec3<Q>& translation) -> mat4<T> {
	return matrix * mat4<T>{
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ internal::to_storage(translation.x()), internal::to_storage(translation.y()), internal::to_storage(translation.z()), 1 }
	};
}

template <typename T>
constexpr auto gse::rotate(const mat4<T>& matrix, const axis axis, std::type_identity_t<angle_t<T>> angle) -> mat4<T> {
	auto a = normalize(to_axis_v<T>(axis));
	const auto half_angle = angle / T(2);
	T s = sin(half_angle);
	T c = cos(half_angle);
	auto q = normalize(quat_t<T>(c, a.x() * s, a.y() * s, a.z() * s));
	mat4<T> rotation_matrix = mat4<T>(q);
	return matrix * rotation_matrix;
}

template <typename T, gse::is_vec V> requires (V::extent == 3)
constexpr auto gse::scale(const mat4<T>& matrix, const V& scale) -> mat4<T> {
	return matrix * mat4<T>{
		{ internal::to_storage(scale.x()), 0, 0, 0 },
		{ 0,		internal::to_storage(scale.y()),	0,			0 },
		{ 0,		0,			internal::to_storage(scale.z()),	0 },
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
