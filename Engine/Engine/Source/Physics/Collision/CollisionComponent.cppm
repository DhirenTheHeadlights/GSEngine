export module gse.physics:collision_component;

import std;

import :bounding_box;
import :convex_hull;

import gse.core;
import gse.ecs;
import gse.math;

export namespace gse::physics {
	struct collision_component {
		[[= networked]] collision_shape shape;
		[[= networked]] bool resolve_collisions = true;
	};

	struct collision_result_component : collision_information {};

	struct hull_definition {
		std::vector<vec3<length>> points;
		bool interned = false;
	};

	auto world_aabb_of(
		const transform_component& tc,
		const collision_component& cc,
		const convex_hull* hull
	) -> aabb;
}

auto gse::physics::world_aabb_of(const transform_component& tc, const collision_component& cc, const convex_hull* hull) -> aabb {
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
		})
		.else_if_is([&](const hull_shape&) {
			if (hull != nullptr && hull->valid()) {
				result = bounding_box(tc, *hull).aabb();
			}
		});
	return result;
}
