export module gse.physics:collision_component;

import std;

import :bounding_box;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
export namespace gse::physics {
	enum class shape_type : std::uint8_t { box, sphere, capsule };

	struct collision_component_data {
		bounding_box bounding_box;
		collision_information collision_information;
		shape_type shape = shape_type::box;
		length shape_radius = {};
		length shape_half_height = {};
		bool resolve_collisions = true;
	};

	using collision_component = component<collision_component_data, bounding_box>;
}
