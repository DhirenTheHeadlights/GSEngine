export module gse.physics.narrow_phase_collisions;

import std;

import gse.core.main_clock;
import gse.graphics.debug_rendering;
import gse.physics.motion_component;
import gse.physics.bounding_box;
import gse.physics.collision_component;
import gse.physics.math;

constexpr bool dynamic_contact_point_resolution = true;
constexpr int mpr_collision_refinement_iterations = 32;
//Whether to dynamically adjust the contact point based on the collision normal and the face of the OBB that was hit.



export namespace gse::narrow_phase_collision {
    auto resolve_dynamic_collision(physics::motion_component* object_motion_component, physics::collision_component& object_collision_component, physics::motion_component* other_motion_component, physics::collision_component& other_collision_component) -> void;
    auto resolve_static_collision(physics::motion_component* object_motion_component, physics::collision_component& object_collision_component, physics::collision_component& other_collision_component) -> void;
}




//SAT (Separating Axis Theorem) collision and helpers
struct sat_result {
    bool collided;
    gse::unitless::vec3 normal;
    gse::length penetration;
    int axis_index;    // 0–2 => obb1 face i, 3–5 => obb2 face (i?3), >=6 => edge?edge
};

auto overlaps_on_axis(const gse::oriented_bounding_box& box1, const gse::oriented_bounding_box& box2, const gse::vec3<gse::length>& axis, gse::length& penetration) -> bool {
    if (gse::is_zero(axis)) {
        return true;
    }

    const auto normalized_axis = gse::normalize(axis);
    const auto corners1 = box1.corners();

    auto project_point = [](const gse::vec3<gse::length>& point, const gse::vec3<gse::length>& projection_axis) -> gse::length {
        return gse::dot(point, projection_axis);
        };

    gse::length min1 = project_point(corners1[0], normalized_axis);
    gse::length max1 = min1;

    for (const auto& corner : corners1) {
        gse::length projection = project_point(corner, normalized_axis);
        min1 = std::min(min1, projection);
        max1 = std::max(max1, projection);
    }

    const auto corners2 = box2.corners();

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
auto sat_collision(const gse::oriented_bounding_box& obb1, const gse::oriented_bounding_box& obb2, sat_result& out) -> bool
{

	//std::cout << "Entered sat_collision\n";
	for (const auto& corner : obb1.corners()) {
		//std::cout << "OBB1 corner: " << corner.x.as_default_unit() << ", " << corner.y.as_default_unit() << ", " << corner.z.as_default_unit() << "\n";
	}
    //print obb1 axes
	for (const auto& axis : obb1.axes) {
		//std::cout << "OBB1 axis: " << axis.x << ", " << axis.y << ", " << axis.z << "\n";
	}
	for (const auto& corner : obb2.corners()) {
		//std::cout << "OBB2 corner: " << corner.x.as_default_unit() << ", " << corner.y.as_default_unit() << ", " << corner.z.as_default_unit() << "\n";
	}
	//print obb2 axes
	for (const auto& axis : obb2.axes) {
		//std::cout << "OBB2 axis: " << axis.x << ", " << axis.y << ", " << axis.z << "\n";
	}
    std::array<gse::vec3<gse::length>, 15> axes;
    int axis_count = 0;

    // 1) face normals of obb1
    for (int i = 0; i < 3; ++i) {
        axes[axis_count++] = obb1.axes[i];
    }
    // 2) face normals of obb2
    for (int i = 0; i < 3; ++i) {
        axes[axis_count++] = obb2.axes[i];
    }
    // 3) edge cross product
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            auto c = gse::cross(obb1.axes[i], (obb2.axes[j]));
            if (gse::dot(c, c) > 1e-6f) axes[axis_count++] = c;
        }
    }

    out.collided = true;
    out.penetration = gse::meters(std::numeric_limits<float>::max());
    out.normal = gse::unitless::vec3(0.0f);
    out.axis_index = -1;

    // Test all axes
    for (int i = 0; i < axis_count; ++i) {
        gse::length pen;
        if (!overlaps_on_axis(obb1, obb2, axes[i], pen)) {
            out.collided = false;
			//std::cout << "No overlap on axis " << i << "\n";
            return false;   // found a separating axis
        }
        if (pen < out.penetration) {
            out.penetration = pen;
            out.normal = gse::normalize(axes[i]);
            out.axis_index = i;
        }
    }

    // Ensure normal points from obb1 ? obb2
    {
        auto direction = (obb1.center - obb2.center).as<gse::units::meters>();
        if (gse::dot(direction, out.normal) < 0.0f) {
            out.normal *= -1.f;
        }
    }
	//std::cout << "Collision detected! Normal: " << out.normal.x << ", " << out.normal.y << ", " << out.normal.z << " Penetration: " << out.penetration.as_default_unit() << " Axis index: " << out.axis_index << "\n";
    return true;
}
auto get_obb_overlap_vertices(const gse::oriented_bounding_box& obb1, const gse::oriented_bounding_box& obb2, std::vector<gse::vec3<gse::length>>& overlap_vertices) {
	//find each vertex of obb1 that is inside obb2 and apnd it to overlap_vertices
    for (const auto& corner : obb1.corners()) {
        if (obb2.contains(corner)) {
            overlap_vertices.push_back(corner);
        }
	}
}

