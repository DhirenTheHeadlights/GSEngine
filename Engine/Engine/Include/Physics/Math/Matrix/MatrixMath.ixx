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
	template <typename T = float>		constexpr auto look_at(const vec3<length_t<T>>& position, const vec3<length_t<T>>& target, const unitless::vec3_t<T>& up) -> mat4_t<T>;
	template <typename T>				constexpr auto perspective(angle_t<T> fov, T aspect, length_t<T> near, length_t<T> far) -> mat4_t<T>;
	template <typename T>				constexpr auto orthographic(length_t<T> left, length_t<T> right, length_t<T> bottom, length_t<T> top, length_t<T> near, length_t<T> far) -> mat4_t<T>;
	template <typename T>				constexpr auto translate(const mat4_t<T>& matrix, const vec3<length_t<T>>& translation) -> mat4_t<T>;
	template <typename T>				constexpr auto rotate(const mat4_t<T>& matrix, axis axis, angle_t<T> angle) -> mat4_t<T>;
	template <typename T>				constexpr auto scale(const mat4_t<T>& matrix, const vec3<length_t<T>>& scale) -> mat4_t<T>;
	template <typename T, int N, int M>	constexpr auto value_ptr(mat_t<T, N, M>& matrix) -> T*;
	template <typename T, int N, int M> constexpr auto value_ptr(const mat_t<T, N, M>& matrix) -> const T*;
	template <typename T, int N, int M>	constexpr auto identity() -> mat_t<T, N, M>;
}

template <typename T>
constexpr auto gse::look_at(const vec3<length_t<T>>& position, const vec3<length_t<T>>& target, const unitless::vec3_t<T>& up) -> mat4_t<T> {
	auto z_axis = normalize(target - position);
	auto x_axis = normalize(cross(z_axis, up));
	auto y_axis = cross(x_axis, z_axis);
	auto position_in_default_units = position.as<length_t<T>::default_unit>();

	return mat4_t<T>{
		{ x_axis.x, y_axis.x, -z_axis.x,  0 },
		{ x_axis.y, y_axis.y, -z_axis.y,  0 },
		{ x_axis.z, y_axis.z, -z_axis.z,  0 },
		{ -dot(x_axis, position_in_default_units), -dot(y_axis, position_in_default_units), dot(z_axis, position_in_default_units), 1 }
	};
}

template <typename T>
constexpr auto gse::perspective(const angle_t<T> fov, const T aspect, length_t<T> near, length_t<T> far) -> mat4_t<T> {
	const auto tan_half_fov_y = std::tan(fov.template as<units::radians>() / T(2));
	auto near_m = near.template as<length_t<T>::default_unit>();
	auto far_m = far.template as<length_t<T>::default_unit>();

	mat4_t<T> result = { {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };

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
		{2 / (right - left).as_default_unit(), 0, 0, 0},
		{0, 2 / (top - bottom).as_default_unit(), 0, 0},
		{0, 0, -2 / (far - near).as_default_unit(), 0},
		{-(right + left) / (right - left), -(top + bottom) / (top - bottom), -(far + near) / (far - near), 1}
	};
}

template <typename T>
constexpr auto gse::translate(const mat4_t<T>& matrix, const vec3<length_t<T>>& translation) -> mat4_t<T> {
	return matrix * mat4_t<T>{
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{translation.x.as_default_unit(), translation.y.as_default_unit(), translation.z.as_default_unit(), 1}
	};
}

template <typename T>
constexpr auto get_axis_vector(const gse::axis axis) -> gse::unitless::vec3_t<T> {
	switch (axis) {
		case gse::axis::x: return { static_cast<T>(1), 0, 0 };
		case gse::axis::y: return { 0, static_cast<T>(1), 0 };
		case gse::axis::z: return { 0, 0, static_cast<T>(1) };
		default: return { 0, 0, 0 };
	}
}

template <typename T>
constexpr auto gse::rotate(const mat4_t<T>& matrix, const axis axis, angle_t<T> angle) -> mat4_t<T> {
	auto a = normalize(get_axis_vector<T>(axis));

	T half_angle = angle.template as<units::radians>() / 2;
	T s = std::sin(half_angle);
	T c = std::cos(half_angle);

	auto q = normalize(quat_t<T>(c, a.x * s, a.y * s, a.z * s));
	mat4_t <T> rotation_matrix = mat4_t<T>{
		{1 - 2 * q.y * q.y - 2 * q.z * q.z, 2 * q.x * q.y - 2 * q.z * q.s, 2 * q.x * q.z + 2 * q.y * q.s, 0},
		{2 * q.x * q.y + 2 * q.z * q.s, 1 - 2 * q.x * q.x - 2 * q.z * q.z, 2 * q.y * q.z - 2 * q.x * q.s, 0},
		{2 * q.x * q.z - 2 * q.y * q.s, 2 * q.y * q.z + 2 * q.x * q.s, 1 - 2 * q.x * q.x - 2 * q.y * q.y, 0},
		{0, 0, 0, 1}
	};
	return matrix * rotation_matrix;
}

template <typename T>
constexpr auto gse::scale(const mat4_t<T>& matrix, const vec3<length_t<T>>& scale) -> mat4_t<T> {
	return matrix * mat4_t<T>{
		{scale.x.as_default_unit(), 0, 0, 0},
		{0, scale.y.as_default_unit(), 0, 0},
		{0, 0, scale.z.as_default_unit(), 0},
		{0, 0, 0, 1}
	};
}

template <typename T, int Cols, int Rows>
constexpr auto gse::value_ptr(mat_t<T, Cols, Rows>& matrix) -> T* {
	return &matrix[0][0];
}

template <typename T, int Cols, int Rows>
constexpr auto gse::value_ptr(const mat_t<T, Cols, Rows>& matrix) -> const T* {
	return &matrix[0][0];
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
