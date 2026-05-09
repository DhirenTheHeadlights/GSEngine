export module gse.physics:collision_component;

import std;

import :bounding_box;

import gse.core;
import gse.ecs;
import gse.meta;

export namespace gse::physics {
	enum class shape_type : std::uint8_t { box, sphere, capsule };

	struct collision_component {
		[[= networked]] bounding_box bounding_box;
		[[= networked]] shape_type shape = shape_type::box;
		[[= networked]] length shape_radius = {};
		[[= networked]] length shape_half_height = {};
		[[= networked]] bool resolve_collisions = true;
	};

	struct collision_result_component : collision_information {};
}
