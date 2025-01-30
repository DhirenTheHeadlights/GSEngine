export module gse.physics.math.quat_math;

import std;

import gse.physics.math.units;
import gse.physics.math.unit_vec;
import gse.physics.math.vec_math;
import gse.physics.math.quat;
import gse.physics.math.matrix;

export namespace gse {
	template <typename T>
	constexpr auto normalize(const quat_t<T>& q) -> quat_t<T>;

	template <typename T>
	constexpr auto mat3_cast(const quat_t<T>& q) -> mat3_t<T>;
}

template <typename T>
constexpr auto gse::normalize(const quat_t<T>& q) -> quat_t<T> {
	auto mag = magnitude(q.v);
	if (mag == 0) {
		return quat_t<unitless>{};
	}
	return { q / mag, q.s / mag };
}

template <typename T>
constexpr auto gse::mat3_cast(const quat_t<T>& q) -> mat3_t<T> {
	auto x = q.v.x;
	auto y = q.v.y;
	auto z = q.v.z;
	auto w = q.s;
	auto x2 = x + x;
	auto y2 = y + y;
	auto z2 = z + z;
	auto xx = x * x2;
	auto xy = x * y2;
	auto xz = x * z2;
	auto yy = y * y2;
	auto yz = y * z2;
	auto zz = z * z2;
	auto wx = w * x2;
	auto wy = w * y2;
	auto wz = w * z2;
	return mat3_t<T>{
			unitless_vec3_t<T>{1 - (yy + zz), xy + wz, xz - wy},
			unitless_vec3_t<T>{xy - wz, 1 - (xx + zz), yz + wx},
			unitless_vec3_t<T>{xz + wy, yz - wx, 1 - (xx + yy)}
	};
}