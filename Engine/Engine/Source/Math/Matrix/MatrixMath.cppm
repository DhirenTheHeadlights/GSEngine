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
	template <internal::is_vec_element E1, internal::is_vec_element E2, std::size_t Cols, std::size_t Rows>
		requires requires { typename gse::vec::add_exposed_t<E1, E2>; }
	constexpr auto operator+(
		const mat<E1, Cols, Rows>& lhs,
		const mat<E2, Cols, Rows>& rhs
	);

	template <internal::is_vec_element E1, internal::is_vec_element E2, std::size_t Cols, std::size_t Rows>
		requires requires { typename gse::vec::sub_exposed_t<E1, E2>; }
	constexpr auto operator-(
		const mat<E1, Cols, Rows>& lhs,
		const mat<E2, Cols, Rows>& rhs
	);

	template <internal::is_vec_element E1, internal::is_vec_element E2, std::size_t Cols, std::size_t Rows, std::size_t OtherCols>
	constexpr auto operator*(
		const mat<E1, Cols, Rows>& lhs,
		const mat<E2, OtherCols, Cols>& rhs
	);

	template <internal::is_vec_element E, std::size_t Cols, std::size_t Rows, is_vec V>
		requires (V::extent == Cols)
	constexpr auto operator*(
		const mat<E, Cols, Rows>& lhs,
		const V& rhs
	);

	template <internal::is_vec_element E, std::size_t Cols, std::size_t Rows, internal::is_vec_element S>
	constexpr auto operator*(
		const mat<E, Cols, Rows>& lhs,
		const S& rhs
	);

	template <internal::is_vec_element S, internal::is_vec_element E, std::size_t Cols, std::size_t Rows>
	constexpr auto operator*(
		const S& lhs,
		const mat<E, Cols, Rows>& rhs
	);

	template <internal::is_vec_element E, std::size_t Cols, std::size_t Rows, internal::is_vec_element S>
	constexpr auto operator/(
		const mat<E, Cols, Rows>& lhs,
		const S& rhs
	);

	template <internal::is_vec_element E1, internal::is_vec_element E2, std::size_t Cols, std::size_t Rows>
		requires (requires { typename gse::vec::add_exposed_t<E1, E2>; } && std::same_as<gse::vec::add_exposed_t<E1, E2>, E1>)
	constexpr auto operator+=(
		mat<E1, Cols, Rows>& lhs,
		const mat<E2, Cols, Rows>& rhs
	) -> auto&;

	template <internal::is_vec_element E1, internal::is_vec_element E2, std::size_t Cols, std::size_t Rows>
		requires (requires { typename gse::vec::sub_exposed_t<E1, E2>; } && std::same_as<gse::vec::sub_exposed_t<E1, E2>, E1>)
	constexpr auto operator-=(
		mat<E1, Cols, Rows>& lhs,
		const mat<E2, Cols, Rows>& rhs
	) -> auto&;

	template <internal::is_vec_element E, std::size_t N>
	constexpr auto operator*=(
		mat<E, N, N>& lhs,
		const mat<E, N, N>& rhs
	) -> auto&;

	template <internal::is_vec_element E, std::size_t Cols, std::size_t Rows, internal::is_vec_element S>
		requires internal::is_arithmetic<S>
	constexpr auto operator*=(
		mat<E, Cols, Rows>& lhs, S rhs
	) -> auto&;

	template <internal::is_vec_element E, std::size_t Cols, std::size_t Rows, internal::is_vec_element S>
		requires internal::is_arithmetic<S>
	constexpr auto operator/=(
		mat<E, Cols, Rows>& lhs, S rhs
	) -> auto&;

	template <internal::is_vec_element E, std::size_t Cols, std::size_t Rows>
	constexpr auto operator-(
		const mat<E, Cols, Rows>& m
	) -> auto;

	template <typename T>
	constexpr auto look_at(
		const vec3<length_t<T>>& position,
		const vec3<length_t<T>>& target,
		const unitless::vec3_t<T>& up
	) -> mat4_t<T>;

	template <typename T>
	constexpr auto perspective(
		angle_t<T> fov,
		T aspect,
		length_t<T> near,
		length_t<T> far
	) -> mat4_t<T>;

	template <typename T>
	constexpr auto orthographic(
		length_t<T> left,
		length_t<T> right,
		length_t<T> bottom,
		length_t<T> top,
		length_t<T> near,
		length_t<T> far
	) -> mat4_t<T>;

	template <typename T>
	constexpr auto translate(
		const mat4_t<T>& matrix,
		const vec3<length_t<T>>& translation
	) -> mat4_t<T>;

	template <typename T>
	constexpr auto rotate(
		const mat4_t<T>& matrix,
		unitless::axis axis,
		std::type_identity_t<angle_t<T>> angle
	) -> mat4_t<T>;

	template <typename T>
	constexpr auto scale(
		const mat4_t<T>& matrix,
		const unitless::vec3& scale
	) -> mat4_t<T>;

	template <internal::is_vec_element E, std::size_t N, std::size_t M>
	constexpr auto value_ptr(
		mat<E, N, M>& matrix
	) -> typename mat<E, N, M>::value_type*;

	template <internal::is_vec_element E, std::size_t N, std::size_t M>
	constexpr auto value_ptr(
		const mat<E, N, M>& matrix
	) -> const typename mat<E, N, M>::value_type*;

	template <typename T, std::size_t N, std::size_t M>
	constexpr auto identity(
	) -> mat_t<T, N, M>;

	template <is_vec V1, is_vec V2>
		requires (std::same_as<typename V1::storage_type, typename V2::storage_type>)
	constexpr auto outer_product(
		const V1& a,
		const V2& b
	);
}

