export module gse.physics.narrow_phase_collisions;

import std;

import gse.physics.motion_component;
import gse.physics.bounding_box;
import gse.physics.collision_component;
import gse.physics.math.vector;
import gse.physics.math.vector_math;
import gse.physics.math.units;

export namespace gse::narrow_phase_collision {
	auto resolve_collision(physics::motion_component* object_motion_component, physics::collision_component& object_collision_component, const physics::collision_component& other_collision_component) -> void;
}

auto overlaps_on_axis(const gse::oriented_bounding_box& box1, const gse::oriented_bounding_box& box2, const gse::vec3<gse::length>& axis, gse::length& penetration) -> bool {
    if (is_zero(axis)) {
        return true;
    }

    const auto normalized_axis = normalize(axis);
    const auto corners1 = box1.get_corners();

    auto project_point = [](const gse::vec3<gse::length>& point, const gse::vec3<gse::length>& projection_axis) -> float {
        return dot(point, projection_axis);
        };

    float min1 = project_point(corners1[0], normalized_axis);
    float max1 = min1;

    for (const auto& corner : corners1) {
        float projection = project_point(corner, normalized_axis);
        min1 = std::min(min1, projection);
        max1 = std::max(max1, projection);
    }

    const auto corners2 = box2.get_corners();

    float min2 = project_point(corners2[0], normalized_axis);
    float max2 = min2;

    for (const auto& corner : corners2) {
        float projection = project_point(corner, normalized_axis);
        min2 = std::min(min2, projection);
        max2 = std::max(max2, projection);
    }

    if (max1 < min2 || max2 < min1) {
        return false;
    }

    const auto overlap = gse::meters(std::min(max1, max2) - std::max(min1, min2));
    penetration = overlap;
    return true;
}

auto sat_collision(const gse::oriented_bounding_box& obb1, const gse::oriented_bounding_box& obb2, gse::vec3<>& collision_normal, gse::length& min_penetration) -> bool {
    std::array<gse::vec3<gse::units::meters>, 15> axes;
    int axis_count = 0;

    for (int i = 0; i < 3; ++i) {
        axes[axis_count++] = obb1.axes[i];
    }

    for (int i = 0; i < 3; ++i) {
        axes[axis_count++] = obb2.axes[i];
    }

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (gse::vec3<gse::length> cross = gse::cross(obb1.axes[i], obb2.axes[j]); magnitude(cross) > gse::meters(1e-6f)) { // Avoid near-zero vectors
                axes[axis_count++] = cross;
            }
        }
    }

    min_penetration = gse::meters(std::numeric_limits<float>::max());
    collision_normal = gse::vec3(0.0f);

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
    if (const gse::vec3<gse::length> direction = obb2.center - obb1.center; dot(direction, collision_normal) < 0.0f) {
        collision_normal = -collision_normal;
    }

    return true; // No separating axis found, collision detected
}

auto gse::narrow_phase_collision::resolve_collision(physics::motion_component* object_motion_component, physics::collision_component& object_collision_component, const physics::collision_component& other_collision_component) -> void {
    if (!sat_collision(object_collision_component.oriented_bounding_box, other_collision_component.oriented_bounding_box, object_collision_component.collision_information.collision_normal, object_collision_component.collision_information.penetration)) {
        return;
    }

    object_collision_component.collision_information.colliding = true;

    const auto  collision_normal = object_collision_component.collision_information.collision_normal;
    const float penetration_depth = object_collision_component.collision_information.penetration.as_default_unit();

    const float velocity_into_surface = dot(object_motion_component->current_velocity, collision_normal);
    const float acceleration_into_surface = dot(object_motion_component->current_acceleration, collision_normal);

    float& vel = object_motion_component->current_velocity.as_default_units()[object_collision_component.collision_information.get_axis()];
    float& acc = object_motion_component->current_acceleration.as_default_units()[object_collision_component.collision_information.get_axis()];

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

    if (collision_normal.as_default_units().y < 0.f) { // Normal here is inverted because the collision normal points from the other object to this object
        object_motion_component->airborne = false;
        object_motion_component->most_recent_y_collision = meters(object_motion_component->current_position.as_default_units().y);
    }
}
