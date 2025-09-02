export module gse.physics:collision_component;

import std;

import :bounding_box;

import gse.utility;

export namespace gse::physics {
	struct collision_component_data {
		bounding_box bounding_box;
		collision_information collision_information;
		bool resolve_collisions = true;
	};

	using collision_component = component<collision_component_data>;
}
