export module gse.core.object.box;

import std;
import gse.physics.math.vector;

export namespace gse {
	auto create_box(std::uint32_t object_uuid, const vec3<length>& initial_position, const vec3<length>& size) -> void;
	auto create_box(const vec3<length>& initial_position, const vec3<length>& size) -> std::uint32_t;
}