export module gse.physics.math:matrix_math;

import std;

import :unit_vec;
import :unit_vec_math;
import :unitless_vec;
import :vec_math;
import :length;
import :angle;
import :matrix;
import :quat;
import :quat_math;

export namespace gse {
	template <typename T, std::size_t Cols, std::size_t Rows>
	constexpr auto operator==(
		const mat_t<T, Cols, Rows>& lhs, 
		const mat_t<T, Cols, Rows>& rhs
	) -> bool;

	template <typename T, std::size_t Cols, std::size_t Rows>
	constexpr auto operator!=(
		const mat_t<T, Cols, Rows>& lhs, 
		const mat_t<T, Cols, Rows>& rhs
	) -> bool;

	template <typename T, std::size_t Cols, std::size_t Rows>
	constexpr auto operator+(
		const mat_t<T, Cols, Rows>& lhs, 
		const mat_t<T, Cols, Rows>& rhs
	) -> mat_t<T, Cols, Rows>;

	template <typename T, std::size_t Cols, std::size_t Rows>
	constexpr auto operator-(
		const mat_t<T, Cols, Rows>& lhs,
		const mat_t<T, Cols, Rows>& rhs
	) -> mat_t<T, Cols, Rows>;

	template <typename T, std::size_t Cols, std::size_t Rows, std::size_t OtherCols>
	constexpr auto operator*(
		const mat_t<T, Cols, Rows>& lhs, 
		const mat_t<T, OtherCols, Cols>& rhs
	) -> mat_t<T, OtherCols, Rows>;

	template <typename T, std::size_t Cols, std::size_t Rows>
	constexpr auto operator*(
		const mat_t<T, Cols, Rows>& lhs, 
		const unitless::vec_t<T, Cols>& rhs
	) -> unitless::vec_t<T, Rows>;

	template <typename T, std::size_t Cols, std::size_t Rows>
	constexpr auto operator*(
		const mat_t<T, Cols, Rows>& lhs, 
		T rhs
	) -> mat_t<T, Cols, Rows>;

	template <typename T, std::size_t Cols, std::size_t Rows>
	constexpr auto operator*(
		T lhs, 
		const mat_t<T, Cols, Rows>& rhs
	) -> mat_t<T, Cols, Rows>;

	template <typename T, std::size_t Cols, std::size_t Rows>
	constexpr auto operator/(
		const mat_t<T, Cols, Rows>& lhs, 
		T rhs
	) -> mat_t<T, Cols, Rows>;

	template <typename T, std::size_t Cols, std::size_t Rows>
	constexpr auto operator+=(
		mat_t<T, Cols, Rows>& lhs, 
		const mat_t<T, Cols, 
		Rows>& rhs
	) -> mat_t<T, Cols, Rows>&;

	template <typename T, std::size_t Cols, std::size_t Rows>
	constexpr auto operator-=(
		mat_t<T, Cols, Rows>& lhs, 
		const mat_t<T, Cols, 
		Rows>& rhs
	) -> mat_t<T, Cols, Rows>&;

	template <typename T, std::size_t N>
	constexpr auto operator*=(
		mat_t<T, N, N>& lhs, 
		const mat_t<T, N, N>& rhs
	) -> mat_t<T, N, N>&;

	template <typename T, std::size_t Cols, std::size_t Rows>
	constexpr auto operator*=(
		mat_t<T, Cols, Rows>& lhs, T rhs
	) -> mat_t<T, Cols, Rows>&;

	template <typename T, std::size_t Cols, std::size_t Rows>
	constexpr auto operator/=(
		mat_t<T, Cols, Rows>& lhs, T rhs
	) -> mat_t<T, Cols, Rows>&;

	template <typename T, std::size_t Cols, std::size_t Rows>
	constexpr auto operator-(
		const mat_t<T, Cols, Rows>& m
	) -> mat_t<T, Cols, Rows>;

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
		const mat4_t<T>& mat_trix,
		const vec3<length_t<T>>& translation
	) -> mat4_t<T>;

	template <typename T>
	constexpr auto rotate(
		const mat4_t<T>& mat_trix,
		unitless::axis axis, 
		angle_t<T> angle
	) -> mat4_t<T>;

	template <typename T>
	constexpr auto scale(
		const mat4_t<T>& mat_trix,
		const unitless::vec3& scale
	) -> mat4_t<T>;

	template <typename T, int N, int M>
	constexpr auto value_ptr(
		mat_t<T, N, M>& mat_trix
	) -> T*;

	template <typename T, int N, int M>
	constexpr auto value_ptr(
		const mat_t<T, N, M>& mat_trix
	) -> const T*;

	template <typename T, int N, int M>
	constexpr auto identity(
	) -> mat_t<T, N, M>;
}

