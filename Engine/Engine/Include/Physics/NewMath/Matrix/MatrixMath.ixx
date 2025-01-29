export module gse.physics.math.matrix_math;

import std;

import gse.physics.math.unit_vec;
import gse.physics.math.vec_math;
import gse.physics.math.units.len;
import gse.physics.math.units.ang;
import gse.physics.math.matrix;

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

template <typename T>
auto gse::look_at(const vec3<length_t<T>>& position, const vec3<length_t<T>>& target, const vec3<length_t<T>>& up) -> mat4_t<T> {
	auto z_axis = normalize(target - position);
	auto x_axis = normalize(cross(up, z_axis));
	auto y_axis = cross(z_axis, x_axis);
	return mat4_t<T>{
		unitless_vec4_t<T>{x_axis.x, y_axis.x, z_axis.x, 0},
			unitless_vec4_t<T>{x_axis.y, y_axis.y, z_axis.y, 0},
			unitless_vec4_t<T>{x_axis.z, y_axis.z, z_axis.z, 0},
			unitless_vec4_t<T>{-dot(x_axis, position), -dot(y_axis, position), -dot(z_axis, position), 1
		}
	};
}

template <typename T>
auto gse::perspective(const float fov, const float aspect, length_t<T> near, length_t<T> far) -> mat4_t<T> {
	auto f = 1 / std::tan(fov / 2);
	return mat4_t<T>{
		unitless_vec4_t<T>{f / aspect, 0, 0, 0},
			unitless_vec4_t<T>{0, f, 0, 0},
			unitless_vec4_t<T>{0, 0, (far + near) / (near - far), -1},
			unitless_vec4_t<T>{0, 0, (2 * far * near) / (near - far), 0
		}
	};
}

template <typename T>
auto gse::orthographic(length_t<T> left, length_t<T> right, length_t<T> bottom, length_t<T> top, length_t<T> near, length_t<T> far) -> mat4_t<T> {
	return mat4_t<T>{
		unitless_vec4_t<T>{2 / (right - left), 0, 0, 0},
			unitless_vec4_t<T>{0, 2 / (top - bottom), 0, 0},
			unitless_vec4_t<T>{0, 0, -2 / (far - near), 0},
			unitless_vec4_t<T>{-(right + left) / (right - left), -(top + bottom) / (top - bottom), -(far + near) / (far - near), 1
		}
	};
}

template <typename T>
auto gse::translate(const vec3<length_t<T>>& translation) -> mat4_t<T> {
	return mat4_t<T>{
		unitless_vec4_t<T>{1, 0, 0, 0},
			unitless_vec4_t<T>{0, 1, 0, 0},
			unitless_vec4_t<T>{0, 0, 1, 0},
			unitless_vec4_t<T>{translation.x, translation.y, translation.z, 1
		}
	};
}

template <typename T>
constexpr auto get_axis_vector(const gse::axis axis) -> gse::unitless_vec3_t<T> {
	switch (axis) {
	case gse::axis::x: return { static_cast<T>(1), 0, 0 };
	case gse::axis::y: return { 0, static_cast<T>(1), 0 };
	case gse::axis::z: return { 0, 0, static_cast<T>(1) };
	default: return { 0, 0, 0 };
	}
}

template <typename T>
auto gse::rotate(const axis axis, angle_t<T> angle) -> mat4_t<T> {
	auto a = get_axis_vector<T>(axis);
	auto b = cross(a, unitless_vec3_t<T>{ 0, 0, 1 });
	auto c = cross(a, b);
	return mat4_t<T>{
		unitless_vec4_t<T>{a.x, b.x, c.x, 0},
			unitless_vec4_t<T>{a.y, b.y, c.y, 0},
			unitless_vec4_t<T>{a.z, b.z, c.z, 0},
			unitless_vec4_t<T>{0, 0, 0, 1}
	} *rotate(b, angle)* rotate(c, angle);
}

template <typename T>
auto gse::scale(const vec3<length_t<T>>& scale) -> mat4_t<T> {
	return mat4_t<T>{
		unitless_vec4_t<T>{scale.x, 0, 0, 0},
			unitless_vec4_t<T>{0, scale.y, 0, 0},
			unitless_vec4_t<T>{0, 0, scale.z, 0},
			unitless_vec4_t<T>{0, 0, 0, 1}
	};
}