template <gse::internal::is_vec_element E1, gse::internal::is_vec_element E2, std::size_t Cols, std::size_t Rows>
	requires requires { typename gse::vec::add_exposed_t<E1, E2>; }
constexpr auto gse::operator+(const mat<E1, Cols, Rows>& lhs, const mat<E2, Cols, Rows>& rhs) {
	using RE = vec::add_exposed_t<E1, E2>;
	mat<RE, Cols, Rows> result;
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::add(lhs[c].as_storage_span(), rhs[c].as_storage_span(), result[c].as_storage_span());
	}
	return result;
}

template <gse::internal::is_vec_element E1, gse::internal::is_vec_element E2, std::size_t Cols, std::size_t Rows>
	requires requires { typename gse::vec::sub_exposed_t<E1, E2>; }
constexpr auto gse::operator-(const mat<E1, Cols, Rows>& lhs, const mat<E2, Cols, Rows>& rhs) {
	using RE = vec::sub_exposed_t<E1, E2>;
	mat<RE, Cols, Rows> result;
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::sub(lhs[c].as_storage_span(), rhs[c].as_storage_span(), result[c].as_storage_span());
	}
	return result;
}

template <gse::internal::is_vec_element E1, gse::internal::is_vec_element E2, std::size_t Cols, std::size_t Rows, std::size_t OtherCols>
constexpr auto gse::operator*(const mat<E1, Cols, Rows>& lhs, const mat<E2, OtherCols, Cols>& rhs) {
	using RE = vec::mul_exposed_t<E1, E2>;
	mat<RE, OtherCols, Rows> result;

	if constexpr (std::is_same_v<E1, float> && std::is_same_v<E2, float> && Cols == 4 && Rows == 4 && OtherCols == 4) {
		if (!std::is_constant_evaluated()) {
			simd::mul_mat4(lhs[0].as_storage_span().data(), rhs[0].as_storage_span().data(), result[0].as_storage_span().data());
			return result;
		}
	}

	for (std::size_t j = 0; j < OtherCols; ++j) {
		auto rj = result[j].as_storage_span();
		for (std::size_t i = 0; i < Rows; ++i) {
			typename mat<RE, OtherCols, Rows>::value_type sum{};
			for (std::size_t k = 0; k < Cols; ++k) {
				sum += lhs[k].as_storage_span()[i] * rhs[j].as_storage_span()[k];
			}
			rj[i] = sum;
		}
	}
	return result;
}

template <gse::internal::is_vec_element E, std::size_t Cols, std::size_t Rows, gse::is_vec V>
	requires (V::extent == Cols)
constexpr auto gse::operator*(const mat<E, Cols, Rows>& lhs, const V& rhs) {
	using result_elem = vec::mul_exposed_t<E, typename V::value_type>;
	using VT = mat<E, Cols, Rows>::value_type;

	vec::base<result_elem, Rows> result{};
	auto result_span = result.as_storage_span();
	auto rhs_span = rhs.as_storage_span();

	for (std::size_t i = 0; i < Rows; ++i) {
		VT sum{};
		for (std::size_t j = 0; j < Cols; ++j) {
			sum += lhs[j].as_storage_span()[i] * rhs_span[j];
		}
		result_span[i] = sum;
	}
	return result;
}

