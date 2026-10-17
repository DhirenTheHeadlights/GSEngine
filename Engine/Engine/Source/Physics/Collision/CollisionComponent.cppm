export module gse.physics:collision_component;

import std;

import :bounding_box;
import :convex_hull;

import gse.core;
import gse.ecs;
import gse.math;

export namespace gse::physics {
	struct collision_filter {
		id group;
		std::uint32_t layers = 0;
		std::uint32_t mask = 0;
	};

	struct collision_component {
		[[= networked]] collision_shape shape;
		[[= networked]] bool resolve_collisions = true;
		[[= networked]] collision_filter filter;
	};

	struct collision_result_component : collision_information {};

	struct hull_definition {
		std::vector<vec3<length>> points;
		bool interned = false;
	};

	struct collision_exclusion {
		[[= networked]] id entity_a;
		[[= networked]] id entity_b;
	};

	auto world_aabb_of(
		const transform_component& tc,
		const collision_component& cc,
		const convex_hull* hull
	) -> aabb;

	auto can_collide(
		const collision_filter& a,
		const collision_filter& b
	) -> bool;
}

auto gse::physics::can_collide(const collision_filter& a, const collision_filter& b) -> bool {
	if (!a.group.exists() || a.group != b.group) {
		return true;
	}
	return (a.layers & b.mask) != 0 || (b.layers & a.mask) != 0;
}

auto gse::physics::world_aabb_of(const transform_component& tc, const collision_component& cc, const convex_hull* hull) -> aabb {
	aabb result;
	match(cc.shape)
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