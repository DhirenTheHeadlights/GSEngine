export module gse.physics:collision_component;

import std;

import :bounding_box;

import gse.core;
import gse.ecs;
import gse.math;
import gse.meta;

export namespace gse::physics {
	struct collision_component {
		[[= networked]] std::variant<box_shape, sphere_shape, capsule_shape> shape = box_shape{};
		[[= networked]] bool resolve_collisions = true;
	};

	struct collision_result_component : collision_information {};

	auto world_aabb_of(
		const transform_component& tc,
		const collision_component& cc
	) -> aabb;
}

auto gse::physics::world_aabb_of(const transform_component& tc, const collision_component& cc) -> aabb {
	return std::visit([&](const auto& shape) {
		return bounding_box(tc, shape).aabb();
	}, cc.shape);
}
