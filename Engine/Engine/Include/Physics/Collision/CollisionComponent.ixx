export module gse.physics.collision_component;

import std;

import gse.physics.bounding_box;
import gse.core.component;

export namespace gse::physics {
	struct collision_component final : component {
		collision_component(const std::uint32_t id) : component(id) {}

		axis_aligned_bounding_box bounding_box;
		oriented_bounding_box oriented_bounding_box;
		collision_information collision_information;

		bool resolve_collisions = true;
	};
}
