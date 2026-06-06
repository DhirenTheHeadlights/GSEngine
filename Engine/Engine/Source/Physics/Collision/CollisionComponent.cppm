export module gse.physics:collision_component;

import std;

import :bounding_box;

import gse.core;
import gse.ecs;
import gse.math;

export namespace gse::physics {
	struct collision_component {
		[[= networked]] collision_shape shape;
		[[= networked]] bool resolve_collisions = true;
	};

	struct collision_result_component : collision_information {};

	auto world_aabb_of(
		const transform_component& tc,
		const collision_component& cc
	) -> aabb;
}

auto gse::physics::world_aabb_of(const transform_component& tc, const collision_component& cc) -> aabb {
	aabb result;
	gse::match(cc.shape)
		.if_is([&](const box_shape& s) {
			result = bounding_box(tc, s).aabb();
		})
		.else_if_is([&](const sphere_shape& s) {
			result = bounding_box(tc, s).aabb();
		})
		.else_if_is([&](const capsule_shape& s) {
			result = bounding_box(tc, s).aabb();
		});
	return result;
}