template <typename T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::operator==(const mat_t<T, Cols, Rows>& lhs, const mat_t<T, Cols, Rows>& rhs) -> bool {
	for (std::size_t i = 0; i < Cols; ++i) {
		if (lhs[i] != rhs[i]) {
			return false;
		}
	}
	return true;
}

template <typename T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::operator!=(const mat_t<T, Cols, Rows>& lhs, const mat_t<T, Cols, Rows>& rhs) -> bool {
	return !(lhs == rhs);
}

template <typename T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::operator+(const mat_t<T, Cols, Rows>& lhs, const mat_t<T, Cols, Rows>& rhs) -> mat_t<T, Cols, Rows> {
	mat_t<T, Cols, Rows> result;
	for (std::size_t i = 0; i < Cols; ++i) {
		result[i] = lhs[i] + rhs[i];
	}
	return result;
}

template <typename T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::operator-(const mat_t<T, Cols, Rows>& lhs, const mat_t<T, Cols, Rows>& rhs) -> mat_t<T, Cols, Rows> {
	mat_t<T, Cols, Rows> result;
	for (std::size_t i = 0; i < Cols; ++i) {
		result[i] = lhs[i] - rhs[i];
	}
	return result;
}

template <typename T, std::size_t Cols, std::size_t Rows, std::size_t OtherCols>
constexpr auto gse::operator*(const mat_t<T, Cols, Rows>& lhs, const mat_t<T, OtherCols, Cols>& rhs) -> mat_t<T, OtherCols, Rows> {
	mat_t<T, OtherCols, Rows> result;
	for (std::size_t j = 0; j < OtherCols; ++j) {
		for (std::size_t k = 0; k < Cols; ++k) {
			result[j] += lhs[k] * rhs[j][k];
		}
	}
	return result;
}

template <typename T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::operator*(const mat_t<T, Cols, Rows>& lhs, const unitless::vec_t<T, Cols>& rhs) -> unitless::vec_t<T, Rows> {
	vec::storage<T, Rows> result{};
	for (std::size_t j = 0; j < Cols; ++j) {
		result += lhs[j] * rhs[j];
	}
	return result;
}

template <typename T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::operator*(const mat_t<T, Cols, Rows>& lhs, T rhs) -> mat_t<T, Cols, Rows> {
	mat_t<T, Cols, Rows> result;
	for (std::size_t i = 0; i < Cols; ++i) {
		result[i] = lhs[i] * rhs;
	}
	return result;
}

template <typename T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::operator*(T lhs, const mat_t<T, Cols, Rows>& rhs) -> mat_t<T, Cols, Rows> {
	return rhs * lhs;
}

template <typename T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::operator/(const mat_t<T, Cols, Rows>& lhs, T rhs) -> mat_t<T, Cols, Rows> {
	mat_t<T, Cols, Rows> result;
	for (std::size_t i = 0; i < Cols; ++i) {
		result[i] = lhs[i] / rhs;
	}
	return result;
}

template <typename T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::operator+=(mat_t<T, Cols, Rows>& lhs, const mat_t<T, Cols, Rows>& rhs) -> mat_t<T, Cols, Rows>& {
	for (std::size_t i = 0; i < Cols; ++i) {
		lhs[i] += rhs[i];
	}
	return lhs;
}

template <typename T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::operator-=(mat_t<T, Cols, Rows>& lhs, const mat_t<T, Cols, Rows>& rhs) -> mat_t<T, Cols, Rows>& {
	for (std::size_t i = 0; i < Cols; ++i) {
		lhs[i] -= rhs[i];
	}
	return lhs;
}

template <typename T, std::size_t N>
constexpr auto gse::operator*=(mat_t<T, N, N>& lhs, const mat_t<T, N, N>& rhs) -> mat_t<T, N, N>& {
	lhs = lhs * rhs;
	return lhs;
}

template <typename T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::operator*=(mat_t<T, Cols, Rows>& lhs, T rhs) -> mat_t<T, Cols, Rows>& {
	for (std::size_t i = 0; i < Cols; ++i) {
		lhs[i] *= rhs;
	}
	return lhs;
}

template <typename T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::operator/=(mat_t<T, Cols, Rows>& lhs, T rhs) -> mat_t<T, Cols, Rows>& {
	for (std::size_t i = 0; i < Cols; ++i) {
		lhs[i] /= rhs;
	}
	return lhs;
}

