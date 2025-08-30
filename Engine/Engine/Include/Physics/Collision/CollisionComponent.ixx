export module gse.physics:collision_component;

import std;

import :bounding_box;

import gse.utility;

export namespace gse::physics {
	struct collision_component_data {
		axis_aligned_bounding_box aabb;
		collision_information collision_information;
		bool resolve_collisions = true;
	};

	struct collision_component : component<collision_component_data> {
		collision_component(const id& owner_id, const collision_component_data& data) : component(owner_id, data), obb(data.aabb) {}
		oriented_bounding_box obb;
	};
}