//MPR (Minkowski Portal Refinement) collision and helpers
struct mpr_result {
    bool collided = false;
    gse::unitless::vec3 normal;
    gse::length penetration;
    gse::vec3<gse::length> contact_point1; // Contact point on object 1
    gse::vec3<gse::length> contact_point2; // Contact point on object 2
};
auto support_obb(const gse::oriented_bounding_box& obb, const gse::unitless::vec3& dir) -> gse::vec3<gse::length> {
    gse::vec3<gse::length> result = obb.center;
    const auto half_extents = obb.get_half_extents();

    for (int i = 0; i < 3; ++i) {
        float sign = (gse::dot(dir, obb.axes[i]) > 0) ? 1.0f : -1.0f;
        result += obb.axes[i] * half_extents[i] * sign;
    }
    return result;
}
auto minkowski_difference(const gse::oriented_bounding_box& obb1, const gse::oriented_bounding_box& obb2, const gse::unitless::vec3& dir) -> gse::vec3<gse::length> {
    return support_obb(obb1, dir) - support_obb(obb2, -dir);
}
bool mpr_collision(const gse::oriented_bounding_box& obb1, const gse::oriented_bounding_box& obb2, mpr_result& out) {
    // Phase 1: Portal Discovery
    gse::vec3<gse::length> v0 = obb1.center - obb2.center;
    if (gse::is_zero(v0)) {
        v0 = gse::vec3<gse::length>{ 0.0001, 0, 0 };
    }

    gse::unitless::vec3 n = gse::normalize(-v0);
    gse::vec3<gse::length> v1 = minkowski_difference(obb1, obb2, n);

    if (gse::dot(v1.as<gse::units::meters>(), n) <= 0) return false;

    n = gse::normalize(gse::cross(v1, v0));
    if (gse::is_zero(n)) {
        n = gse::normalize(gse::vec3<gse::length>{v1.y, -v1.x, 0});
        if (gse::is_zero(n)) n = gse::unitless::vec3{ 1, 0, 0 };
    }
    gse::vec3<gse::length> v2 = minkowski_difference(obb1, obb2, n);

    if (gse::dot(v2.as<gse::units::meters>(), n) <= 0) return false;

    // Make sure normal points towards the origin from the portal plane
    n = gse::normalize(gse::cross(v1 - v0, v2 - v0));
    if (gse::dot(n, -v0.as<gse::units::meters>()) < 0) {
        n = -n;
    }

    // Phase 2: Portal Refinement
    for (int i = 0; i < mpr_collision_refinement_iterations; ++i) {
        gse::vec3<gse::length> v3 = minkowski_difference(obb1, obb2, n);

        const auto portal_dist = gse::dot(v1, gse::vec3<gse::length>(n));
        const auto support_dist = gse::dot(v3, gse::vec3<gse::length>(n));

        // Standard termination: the new point is not significantly further than the portal
        if (support_dist.as_default_unit() <= portal_dist.as_default_unit() + 0.0001f) {
            out.collided = true;
            out.normal = n;
            out.penetration = support_dist;
            out.contact_point2 = support_obb(obb2, -out.normal);
            out.contact_point1 = out.contact_point2 + out.normal * out.penetration;
            return true;
        }

        // Refine the portal. Using dot products with v0 directly is more stable
        // than using the full cross product with mixed unit types.
        if (gse::dot(gse::cross(v1.as<gse::units::meters>(), v3.as<gse::units::meters>()), v0.as<gse::units::meters>()) < 0) {
            v2 = v3;
        }
        else if (gse::dot(gse::cross(v3.as<gse::units::meters>(), v2.as<gse::units::meters>()), v0.as<gse::units::meters>()) < 0) {
            v1 = v3;
        }
        else {
            // *** THE FIX ***
            // This block is hit when the origin is contained by the portal's sweep.
            // This is a valid termination condition. We set the results and return.
            out.collided = true;
            out.normal = n;
            out.penetration = support_dist;
            out.contact_point2 = support_obb(obb2, -out.normal);
            out.contact_point1 = out.contact_point2 + out.normal * out.penetration;
            return true;
        }

        n = gse::normalize(gse::cross(v2 - v0, v1 - v0));
        // Always ensure the new normal points towards the origin
        if (gse::dot(n, -v0.as<gse::units::meters>()) < 0) {
            n = -n;
        }
    }

    return false; // Failed to converge
}