template <gse::internal::is_vec_element E, std::size_t Cols, std::size_t Rows, gse::internal::is_vec_element S>
constexpr auto gse::operator*(const mat<E, Cols, Rows>& lhs, const S& rhs) {
	using RE = vec::mul_exposed_t<E, S>;
	mat<RE, Cols, Rows> result;
	const auto scalar = static_cast<typename mat<E, Cols, Rows>::value_type>(internal::to_storage(rhs));
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::mul_s(lhs[c].as_storage_span(), scalar, result[c].as_storage_span());
	}
	return result;
}

template <gse::internal::is_vec_element S, gse::internal::is_vec_element E, std::size_t Cols, std::size_t Rows>
constexpr auto gse::operator*(const S& lhs, const mat<E, Cols, Rows>& rhs) {
	return rhs * lhs;
}

template <gse::internal::is_vec_element E, std::size_t Cols, std::size_t Rows, gse::internal::is_vec_element S>
constexpr auto gse::operator/(const mat<E, Cols, Rows>& lhs, const S& rhs) {
	using RE = vec::div_exposed_t<E, S>;
	mat<RE, Cols, Rows> result;
	const auto scalar = static_cast<typename mat<E, Cols, Rows>::value_type>(internal::to_storage(rhs));
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::div_s(lhs[c].as_storage_span(), scalar, result[c].as_storage_span());
	}
	return result;
}

template <gse::internal::is_vec_element E1, gse::internal::is_vec_element E2, std::size_t Cols, std::size_t Rows>
	requires (requires { typename gse::vec::add_exposed_t<E1, E2>; } && std::same_as<gse::vec::add_exposed_t<E1, E2>, E1>)
constexpr auto gse::operator+=(mat<E1, Cols, Rows>& lhs, const mat<E2, Cols, Rows>& rhs) -> auto& {
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::add(lhs[c].as_storage_span(), rhs[c].as_storage_span(), lhs[c].as_storage_span());
	}
	return lhs;
}

template <gse::internal::is_vec_element E1, gse::internal::is_vec_element E2, std::size_t Cols, std::size_t Rows>
	requires (requires { typename gse::vec::sub_exposed_t<E1, E2>; } && std::same_as<gse::vec::sub_exposed_t<E1, E2>, E1>)
constexpr auto gse::operator-=(mat<E1, Cols, Rows>& lhs, const mat<E2, Cols, Rows>& rhs) -> auto& {
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::sub(lhs[c].as_storage_span(), rhs[c].as_storage_span(), lhs[c].as_storage_span());
	}
	return lhs;
}

template <gse::internal::is_vec_element E, std::size_t N>
constexpr auto gse::operator*=(mat<E, N, N>& lhs, const mat<E, N, N>& rhs) -> auto& {
	lhs = lhs * rhs;
	return lhs;
}

template <gse::internal::is_vec_element E, std::size_t Cols, std::size_t Rows, gse::internal::is_vec_element S>
	requires gse::internal::is_arithmetic<S>
constexpr auto gse::operator*=(mat<E, Cols, Rows>& lhs, S rhs) -> auto& {
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::mul_s(lhs[c].as_storage_span(), rhs, lhs[c].as_storage_span());
	}
	return lhs;
}

template <gse::internal::is_vec_element E, std::size_t Cols, std::size_t Rows, gse::internal::is_vec_element S>
	requires gse::internal::is_arithmetic<S>
constexpr auto gse::operator/=(mat<E, Cols, Rows>& lhs, S rhs) -> auto& {
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::div_s(lhs[c].as_storage_span(), rhs, lhs[c].as_storage_span());
	}
	return lhs;
}

template <gse::internal::is_vec_element E, std::size_t Cols, std::size_t Rows>
constexpr auto gse::operator-(const mat<E, Cols, Rows>& m) {
	using VT = mat<E, Cols, Rows>::value_type;
	mat<E, Cols, Rows> result;
	for (std::size_t c = 0; c < Cols; ++c) {
		simd::mul_s(m[c].as_storage_span(), static_cast<VT>(-1), result[c].as_storage_span());
	}
	return result;
}

