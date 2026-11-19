export module gse.physics:ray_query;

import std;

import gse.core;
import gse.ecs;
import gse.math;

import :bounding_box;
import :collision_component;
import :convex_hull;
import :hull_collision;
import :transform_component;

export namespace gse::physics {
	struct ray_target {
		id owner;
		id group;
		aabb box;
		transform_component transform;
		collision_shape shape;
		const convex_hull* hull = nullptr;
	};

	struct ray_hit {
		id owner;
		length distance;
		vec3f normal;
	};

	auto gather_ray_targets(
		const read<transform_component>& transforms,
		const read<collision_component>& colliders,
		std::span<const convex_hull> hulls,
		std::vector<ray_target>& out
	) -> void;

	auto targets_within(
		std::span<const ray_target> targets,
		const aabb& region,
		std::vector<ray_target>& out
	) -> void;

	auto nearest_hit(
		std::span<const ray_target> targets,
		const vec3<position>& from,
		const vec3<position>& to,
		std::span<const id> ignored
	) -> std::optional<ray_hit>;
}

namespace gse::physics {
	auto segment_meets(
		const aabb& box,
		const vec3<position>& from,
		const vec3<position>& to
	) -> bool;

	auto unit_sphere_entry(
		const vec3f& origin,
		const vec3f& direction
	) -> std::optional<float>;

	auto sphere_first_hit(
		const transform_component& tc,
		const sphere_shape& shape,
		const vec3<position>& from,
		const vec3<position>& to
	) -> std::optional<segment_hit>;

	auto capsule_first_hit(
		const transform_component& tc,
		const capsule_shape& shape,
		const vec3<position>& from,
		const vec3<position>& to
	) -> std::optional<segment_hit>;

	auto shape_first_hit(
		const ray_target& target,
		const vec3<position>& from,
		const vec3<position>& to
	) -> std::optional<segment_hit>;
}
