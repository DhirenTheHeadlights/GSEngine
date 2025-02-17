export module gse.physics.narrow_phase_collisions;

import std;

import gse.core.registry;
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

auto gse::narrow_phase_collision::resolve_collision(
    physics::motion_component* object_motion_component,
    physics::collision_component& object_collision_component,
    const physics::collision_component& other_collision_component
) -> void {
    // First, check for collision using SAT
    if (!sat_collision(object_collision_component.oriented_bounding_box,
        other_collision_component.oriented_bounding_box,
        object_collision_component.collision_information.collision_normal,
        object_collision_component.collision_information.penetration)) {
        return;
    }

    object_collision_component.collision_information.colliding = true;

    const auto collision_normal = object_collision_component.collision_information.collision_normal;
    const float penetration_depth = object_collision_component.collision_information.penetration.as_default_unit();

    // Remove any velocity/acceleration component into the surface along the collision axis.
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

    // Correct position to remove overlap (with a small slop)
    constexpr float slop = 0.001f;
    const float corrected_penetration = std::max(penetration_depth - slop, 0.0f);
    const auto correction = collision_normal * meters(corrected_penetration);
    object_motion_component->current_position -= correction;

    // If collision normal indicates an upward collision, mark as grounded.
    if (collision_normal.y < 0.f) { // Collision normal points from other object to this object.
        object_motion_component->airborne = false;
        object_motion_component->most_recent_y_collision = object_motion_component->current_position.y;
    }

    // Compute the contact point. This uses a helper that clips the face vertices against a plane defined at the current position.
    const auto contact_point = compute_contact_point(
        clip_polygon_against_plane(
            object_collision_component.oriented_bounding_box.get_face_vertices(object_collision_component.collision_information.get_axis(), true),
            create_plane(object_motion_component->current_position, object_collision_component.collision_information.collision_normal)
        )
    );

    // Compute the relative vectors from each object's center to the contact point.
    const auto r_a = contact_point - object_motion_component->current_position;
    const auto r_b = contact_point - other_collision_component.oriented_bounding_box.center;

    // Compute the velocity at the contact point for the first object (linear + rotational).
    const auto contact_velocity = object_motion_component->current_velocity.as<units::meters_per_second>()
        + cross(object_motion_component->angular_velocity.as<units::radians_per_second>(), r_a.as<units::meters>());
    const float relative_velocity_along_normal = dot(contact_velocity, collision_normal);

    // Get the other object's motion component.
    const auto other_motion_component = gse::registry::get_component_ptr<physics::motion_component>(other_collision_component.parent_id);
    if (!other_motion_component || is_zero(object_motion_component->inertial_tensor)) {
        return;
    }

    // Compute the effective mass (denominator) for the impulse calculation.
    // This includes the inverse masses and the rotational contributions.
    const float inv_mass_a = 1.f / object_motion_component->mass.as<units::kilograms>();
    const float inv_mass_b = 1.f / other_motion_component->mass.as<units::kilograms>();
    const float term_a = dot(collision_normal,
        cross(object_motion_component->inertial_tensor.inverse() *
            cross(r_a.as<units::meters>(), collision_normal),
            r_a.as<units::meters>()));
    const float term_b = dot(collision_normal,
		cross((is_zero(other_motion_component->inertial_tensor) ? mat3(0.f) : other_motion_component->inertial_tensor.inverse()) *
            cross(r_b.as<units::meters>(), collision_normal),
            r_b.as<units::meters>()));
    const float denom = inv_mass_a + inv_mass_b + term_a + term_b;

    // Compute the impulse magnitude.
    // Here, 'restitution' defines how "bouncy" the collision is.
    constexpr float restitution = 0.5f; // Adjust as needed.
    const float impulse_magnitude = -(1.f + restitution) * relative_velocity_along_normal / denom;
    const auto impulse = impulse_magnitude * collision_normal;

    // Update angular velocities using the impulse and the lever arms.
    object_motion_component->angular_velocity += vec3<angular_velocity>(object_motion_component->inertial_tensor.inverse() *
        cross(r_a.as<units::meters>(), impulse));

    // (Optional) Apply an extra torque correction if desired.
    const auto lever_arm = contact_point - object_motion_component->current_position;
    const auto torque = cross(lever_arm.as<units::meters>(), collision_normal);
    object_motion_component->current_torque += gse::vec3<gse::torque>(torque) * 100.f;

    // Store the contact point for debugging or further processing.
    object_collision_component.collision_information.collision_point = contact_point;
}
