export module gse.physics.narrow_phase_collisions;

import std;

import gse.physics.motion_component;
import gse.physics.bounding_box;
import gse.physics.collision_component;
import gse.physics.math;

export namespace gse::narrow_phase_collision {
    auto resolve_collision(physics::motion_component* object_motion_component, physics::collision_component& object_collision_component, const physics::collision_component& other_collision_component) -> void;
}

auto overlaps_on_axis(const gse::oriented_bounding_box& box1, const gse::oriented_bounding_box& box2, const gse::vec3<gse::length>& axis, gse::length& penetration) -> bool {
    if (is_zero(axis)) {
        return true;
    }

    const auto normalized_axis = normalize(axis);
    const auto corners1 = box1.get_corners();

    auto project_point = [](const gse::vec3<gse::length>& point, const gse::vec3<gse::length>& projection_axis) -> gse::length {
        return dot(point, projection_axis);
        };

    gse::length min1 = project_point(corners1[0], normalized_axis);
    gse::length max1 = min1;

    for (const auto& corner : corners1) {
        gse::length projection = project_point(corner, normalized_axis);
        min1 = std::min(min1, projection);
        max1 = std::max(max1, projection);
    }

    const auto corners2 = box2.get_corners();

    gse::length min2 = project_point(corners2[0], normalized_axis);
    gse::length max2 = min2;

    for (const auto& corner : corners2) {
        gse::length projection = project_point(corner, normalized_axis);
        min2 = std::min(min2, projection);
        max2 = std::max(max2, projection);
    }

    if (max1 < min2 || max2 < min1) {
        return false;
    }

    const auto overlap = std::min(max1, max2) - std::max(min1, min2);
    penetration = overlap;
    return true;
}

auto sat_collision(const gse::oriented_bounding_box& obb1, const gse::oriented_bounding_box& obb2, gse::unitless::vec3& collision_normal, gse::length& min_penetration) -> bool {
    std::array<gse::vec3<gse::length>, 15> axes;
    int axis_count = 0;

    for (int i = 0; i < 3; ++i) {
        axes[axis_count++] = obb1.axes[i];
    }

    for (int i = 0; i < 3; ++i) {
        axes[axis_count++] = obb2.axes[i];
    }

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (gse::vec3<gse::length> cross = gse::cross(obb1.axes[i].as<gse::units::meters>(), obb2.axes[j].as<gse::units::meters>()); !is_zero(cross)) { // Avoid near-zero vectors
                axes[axis_count++] = cross;
            }
        }
    }

    min_penetration = gse::meters(std::numeric_limits<float>::max());
    collision_normal = gse::unitless::vec3(0.0f);

    for (int i = 0; i < axis_count; ++i) {
        gse::length penetration;
        if (!overlaps_on_axis(obb1, obb2, axes[i], penetration)) {
            return false; // Separating axis found, no collision
        }

        if (penetration < min_penetration) {
            min_penetration = penetration;
            collision_normal = normalize(axes[i]);
        }
    }

    // Ensure the collision normal points from obb1 to obb2
    if (const gse::unitless::vec3 direction = (obb2.center - obb1.center).as<gse::units::meters>(); gse::dot(direction, collision_normal) < 0.0f) {
        collision_normal *= -1;
    }

    return true; // No separating axis found, collision detected
}

struct plane {
	gse::unitless::vec3 normal;
	gse::length offset;
};

auto create_plane(const gse::vec3<gse::length>& point, const gse::unitless::vec3& edge_direction) -> plane {
	const auto normal = gse::normalize(gse::cross(edge_direction, gse::unitless::vec3(0.0f, 1.0f, 0.0f)));
    return {
        .normal = normal,
        .offset = gse::dot(normal, point.as<gse::units::meters>())
    };
}