template <typename T, std::size_t Cols, std::size_t Rows>
constexpr auto gse::operator-(const mat_t<T, Cols, Rows>& m) -> mat_t<T, Cols, Rows> {
	mat_t<T, Cols, Rows> result;
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

	const auto pos = position.template as<typename length_t<T>::default_unit>();

	return mat4_t<T>{
		{ right_axis.x,				up_axis.x,			direction_axis.x,			0 },
		{ right_axis.y,				up_axis.y,			direction_axis.y,			0 },
		{ right_axis.z,				up_axis.z,			direction_axis.z,			0 },
		{ -dot(right_axis, pos),	-dot(up_axis, pos), -dot(direction_axis, pos),	1 }
	};
}

template <typename T>
constexpr auto gse::perspective(const angle_t<T> fov, const T aspect, length_t<T> near, length_t<T> far) -> mat4_t<T> {
	const auto tan_half_fov_y = std::tan(fov.template as<units::radians>() / T(2));
	auto near_m = near.template as<length_t<T>::default_unit>();
	auto far_m = far.template as<length_t<T>::default_unit>();

	mat4_t<T> result;

	result[0][0] = T(1) / (aspect * tan_half_fov_y);
	result[1][1] = T(1) / (tan_half_fov_y);

	result[2][2] = far_m / (far_m - near_m);
	result[2][3] = T(1);
	result[3][2] = -(far_m * near_m) / (far_m - near_m);

	result[1][1] *= -1;

	return result;
}

template <typename T>
constexpr auto gse::orthographic(length_t<T> left, length_t<T> right, length_t<T> bottom, length_t<T> top, length_t<T> near, length_t<T> far) -> mat4_t<T> {
	return mat4_t<T>{
		{ 2 / (right - left).as_default_unit(), 0,										0,									 0 },
		{ 0,									2 / (top - bottom).as_default_unit(),	0,									 0 },
		{ 0,									0,										-2 / (far - near).as_default_unit(), 0 },
		{ -(right + left) / (right - left),		-(top + bottom) / (top - bottom),		-(far + near) / (far - near),		 1 }
	};
}

template <typename T>
constexpr auto gse::translate(const mat4_t<T>& mat_trix, const vec3<length_t<T>>& translation) -> mat4_t<T> {
	return mat_trix * mat4_t<T>{
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ translation.x.as_default_unit(), translation.y.as_default_unit(), translation.z.as_default_unit(), 1 }
	};
}

template <typename T>
constexpr auto gse::rotate(const mat4_t<T>& mat_trix, const enum unitless::axis axis, angle_t<T> angle) -> mat4_t<T> {
	auto a = normalize(unitless::axis_v<T>(axis));

	T half_angle = angle.template as<units::radians>() / 2;
	T s = std::sin(half_angle);
	T c = std::cos(half_angle);

	auto q = normalize(quat_t<T>(c, a.x * s, a.y * s, a.z * s));
	mat4_t <T> rotation_mat_trix = mat4_t<T>{
		{ 1 - 2 * q.y * q.y - 2 * q.z * q.z, 2 * q.x * q.y - 2 * q.z * q.s,		2 * q.x * q.z + 2 * q.y * q.s,		0},
		{ 2 * q.x * q.y + 2 * q.z * q.s,	 1 - 2 * q.x * q.x - 2 * q.z * q.z, 2 * q.y * q.z - 2 * q.x * q.s,		0},
		{ 2 * q.x * q.z - 2 * q.y * q.s,	 2 * q.y * q.z + 2 * q.x * q.s,		1 - 2 * q.x * q.x - 2 * q.y * q.y,	0},
		{ 0,								 0,									0,									1}
	};
	return mat_trix * rotation_mat_trix;
}

template <typename T>
constexpr auto gse::scale(const mat4_t<T>& mat_trix, const unitless::vec3& scale) -> mat4_t<T> {
	return mat_trix * mat4_t<T>{
		{ scale.x,	0,			0,			0 },
		{ 0,		scale.y,	0,			0 },
		{ 0,		0,			scale.z,	0 },
		{ 0,		0,			0,			1 }
	};
}

template <typename T, int Cols, int Rows>
constexpr auto gse::value_ptr(mat_t<T, Cols, Rows>& mat_trix) -> T* {
	return &mat_trix[0][0];
}

template <typename T, int Cols, int Rows>
constexpr auto gse::value_ptr(const mat_t<T, Cols, Rows>& mat_trix) -> const T* {
	return &mat_trix[0][0];
}

template <typename T, int N, int M>
constexpr auto gse::identity() -> mat_t<T, N, M> {
	mat_t<T, N, M> result;
	for (int i = 0; i < N; ++i) {
		for (int j = 0; j < M; ++j) {
			result[i][j] = (i == j) ? static_cast<T>(1) : static_cast<T>(0);
		}
	}
	return result;
}
