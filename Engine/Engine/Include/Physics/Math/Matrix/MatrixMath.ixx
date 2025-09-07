export module gse.physics.math:matrix_math;

import std;

import :vector;
import :vector_math;
import :units;
import :matrix;
import :quat;
import :dimension;
import :quat_math;

export namespace gse {
	template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
	constexpr auto operator==(
		const mat_t<T, Cols, Rows, Dim>& lhs,
		const mat_t<T, Cols, Rows, Dim>& rhs
	) -> bool;

	template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
	constexpr auto operator!=(
		const mat_t<T, Cols, Rows, Dim>& lhs,
		const mat_t<T, Cols, Rows, Dim>& rhs
	) -> bool;

	template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
	constexpr auto operator+(
		const mat_t<T, Cols, Rows, Dim>& lhs,
		const mat_t<T, Cols, Rows, Dim>& rhs
	) -> mat_t<T, Cols, Rows, Dim>;

	template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
	constexpr auto operator-(
		const mat_t<T, Cols, Rows, Dim>& lhs,
		const mat_t<T, Cols, Rows, Dim>& rhs
	) -> mat_t<T, Cols, Rows, Dim>;

	template <typename T, std::size_t Cols, std::size_t Rows, std::size_t OtherCols, typename Dim1, typename Dim2>
	constexpr auto operator*(
		const mat_t<T, Cols, Rows, Dim1>& lhs,
		const mat_t<T, OtherCols, Cols, Dim2>& rhs
	) -> mat_t<T, OtherCols, Rows, decltype(Dim1{} *Dim2{})>;

	template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
	constexpr auto operator*(
		const mat_t<T, Cols, Rows, Dim>& lhs,
		const unitless::vec_t<T, Cols>& rhs
	);

	template <typename T, std::size_t Cols, std::size_t Rows, typename Dim, typename Q>
	constexpr auto operator*(
		const mat_t<T, Cols, Rows, Dim>& lhs,
		const vec_t<Q, Cols>& rhs
	);

	template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
	constexpr auto operator*(
		const mat_t<T, Cols, Rows, Dim>& lhs,
		T rhs
	) -> mat_t<T, Cols, Rows, Dim>;

	template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
	constexpr auto operator*(
		T lhs,
		const mat_t<T, Cols, Rows, Dim>& rhs
	) -> mat_t<T, Cols, Rows, Dim>;

	template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
	constexpr auto operator/(
		const mat_t<T, Cols, Rows, Dim>& lhs,
		T rhs
	) -> mat_t<T, Cols, Rows, Dim>;

	template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
	constexpr auto operator+=(
		mat_t<T, Cols, Rows, Dim>& lhs,
		const mat_t<T, Cols, Rows, Dim>& rhs
	) -> mat_t<T, Cols, Rows, Dim>&;

	template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
	constexpr auto operator-=(
		mat_t<T, Cols, Rows, Dim>& lhs,
		const mat_t<T, Cols, Rows, Dim>& rhs
	) -> mat_t<T, Cols, Rows, Dim>&;

	template <typename T, std::size_t N, typename Dim, typename RhsDim>
		requires internal::has_same_dimensions<RhsDim, Dim>
	constexpr auto operator*=(
		mat_t<T, N, N, Dim>& lhs,
		const mat_t<T, N, N, RhsDim>& rhs
	) -> mat_t<T, N, N, Dim>&;

	template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
	constexpr auto operator*=(
		mat_t<T, Cols, Rows, Dim>& lhs, T rhs
	) -> mat_t<T, Cols, Rows, Dim>&;

	template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
	constexpr auto operator/=(
		mat_t<T, Cols, Rows, Dim>& lhs, T rhs
	) -> mat_t<T, Cols, Rows, Dim>&;

	template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
	constexpr auto operator-(
		const mat_t<T, Cols, Rows, Dim>& m
	) -> mat_t<T, Cols, Rows, Dim>;

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

	template <typename T, typename Dim>
	constexpr auto translate(
		const mat4_t<T, Dim>& matrix,
		const vec3<length_t<T>>& translation
	) -> mat4_t<T, Dim>;

	template <typename T, typename Dim>
	constexpr auto rotate(
		const mat_t<T, 4, 4, Dim>& matrix,
		unitless::axis axis,
		std::type_identity_t<angle_t<T>> angle
	) -> mat_t<T, 4, 4, Dim>;

	template <typename T, typename Dim>
	constexpr auto scale(
		const mat_t<T, 4, 4, Dim>& matrix,
		const unitless::vec3& scale
	) -> mat_t<T, 4, 4, Dim>;

	template <typename T, int N, int M, typename Dim>
	constexpr auto value_ptr(
		mat_t<T, N, M, Dim>& matrix
	) -> T*;

	template <typename T, int N, int M, typename Dim>
	constexpr auto value_ptr(
		const mat_t<T, N, M, Dim>& matrix
	) -> const T*;

