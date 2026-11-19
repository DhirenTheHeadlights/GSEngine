module gse.physics:ray_query_impl;

import std;

import gse.core;
import gse.ecs;
import gse.math;

import :bounding_box;
import :collision_component;
import :convex_hull;
import :hull_collision;
import :narrow_phase_collision;
import :ray_query;
import :system;
import :transform_component;

auto gse::physics::gather_ray_targets(const read<transform_component>& transforms, const read<collision_component>& colliders, const std::span<const convex_hull> hulls, std::vector<ray_target>& out) -> void {
	out.clear();
	const auto owners = colliders.owner_ids();
	for (std::size_t index = 0; index < colliders.size(); ++index) {
		const auto& collider = colliders[index];
		if (!collider.resolve_collisions) {
			continue;
		}
		const auto* transform = transforms.find(owners[index]);
		if (transform == nullptr) {
			continue;
		}
		const auto* hull = resolve_hull(collider.shape, hulls);
		out.push_back({
			.owner = owners[index],
			.group = collider.filter.group,
			.box = world_aabb_of(*transform, collider, hull),
			.transform = *transform,
			.shape = collider.shape,
			.hull = hull,
		});
	}
}

auto gse::physics::targets_within(const std::span<const ray_target> targets, const aabb& region, std::vector<ray_target>& out) -> void {
	out.clear();
	std::ranges::copy_if(targets, std::back_inserter(out), [&region](const ray_target& target) {
		for (std::size_t axis = 0; axis < 3; ++axis) {
			if (target.box.max[axis] < region.min[axis] || target.box.min[axis] > region.max[axis]) {
				return false;
			}
		}
		return true;
	});
}

auto gse::physics::nearest_hit(const std::span<const ray_target> targets, const vec3<position>& from, const vec3<position>& to, const std::span<const id> ignored) -> std::optional<ray_hit> {
	if (magnitude(to - from) < meters(1e-6f)) {
		return std::nullopt;
	}

	std::optional<ray_hit> nearest;
	for (const auto& target : targets) {
		const bool in_ignored_group = target.group.exists() && std::ranges::contains(ignored, target.group);
		if (in_ignored_group || std::ranges::contains(ignored, target.owner) || !segment_meets(target.box, from, to)) {
			continue;
		}
		const auto hit = shape_first_hit(target, from, to);
		if (hit && (!nearest || hit->distance < nearest->distance)) {
			nearest = {
				.owner = target.owner,
				.distance = hit->distance,
				.normal = hit->normal,
			};
		}
	}
	return nearest;
}

auto gse::physics::segment_meets(const aabb& box, const vec3<position>& from, const vec3<position>& to) -> bool {
	float enter = 0.f;
	float exit = 1.f;
	for (std::size_t axis = 0; axis < 3; ++axis) {
		const auto travel = to[axis] - from[axis];
		if (abs(travel) < meters(1e-6f)) {
			if (from[axis] < box.min[axis] || from[axis] > box.max[axis]) {
				return false;
			}
			continue;
		}
		const float at_min = (box.min[axis] - from[axis]) / travel;
		const float at_max = (box.max[axis] - from[axis]) / travel;
		enter = std::max(enter, std::min(at_min, at_max));
		exit = std::min(exit, std::max(at_min, at_max));
		if (enter > exit) {
			return false;
		}
	}
	return true;
}

auto gse::physics::unit_sphere_entry(const vec3f& origin, const vec3f& direction) -> std::optional<float> {
	const float outside = dot(origin, origin) - 1.f;
	const float receding = dot(origin, direction);
	if (outside <= 0.f || receding >= 0.f) {
		return std::nullopt;
	}
	const float discriminant = receding * receding - outside;
	if (discriminant < 0.f) {
		return std::nullopt;
	}
	return -receding - std::sqrt(discriminant);
}

