export module gse.physics.math.matrix_math;

import std;

import gse.physics.math.unit_vec;
import gse.physics.math.units.len;
import gse.physics.math.units.ang;
import gse.physics.math.mat;

namespace gse {
	template <typename T>
	auto look_at(const vec3<length_t<T>>& position, const vec3<length_t<T>>& target, const vec3<length_t<T>>& up) -> mat4_t<T>;

	template <typename T>
	auto perspective(float fov, float aspect, length_t<T> near, length_t<T> far) -> mat4_t<T>;

	template <typename T>
	auto orthographic(length_t<T> left, length_t<T> right, length_t<T> bottom, length_t<T> top, length_t<T> near, length_t<T> far) -> mat4_t<T>;

	template <typename T>
	auto translate(const vec3<length_t<T>>& translation) -> mat4_t<T>;

	template <typename T>
	auto rotate(axis axis, angle_t<T> angle) -> mat4_t<T>;

	template <typename T>
	auto scale(const vec3<length_t<T>>& scale) -> mat4_t<T>;
}