auto clip_polygon_against_plane(const std::array<gse::vec3<gse::length>, 4>& polygon, const plane& plane) -> std::vector<gse::vec3<gse::length>> {
	std::vector<gse::vec3<gse::length>> clipped_polygon;

	const auto& normal = plane.normal;
	const auto& offset = plane.offset;

	for (size_t i = 0; i < polygon.size(); ++i) {
		const auto& current_vertex = polygon[i];
		const auto& next_vertex = polygon[(i + 1) % polygon.size()];
        const float current_distance = gse::dot(normal, current_vertex.as<gse::units::meters>()) - offset.as<gse::units::meters>();
		const float next_distance = gse::dot(normal, next_vertex.as<gse::units::meters>()) - offset.as<gse::units::meters>();

		if (current_distance >= 0) {
			clipped_polygon.push_back(current_vertex);
		}

		if (current_distance * next_distance < 0) {
			const float t = current_distance / (current_distance - next_distance);
			clipped_polygon.push_back(current_vertex + t * (next_vertex - current_vertex));
		}
	}

	return clipped_polygon;
}

auto compute_contact_point(const std::vector<gse::vec3<gse::length>>& clipped_polygon) -> gse::vec3<gse::length> {
	gse::vec3<gse::length> contact_point;

	for (const auto& vertex : clipped_polygon) {
		contact_point += vertex;
	}

	return contact_point / static_cast<float>(clipped_polygon.size());
}

auto gse::narrow_phase_collision::resolve_collision(physics::motion_component* object_motion_component, physics::collision_component& object_collision_component, const physics::collision_component& other_collision_component) -> void {
    if (!sat_collision(object_collision_component.oriented_bounding_box, other_collision_component.oriented_bounding_box, object_collision_component.collision_information.collision_normal, object_collision_component.collision_information.penetration)) {
        return;
    }

    object_collision_component.collision_information.colliding = true;

    const auto  collision_normal = object_collision_component.collision_information.collision_normal;
    const float penetration_depth = object_collision_component.collision_information.penetration.as_default_unit();

    const float velocity_into_surface = dot(object_motion_component->current_velocity.as<units::meters_per_second>(), collision_normal);
    const float acceleration_into_surface = dot(object_motion_component->current_acceleration.as<units::meters_per_second_squared>(), collision_normal);

    velocity& vel = object_motion_component->current_velocity[object_collision_component.collision_information.get_axis()];
    acceleration& acc = object_motion_component->current_acceleration[object_collision_component.collision_information.get_axis()];

    if (velocity_into_surface < 0.0f) {
        vel = 0.0f;
    }
    if (acceleration_into_surface < 0.0f) {
        acc = 0.0f;
    }

    // Correct position to remove overlap (with a tiny slop)
    constexpr float slop = 0.001f;
    const float corrected_penetration = std::max(penetration_depth - slop, 0.0f);
    const auto correction = collision_normal * meters(corrected_penetration);
    object_motion_component->current_position -= correction;

    if (collision_normal.y < 0.f) { // Normal here is inverted because the collision normal points from the other object to this object
        object_motion_component->airborne = false;
        object_motion_component->most_recent_y_collision = object_motion_component->current_position.y;
    }

	const auto contact_point = compute_contact_point(clip_polygon_against_plane(
        object_collision_component.oriented_bounding_box.get_face_vertices(object_collision_component.collision_information.get_axis(), true), 
        create_plane(object_motion_component->current_position, object_collision_component.collision_information.collision_normal)
    ));

	const auto r_a = contact_point - object_motion_component->current_position;
	const auto r_b = contact_point - other_collision_component.oriented_bounding_box.center;

	const auto contact_velocity = object_motion_component->current_velocity.as<units::meters_per_second>() + cross(object_motion_component->angular_velocity.as<units::radians_per_second>(), r_a.as<units::meters>());
	const float relative_velocity_along_normal = dot(contact_velocity, object_collision_component.collision_information.collision_normal);

	/*float denom = 1.f / object_motion_component->mass.as<units::kilograms>()
		+ dot(collision_normal, cross(object_motion_component->inverse_inertia_tensor * cross(r_a.as<units::meters>(), collision_normal), r_a.as<units::meters>())).as_default_unit()
		+ cross(other_collision_component.oriented_bounding_box.inverse_inertia_tensor * cross(r_b.as<units::meters>(), collision_normal), r_b.as<units::meters>()).as_default_unit();*/

	object_collision_component.collision_information.collision_point = contact_point;
	const auto lever_arm = contact_point - object_motion_component->current_position;
	const auto torque = cross(lever_arm.as<units::meters>(), object_collision_component.collision_information.collision_normal);
	object_motion_component->current_torque += gse::vec3<gse::torque>(torque) * 100.f;
}