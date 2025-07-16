export module gse.physics:collision_component;

import std;

import :bounding_box;
import gse.utility;

export namespace gse::physics {
	struct collision_component final : component {
		explicit collision_component(const id& id) : component(id) {}

		axis_aligned_bounding_box bounding_box;
		oriented_bounding_box oriented_bounding_box;
		collision_information collision_information;

		bool resolve_collisions = true;
	};
}