	template <typename T, int N, int M>
	constexpr auto identity(
	) -> mat_t<T, N, M>;
}

template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
constexpr auto gse::operator==(const mat_t<T, Cols, Rows, Dim>& lhs, const mat_t<T, Cols, Rows, Dim>& rhs) -> bool {
	for (std::size_t i = 0; i < Cols; ++i) {
		if (lhs[i] != rhs[i]) {
			return false;
		}
	}
	return true;
}

template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
constexpr auto gse::operator!=(const mat_t<T, Cols, Rows, Dim>& lhs, const mat_t<T, Cols, Rows, Dim>& rhs) -> bool {
	return !(lhs == rhs);
}

template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
constexpr auto gse::operator+(const mat_t<T, Cols, Rows, Dim>& lhs, const mat_t<T, Cols, Rows, Dim>& rhs) -> mat_t<T, Cols, Rows, Dim> {
	mat_t<T, Cols, Rows, Dim> result;
	for (std::size_t i = 0; i < Cols; ++i) {
		result[i] = lhs[i] + rhs[i];
	}
	return result;
}

template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
constexpr auto gse::operator-(const mat_t<T, Cols, Rows, Dim>& lhs, const mat_t<T, Cols, Rows, Dim>& rhs) -> mat_t<T, Cols, Rows, Dim> {
	mat_t<T, Cols, Rows, Dim> result;
	for (std::size_t i = 0; i < Cols; ++i) {
		result[i] = lhs[i] - rhs[i];
	}
	return result;
}

template <typename T, std::size_t Cols, std::size_t Rows, std::size_t OtherCols, typename Dim1, typename Dim2>
constexpr auto gse::operator*(const mat_t<T, Cols, Rows, Dim1>& lhs, const mat_t<T, OtherCols, Cols, Dim2>& rhs)  -> mat_t<T, OtherCols, Rows, decltype(Dim1{} *Dim2{})> {
	using result_dim = decltype(Dim1{} * Dim2{});
	mat_t<T, OtherCols, Rows, result_dim> result;
	for (std::size_t j = 0; j < OtherCols; ++j) {
		for (std::size_t k = 0; k < Cols; ++k) {
			result[j] += lhs[k] * rhs[j][k];
		}
	}
	return result;
}

template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
constexpr auto gse::operator*(const mat_t<T, Cols, Rows, Dim>& lhs, const unitless::vec_t<T, Cols>& rhs) {
	using result_quantity = internal::quantity<T, Dim>;
	vec_t<result_quantity, Rows> result{};
	for (std::size_t j = 0; j < Cols; ++j) {
		result += lhs[j] * rhs[j];
	}
	return result;
}

template <typename T, std::size_t Cols, std::size_t Rows, typename Dim, typename Q>
constexpr auto gse::operator*(const mat_t<T, Cols, Rows, Dim>& lhs, const vec_t<Q, Cols>& rhs) {
	using result_dimension = decltype(Dim{} *typename Q::dimension{});
	using result_value_type = std::common_type_t<T, typename Q::value_type>;
	using result_quantity = internal::quantity<result_value_type, result_dimension>;

	vec_t<result_quantity, Rows> result{};

	for (std::size_t i = 0; i < Rows; ++i) {
		result_quantity sum{};
		for (std::size_t j = 0; j < Cols; ++j) {
			auto mat_element_as_quantity = internal::quantity<T, Dim>(lhs[j][i]);
			sum += mat_element_as_quantity * rhs[j];
		}
		result[i] = sum;
	}

	return result;
}

template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
constexpr auto gse::operator*(const mat_t<T, Cols, Rows, Dim>& lhs, T rhs) -> mat_t<T, Cols, Rows, Dim> {
	mat_t<T, Cols, Rows, Dim> result;
	for (std::size_t i = 0; i < Cols; ++i) {
		result[i] = lhs[i] * rhs;
	}
	return result;
}

template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
constexpr auto gse::operator*(T lhs, const mat_t<T, Cols, Rows, Dim>& rhs) -> mat_t<T, Cols, Rows, Dim> {
	return rhs * lhs;
}

template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
constexpr auto gse::operator/(const mat_t<T, Cols, Rows, Dim>& lhs, T rhs) -> mat_t<T, Cols, Rows, Dim> {
	mat_t<T, Cols, Rows, Dim> result;
	for (std::size_t i = 0; i < Cols; ++i) {
		result[i] = lhs[i] / rhs;
	}
	return result;
}

template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
constexpr auto gse::operator+=(mat_t<T, Cols, Rows, Dim>& lhs, const mat_t<T, Cols, Rows, Dim>& rhs) -> mat_t<T, Cols, Rows, Dim>& {
	for (std::size_t i = 0; i < Cols; ++i) {
		lhs[i] += rhs[i];
	}
	return lhs;
}