struct plane {
	gse::unitless::vec3 normal;
	gse::length offset;
};

auto create_plane(const gse::vec3<gse::length>& p,
    const gse::unitless::vec3& collision_normal) -> plane {
    // if n is almost vertical, cross with X; else cross with Y
    gse::unitless::vec3 helper =
        (fabs(collision_normal.y) > 0.99 ? gse::unitless::vec3{ 1,0,0 }  : gse::unitless::vec3{ 0,1,0 });
    gse::unitless::vec3 plane_n = gse::normalize(gse::cross(collision_normal, helper));
    return { plane_n,
             gse::dot(plane_n, p.as<gse::units::meters>()) };
}

auto clip_polygon_against_plane(const std::array<gse::vec3<gse::length>, 4>& polygon, const plane& c_plane) -> std::vector<gse::vec3<gse::length>> {
	std::vector<gse::vec3<gse::length>> clipped_polygon;

	const auto& normal = c_plane.normal;
	const auto& offset = c_plane.offset;

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

    return clipped_polygon.empty() ? gse::vec3<gse::length>{ 0.f, 0.f, 0.f } : (contact_point / static_cast<float>(clipped_polygon.size()));
}

auto gse::narrow_phase_collision::resolve_dynamic_collision(physics::motion_component* object_motion_component, physics::collision_component& object_collision_component, physics::motion_component* other_motion_component, physics::collision_component& other_collision_component) -> void {
    sat_result sat_res;
    mpr_result mpr_res;
    std::vector<gse::vec3<gse::length>> contact_points;
    /*if (!sat_collision(object_collision_component.oriented_bounding_box, other_collision_component.oriented_bounding_box, sat_res)) {
        return;
    }*/
    if (!mpr_collision(object_collision_component.oriented_bounding_box, other_collision_component.oriented_bounding_box, mpr_res)) {
        return;
	}
    if (object_motion_component->position_locked){
        return;
    }

    //get_obb_overlap_vertices(object_collision_component.oriented_bounding_box, other_collision_component.oriented_bounding_box, contact_points);

    object_collision_component.collision_information.colliding = mpr_res.collided;
    object_collision_component.collision_information.collision_normal = -mpr_res.normal;
    object_collision_component.collision_information.penetration = mpr_res.penetration;

    const auto  collision_normal = object_collision_component.collision_information.collision_normal;
    const float penetration_depth = object_collision_component.collision_information.penetration.as_default_unit() * 0.1f;

    const float velocity_into_surface = dot(object_motion_component->current_velocity.as<units::meters_per_second>(), collision_normal);
    const float acceleration_into_surface = dot(object_motion_component->current_acceleration.as<units::meters_per_second_squared>(), collision_normal);

    velocity& vel = object_motion_component->current_velocity[object_collision_component.collision_information.get_axis()];
    acceleration& acc = object_motion_component->current_acceleration[object_collision_component.collision_information.get_axis()];

    const float inv_mass_a = 1.f / object_motion_component->mass.as<units::kilograms>();
	const float inv_mass_b = 1.f / other_motion_component->mass.as<units::kilograms>();

	const float total_inv_mass = inv_mass_a + inv_mass_b;
	const float percentage_a = inv_mass_a / total_inv_mass;
	const float percentage_b = inv_mass_b / total_inv_mass;

    // Correct position to remove overlap (with a tiny slop)
    constexpr float slop = 0.001f;
    const float corrected_penetration = std::max(penetration_depth - slop, 0.0f);

    const auto correction_a = -collision_normal * meters(corrected_penetration * percentage_a);
	const auto correction_b = -collision_normal * meters(corrected_penetration * percentage_b);
    object_motion_component->current_position -= correction_a;
    if (!other_motion_component->position_locked) { other_motion_component->current_position += correction_b; }

    if (collision_normal.y > 0.f) { // Normal here is inverted because the collision normal points from the other object to this object
        object_motion_component->airborne = false;
        object_motion_component->most_recent_y_collision = object_motion_component->current_position.y;
    }

    // final contact point
    gse::vec3<gse::length> main_contact_point = mpr_res.contact_point1;

    //SETTINGS DEPENDENT- PERFORMANT
    //if (dynamic_contact_point_resolution) {
    //    /////////////////////////////////////
    //    int  face_axis;
    //    bool edge_edge_sim = false;
    //    bool box_a_face = false;
    //    auto* ref_obb = &object_collision_component.oriented_bounding_box;

    //    // decide which face we hit (or fall back to an edge–edge sim)
    //    if (sat_res.axis_index < 3) {
    //        face_axis = sat_res.axis_index;
    //        box_a_face = true;
    //        ref_obb = &object_collision_component.oriented_bounding_box;
    //    }

    //    else if (sat_res.axis_index < 6) {
    //        face_axis = sat_res.axis_index - 3;
    //        ref_obb = &other_collision_component.oriented_bounding_box;
    //    }

    //    else {
    //        // pure edge–edge: just pick any axis for fallback
    //        edge_edge_sim = true;
    //        face_axis = static_cast<int>(object_collision_component.collision_information.get_axis());
    //        ref_obb = &object_collision_component.oriented_bounding_box;
    //    }

    //    // get the geometric center of that face
    //    auto face_center = ref_obb->center;
    //    auto n = sat_res.normal;
    //    float offset_val = gse::dot(n, face_center.as<gse::units::meters>());
    //    plane clip_plane{ n, gse::meters(offset_val) };

    //    // build the face polygon for clipping
    //    bool positive = (n[face_axis] >= 0.f);
    //    auto face_verts = ref_obb->face_vertices(static_cast<gse::axis>(face_axis), positive);

    //    if (edge_edge_sim) {
    //        // project the face center onto the plane
    //        float d = gse::dot(n, face_center.as<gse::units::meters>())
    //            - clip_plane.offset.as<gse::units::meters>();
    //        main_contact_point = face_center - n * gse::meters(d);
    //    }

    //    else {

    //        auto clipped = clip_polygon_against_plane(face_verts, clip_plane);

    //        if (clipped.size() >= 3) {
    //            // use the centroid of the clipped polygon
    //            main_contact_point = compute_contact_point(clipped);
    //        }

    //        else {
    //            // degenerate: fall back to projecting the face center
    //            float d = gse::dot(n, face_center.as<gse::units::meters>())
    //                - clip_plane.offset.as<gse::units::meters>();
    //            main_contact_point = face_center - n * gse::meters(d);
    //        }
    //    }
    //}
    //////////////////////////////
    //else {
    //    main_contact_point = compute_contact_point(clip_polygon_against_plane(
    //        object_collision_component.oriented_bounding_box.face_vertices(object_collision_component.collision_information.get_axis(), true),
    //        create_plane(object_motion_component->current_position, object_collision_component.collision_information.collision_normal)
    //    ));
    //}

    contact_points.push_back(main_contact_point);

	object_collision_component.collision_information.collision_point = main_contact_point;
	gse::debug_renderer::add_debug_point(object_collision_component.parent_id, main_contact_point);

    for (auto& contact_point : contact_points) {

        const auto r_a = contact_point - object_motion_component->current_position;
		const auto r_b = contact_point - other_motion_component->current_position;
        const mat3 inv_i_a = object_motion_component->get_inverse_inertia_tensor_world();
        const mat3 inv_i_b = other_motion_component->get_inverse_inertia_tensor_world();

        const auto rcross_a = cross(r_a.as<units::meters>(), collision_normal);
        const auto rcross_b = cross(r_b.as<units::meters>(), collision_normal);


        const auto contact_velocity_a = object_motion_component->current_velocity.as<units::meters_per_second>() + cross(object_motion_component->angular_velocity.as<units::radians_per_second>(), r_a.as<units::meters>());
        const auto contact_velocity_b = other_motion_component->current_velocity.as<units::meters_per_second>() + cross(other_motion_component->angular_velocity.as<units::radians_per_second>(), r_b.as<units::meters>());
        const auto contact_velocity = contact_velocity_a - contact_velocity_b;
        const float relative_velocity_along_normal = dot(contact_velocity, object_collision_component.collision_information.collision_normal);

        const float rot_term_a = dot(
            collision_normal,
            cross(inv_i_a * rcross_a, r_a.as<units::meters>())
        );

        const float rot_term_b = other_motion_component->position_locked ? 0.f : dot(
            collision_normal,
            cross(inv_i_b * rcross_b, r_b.as<units::meters>())
        );

        /////// FOR USE WITH NON-STATIC OTHER OBJECT; REQUIRES OBJECT B MOTION COMPONENT
        constexpr const float epsilon = 0.00001f; // small value to prevent division by zero
        const float denom = std::max(epsilon, inv_mass_a + inv_mass_b + rot_term_a + rot_term_b);

        // Arbitrary coefficient; should be based off material.
        const float restitution = 0.5f;

        const gse::time dt = gse::main_clock::get_constant_update_time();
        if (relative_velocity_along_normal < 0.0f) {
            const float jn = -(1.f + restitution) * relative_velocity_along_normal / denom;
            auto impulse = collision_normal * jn;
            object_motion_component->current_velocity += gse::vec3<gse::velocity>(impulse * inv_mass_a);
            

            // angular impulse -> torque accumulator
             // Apply angular impulse directly to angular velocity
            const auto angular_impulse_a = cross(r_a.as<units::meters>(), impulse);
            object_motion_component->angular_velocity += gse::vec3<gse::angular_velocity>(inv_i_a * angular_impulse_a);

            // Optional: Debug draw the impulse vector itself, not a derived torque
            gse::debug_renderer::add_debug_vector(object_collision_component.parent_id, contact_point, impulse);

            if (!other_motion_component->position_locked) {
                // For object B, the impulse is negative
                const auto angular_impulse_b = cross(r_b.as<units::meters>(), -impulse);
                other_motion_component->angular_velocity += gse::vec3<gse::angular_velocity>(inv_i_b * angular_impulse_b);
				other_motion_component->current_velocity -= gse::vec3<gse::velocity>(impulse * inv_mass_b);
                // Optional: Debug draw the impulse vector
                gse::debug_renderer::add_debug_vector(other_collision_component.parent_id, contact_point, -impulse);
            }


        }
    }

}