template <typename T>
constexpr auto gse::look_at(const vec3<length_t<T>>& position, const vec3<length_t<T>>& target, const unitless::vec3_t<T>& up) -> mat4_t<T> {
	const auto direction_axis = normalize(position - target);
	const auto right_axis = normalize(cross(up, direction_axis));
	const auto up_axis = cross(direction_axis, right_axis);

	const auto pos = position.as<typename length_t<T>::default_unit>();

	return mat4_t<T>{
		{ right_axis.x(), up_axis.x(), direction_axis.x(), 0 },
		{ right_axis.y(),			up_axis.y(),		direction_axis.y(),			0 },
		{ right_axis.z(),			up_axis.z(),		direction_axis.z(),			0 },
		{ -dot(right_axis, pos),	-dot(up_axis, pos), -dot(direction_axis, pos),	1 }
	};
}

template <typename T>
constexpr auto gse::perspective(const angle_t<T> fov, const T aspect, length_t<T> near, length_t<T> far) -> mat4_t<T> {
	const auto tan_half_fov_y = std::tan(fov.template as<radians>() / T(2));
	auto near_m = near.as<typename length_t<T>::default_unit>();
	auto far_m = far.as<typename length_t<T>::default_unit>();

	mat4_t<T> result;

	result[0][0] = T(1) / (aspect * tan_half_fov_y);
	result[1][1] = T(1) / (tan_half_fov_y);
	result[2][2] = far_m / (near_m - far_m);
	result[2][3] = T(-1);
	result[3][2] = -(far_m * near_m) / (far_m - near_m);
	result[1][1] *= -1;

	return result;
}

template <typename T>
constexpr auto gse::orthographic(length_t<T> left, length_t<T> right, length_t<T> bottom, length_t<T> top, length_t<T> near, length_t<T> far) -> mat4_t<T> {
	return mat4_t<T>{
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

template <typename T>
constexpr auto gse::translate(const mat4_t<T>& matrix, const vec3<length_t<T>>& translation) -> mat4_t<T> {
	return matrix * mat4_t<T>{
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ translation.x().template as<length::default_unit>(), translation.y().template as<length::default_unit>(), translation.z().template as<length::default_unit>(), 1 }
	};
}

template <typename T>
constexpr auto gse::rotate(const mat4_t<T>& matrix, const unitless::axis axis, std::type_identity_t<angle_t<T>> angle) -> mat4_t<T> {
	auto a = normalize(unitless::to_axis_v<T>(axis));
	T half_angle = angle.template as<radians>() / 2;
	T s = std::sin(half_angle);
	T c = std::cos(half_angle);
	auto q = normalize(quat_t<T>(c, a.x() * s, a.y() * s, a.z() * s));
	mat4_t<T> rotation_matrix = mat4_t<T>(q);
	return matrix * rotation_matrix;
}

template <typename T>
constexpr auto gse::scale(const mat4_t<T>& matrix, const unitless::vec3& scale) -> mat4_t<T> {
	return matrix * mat4_t<T>{
		{ scale.x(), 0, 0, 0 },
		{ 0,		scale.y(),	0,			0 },
		{ 0,		0,			scale.z(),	0 },
		{ 0,		0,			0,			1 }
	};
}

template <gse::internal::is_vec_element E, std::size_t Cols, std::size_t Rows>
constexpr auto gse::value_ptr(mat<E, Cols, Rows>& matrix) -> typename mat<E, Cols, Rows>::value_type* {
	return matrix[0].as_storage_span().data();
}

template <gse::internal::is_vec_element E, std::size_t Cols, std::size_t Rows>
constexpr auto gse::value_ptr(const mat<E, Cols, Rows>& matrix) -> const typename mat<E, Cols, Rows>::value_type* {
	return matrix[0].as_storage_span().data();
}

template <typename T, std::size_t N, std::size_t M>
constexpr auto gse::identity() -> mat_t<T, N, M> {
	mat_t<T, N, M> result;
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
	using result_elem = vec::mul_exposed_t<typename V1::value_type, typename V2::value_type>;

	mat<result_elem, M, N> result;
	for (std::size_t col = 0; col < M; ++col) {
		auto rc = result[col].as_storage_span();
		for (std::size_t row = 0; row < N; ++row) {
			rc[row] = a.as_storage_span()[row] * b.as_storage_span()[col];
		}
	}
	return result;
}