auto gse::physics::sphere_first_hit(const transform_component& tc, const sphere_shape& shape, const vec3<position>& from, const vec3<position>& to) -> std::optional<segment_hit> {
	if (shape.radius <= length{}) {
		return std::nullopt;
	}
	const vec3<position> center = tc.position;
	const auto reach = to - from;
	const vec3f direction = normalize(reach);
	const vec3f origin = (from - center) / shape.radius;

	const auto entry = unit_sphere_entry(origin, direction);
	if (!entry || shape.radius * *entry > magnitude(reach)) {
		return std::nullopt;
	}
	return segment_hit{
		.distance = shape.radius * *entry,
		.normal = origin + direction * *entry,
	};
}

auto gse::physics::capsule_first_hit(const transform_component& tc, const capsule_shape& shape, const vec3<position>& from, const vec3<position>& to) -> std::optional<segment_hit> {
	constexpr float parallel = 1e-6f;

	if (shape.radius <= length{}) {
		return std::nullopt;
	}
	const vec3<position> center = tc.position;
	const auto reach = to - from;
	const vec3f direction = normalize(reach);
	const vec3f axis = rotate_vector(tc.orientation, vec3f(0.f, 1.f, 0.f));
	const vec3<position> top = center + axis * shape.half_height;
	const vec3f origin = (from - top) / shape.radius;
	const vec3f spine = axis * (-2.f * (shape.half_height / shape.radius));

	const float spine_squared = dot(spine, spine);
	const float along_spine = dot(spine, origin);
	const float nearest_on_spine = spine_squared > 0.f ? std::clamp(along_spine / spine_squared, 0.f, 1.f) : 0.f;
	const vec3f from_spine = origin - spine * nearest_on_spine;
	if (dot(from_spine, from_spine) <= 1.f) {
		return std::nullopt;
	}

	std::optional<float> entry;
	vec3f normal;
	const auto consider = [&](const std::optional<float> candidate, const vec3f& surface_center) {
		if (candidate && (!entry || *candidate < *entry)) {
			entry = candidate;
			normal = origin + direction * *candidate - surface_center;
		}
	};

	consider(unit_sphere_entry(origin, direction), vec3f());
	consider(unit_sphere_entry(origin - spine, direction), spine);

	const float spine_approach = dot(spine, direction);
	const float flare = spine_squared - spine_approach * spine_approach;
	if (flare > parallel * spine_squared) {
		const float closing = spine_squared * dot(direction, origin) - along_spine * spine_approach;
		const float clearance = spine_squared * dot(origin, origin) - along_spine * along_spine - spine_squared;
		const float discriminant = closing * closing - flare * clearance;
		if (discriminant >= 0.f) {
			const float barrel = (-closing - std::sqrt(discriminant)) / flare;
			const float height = along_spine + barrel * spine_approach;
			if (barrel >= 0.f && height > 0.f && height < spine_squared) {
				consider(barrel, spine * (height / spine_squared));
			}
		}
	}

	if (!entry || shape.radius * *entry > magnitude(reach)) {
		return std::nullopt;
	}
	return segment_hit{
		.distance = shape.radius * *entry,
		.normal = normalize(normal),
	};
}

auto gse::physics::shape_first_hit(const ray_target& target, const vec3<position>& from, const vec3<position>& to) -> std::optional<segment_hit> {
	std::optional<segment_hit> result;
	match(target.shape)
		.if_is([&](const box_shape& s) {
			if (const auto hit = narrow_phase_collision::segment_obb_first_hit(bounding_box(target.transform, s), from, to)) {
				result = {
					.distance = hit->distance,
					.normal = hit->normal,
				};
			}
		})
		.else_if_is([&](const sphere_shape& s) {
			result = sphere_first_hit(target.transform, s, from, to);
		})
		.else_if_is([&](const capsule_shape& s) {
			result = capsule_first_hit(target.transform, s, from, to);
		})
		.else_if_is([&](const hull_shape&) {
			if (target.hull != nullptr && target.hull->valid()) {
				const hull_pose pose{
					.center = target.transform.position,
					.orientation = target.transform.orientation,
				};
				result = segment_polytope_first_hit(hull_polytope(*target.hull), pose, from, to);
			}
		});
	return result;
}