template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
constexpr auto gse::operator-=(mat_t<T, Cols, Rows, Dim>& lhs, const mat_t<T, Cols, Rows, Dim>& rhs) -> mat_t<T, Cols, Rows, Dim>& {
	for (std::size_t i = 0; i < Cols; ++i) {
		lhs[i] -= rhs[i];
	}
	return lhs;
}

template <typename T, std::size_t N, typename Dim, typename RhsDim>
	requires gse::internal::has_same_dimensions<RhsDim, Dim>
constexpr auto gse::operator*=(mat_t<T, N, N, Dim>& lhs, const mat_t<T, N, N, RhsDim>& rhs) -> mat_t<T, N, N, Dim>& {
	lhs = lhs * rhs;
	return lhs;
}

template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
constexpr auto gse::operator*=(mat_t<T, Cols, Rows, Dim>& lhs, T rhs) -> mat_t<T, Cols, Rows, Dim>& {
	for (std::size_t i = 0; i < Cols; ++i) {
		lhs[i] *= rhs;
	}
	return lhs;
}

template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
constexpr auto gse::operator/=(mat_t<T, Cols, Rows, Dim>& lhs, T rhs) -> mat_t<T, Cols, Rows, Dim>& {
	for (std::size_t i = 0; i < Cols; ++i) {
		lhs[i] /= rhs;
	}
	return lhs;
}

template <typename T, std::size_t Cols, std::size_t Rows, typename Dim>
constexpr auto gse::operator-(const mat_t<T, Cols, Rows, Dim>& m) -> mat_t<T, Cols, Rows, Dim> {
	mat_t<T, Cols, Rows, Dim> result;
	for (std::size_t i = 0; i < Cols; ++i) {
		result[i] = -m[i];
	}
	return result;
}

template <typename T>
constexpr auto gse::look_at(const vec3<length_t<T>>& position, const vec3<length_t<T>>& target, const unitless::vec3_t<T>& up) -> mat4_t<T> {
	const auto direction_axis = normalize(position - target);
	const auto right_axis = normalize(cross(up, direction_axis));
	const auto up_axis = cross(direction_axis, right_axis);

	const auto pos = position.as<typename length_t<T>::default_unit{}>();

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
	auto near_m = near.as<typename length_t<T>::default_unit{}>();
	auto far_m = far.as<typename length_t<T>::default_unit{}>();

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
		{ 2 / (right - left).as_default_unit(), 0, 0, 0 },
		{ 0,									2 / (top - bottom).as_default_unit(),	0,									 0 },
		{ 0,									0,										-2 / (far - near).as_default_unit(), 0 },
		{ -(right + left) / (right - left),		-(top + bottom) / (top - bottom),		-(far + near) / (far - near),		 1 }
	};
}

template <typename T, typename Dim>
constexpr auto gse::translate(const mat4_t<T, Dim>& matrix, const vec3<length_t<T>>& translation) -> mat4_t<T, Dim> {
	return matrix * mat4_t<T, Dim>{
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ translation.x().as_default_unit(), translation.y().as_default_unit(), translation.z().as_default_unit(), 1 }
	};
}

template <typename T, typename Dim>
constexpr auto gse::rotate(const mat_t<T, 4, 4, Dim>& matrix, const unitless::axis axis, std::type_identity_t<angle_t<T>> angle) -> mat_t<T, 4, 4, Dim> {
	auto a = normalize(unitless::to_axis_v<T>(axis));
	T half_angle = angle.template as<radians>() / 2;
	T s = std::sin(half_angle);
	T c = std::cos(half_angle);
	auto q = normalize(quat_t<T>(c, a.x() * s, a.y() * s, a.z() * s));
	mat4_t<T> rotation_matrix = mat4_t<T>(q);
	return matrix * rotation_matrix;
}

template <typename T, typename Dim>
constexpr auto gse::scale(const mat_t<T, 4, 4, Dim>& matrix, const unitless::vec3& scale) -> mat_t<T, 4, 4, Dim> {
	return matrix * mat4_t<T>{
		{ scale.x(), 0, 0, 0 },
		{ 0,		scale.y(),	0,			0 },
		{ 0,		0,			scale.z(),	0 },
		{ 0,		0,			0,			1 }
	};
}

template <typename T, int Cols, int Rows, typename Dim>
constexpr auto gse::value_ptr(mat_t<T, Cols, Rows, Dim>& matrix) -> T* {
	return &matrix[0][0];
}

template <typename T, int Cols, int Rows, typename Dim>
constexpr auto gse::value_ptr(const mat_t<T, Cols, Rows, Dim>& matrix) -> const T* {
	return &matrix[0][0];
}

template <typename T, int N, int M>
constexpr auto gse::identity() -> mat_t<T, N, M> {
	mat_t<T, N, M> result;
	for (int i = 0; i < N; ++i) {
		result[i][i] = static_cast<T>(1);
	}
	return result;
}