auto gse::narrow_phase_collision::resolve_static_collision(physics::motion_component* object_motion_component, physics::collision_component& object_collision_component, physics::collision_component& other_collision_component) -> void {
	sat_result sat_res;
    std::vector<gse::vec3<gse::length>> contact_points;

    if (!sat_collision(object_collision_component.oriented_bounding_box, other_collision_component.oriented_bounding_box, sat_res)) {
        return;
    }
    if (object_motion_component->position_locked) {
        return;
    }

    get_obb_overlap_vertices(object_collision_component.oriented_bounding_box, other_collision_component.oriented_bounding_box, contact_points);

    object_collision_component.collision_information.colliding = true;
    object_collision_component.collision_information.collision_normal = sat_res.normal;
    object_collision_component.collision_information.penetration = sat_res.penetration;

    const auto  collision_normal = object_collision_component.collision_information.collision_normal;
    const float penetration_depth = object_collision_component.collision_information.penetration.as_default_unit();

    const float velocity_into_surface = dot(object_motion_component->current_velocity.as<units::meters_per_second>(), collision_normal);
    const float acceleration_into_surface = dot(object_motion_component->current_acceleration.as<units::meters_per_second_squared>(), collision_normal);

    velocity& vel = object_motion_component->current_velocity[object_collision_component.collision_information.get_axis()];
    acceleration& acc = object_motion_component->current_acceleration[object_collision_component.collision_information.get_axis()];

    const float inv_mass = 1.f / object_motion_component->mass.as<units::kilograms>();

    // Correct position to remove overlap (with a tiny slop)
    constexpr float slop = 0.001f;
    const float corrected_penetration = std::max(penetration_depth - slop, 0.0f);

    const auto correction_a = -collision_normal  * meters(corrected_penetration);
    object_motion_component->current_position -= correction_a;

    if (collision_normal.y > 0.f) { // Normal here is inverted because the collision normal points from the other object to this object
        object_motion_component->airborne = false;
        object_motion_component->most_recent_y_collision = object_motion_component->current_position.y;
    }

	// final contact point
	gse::vec3<gse::length> main_contact_point;

	//SETTINGS DEPENDENT- PERFORMANT
    if (dynamic_contact_point_resolution) {
	    /////////////////////////////////////
	    int  face_axis = sat_res.axis_index - 3;
	    bool edge_edge_sim = false;
	    bool box_a_face = false;
	    auto* ref_obb = &object_collision_component.oriented_bounding_box;
		auto* incident_obb = &other_collision_component.oriented_bounding_box;

        if (sat_res.axis_index < 3) {
            ref_obb = &object_collision_component.oriented_bounding_box;
            incident_obb = &other_collision_component.oriented_bounding_box;
            face_axis = sat_res.axis_index;
        }
        else {
            // pure edge–edge: just pick any axis for fallback
            edge_edge_sim = true;
            face_axis = static_cast<int>(object_collision_component.collision_information.get_axis());
        }

		unitless::vec3 ref_face_normal = ref_obb->axes[face_axis];

	    // get the geometric center of that face
	    auto face_center = ref_obb->center;
	    auto n = sat_res.normal;
	    float offset_val = gse::dot(n, face_center.as<gse::units::meters>());
	    plane clip_plane{ n, gse::meters(offset_val) };

	    // build the face polygon for clipping
	    bool positive = (n[face_axis] >= 0.f);
	    auto face_verts = ref_obb->face_vertices(static_cast<gse::axis>(face_axis), positive);

	    

	    if (edge_edge_sim) {
	        // project the face center onto the plane
	        float d = gse::dot(n, face_center.as<gse::units::meters>())
	            - clip_plane.offset.as<gse::units::meters>();
	        main_contact_point = face_center - n * gse::meters(d);
	    }
	    else {
	        auto clipped = clip_polygon_against_plane(face_verts, clip_plane);
	        if (clipped.size() >= 3) {
	            // use the centroid of the clipped polygon
	            main_contact_point = compute_contact_point(clipped);
	        }
	        else {
	            // degenerate: fall back to projecting the face center
	            float d = gse::dot(n, face_center.as<gse::units::meters>())
	                - clip_plane.offset.as<gse::units::meters>();
	            main_contact_point = face_center - n * gse::meters(d);
	        }
	    }
    }
    ////////////////////////////
    else {
    	main_contact_point = compute_contact_point(clip_polygon_against_plane(
		object_collision_component.oriented_bounding_box.face_vertices(object_collision_component.collision_information.get_axis(), true),
        create_plane(object_motion_component->current_position, object_collision_component.collision_information.collision_normal)
        ));
    }
   
	contact_points.push_back(main_contact_point);

    for (auto& contact_point : contact_points){

	    const auto r_a = contact_point - object_motion_component->current_position;


	    const mat3 inv_inertial_tensor = object_motion_component->get_inverse_inertia_tensor_world();

	    const auto rcross = cross(r_a.as<units::meters>(), collision_normal);

	    const auto contact_velocity = object_motion_component->current_velocity.as<units::meters_per_second>() + cross(object_motion_component->angular_velocity.as<units::radians_per_second>(), r_a.as<units::meters>());
	    const float relative_velocity_along_normal = dot(contact_velocity, object_collision_component.collision_information.collision_normal);

	    const float rot_term_a = dot(
	        collision_normal,
	        cross(inv_inertial_tensor * rcross, r_a.as<units::meters>())
	    );

	    /////// FOR USE WITH NON-STATIC OTHER OBJECT; REQUIRES OBJECT B MOTION COMPONENT
	    constexpr const float epsilon = 0.00001f; // small value to prevent division by zero
	    const float rot_term_b = 0.f;

	    const float denom =
	        std::max(epsilon, inv_mass + rot_term_a);

	    // Arbitrary coefficient; should be based off material.
	    const float restitution = 0.5f;

        const gse::time dt = gse::main_clock::get_constant_update_time();
    	if (relative_velocity_along_normal < 0.0f)
        {
            // ----- normal impulse -----
            const float jn = -(1.f + restitution) * relative_velocity_along_normal / denom;
            auto impulse = collision_normal * jn;                
            object_motion_component->current_velocity += gse::vec3<gse::velocity>(impulse * inv_mass);

            // angular impulse -> torque accumulator
            const auto angular_impulse = cross(r_a.as<units::meters>(), impulse);
            object_motion_component->angular_velocity += gse::vec3<gse::angular_velocity>(inv_inertial_tensor * angular_impulse);

            // Optional: Debug draw the impulse vector itself, not a derived torque
            gse::debug_renderer::add_debug_vector(object_collision_component.parent_id, contact_point, impulse);
        }

	}

    object_collision_component.collision_information.collision_point = main_contact_point;
    gse::debug_renderer::add_debug_point(object_collision_component.parent_id, main_contact_point);

	//if (velocity_into_surface < 0.0f) {
 //   vel = 0.0f;
 //   }
 //   if (acceleration_into_surface < 0.0f) {
 //       acc = 0.0f;
 //   }

}