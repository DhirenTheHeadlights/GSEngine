export module gse.physics.narrow_phase_collisions;

import std;

import gse.graphics.debug_rendering;
import gse.physics.motion_component;
import gse.physics.bounding_box;
import gse.physics.collision_component;
import gse.physics.math;

export namespace gse::narrow_phase_collision {
    auto resolve_dynamic_collision(physics::motion_component* object_motion_component, physics::collision_component& object_collision_component, physics::motion_component* other_motion_component, physics::collision_component& other_collision_component) -> void;
    auto resolve_static_collision(physics::motion_component* object_motion_component, physics::collision_component& object_collision_component, physics::collision_component& other_collision_component) -> void;
}

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

struct sat_result {
    bool collided;
    gse::unitless::vec3 normal;
    gse::length penetration;
    int axis_index;    // 0–2 => obb1 face i, 3–5 => obb2 face (i?3), >=6 => edge?edge
};

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
    if (!sat_collision(object_collision_component.oriented_bounding_box, other_collision_component.oriented_bounding_box, sat_res)) {
        return;
    }
    if (object_motion_component->position_locked){
        return;
    }

    object_collision_component.collision_information.colliding = true;
    object_collision_component.collision_information.collision_normal = sat_res.normal;
    object_collision_component.collision_information.penetration = sat_res.penetration;

    const auto  collision_normal = object_collision_component.collision_information.collision_normal;
    const float penetration_depth = object_collision_component.collision_information.penetration.as_default_unit() * 0.1f;

	//std::cout << "collision normal: " << collision_normal.x << ", " << collision_normal.y << ", " << collision_normal.z << "\n";

    const float velocity_into_surface = dot(object_motion_component->current_velocity.as<units::meters_per_second>(), collision_normal);
    const float acceleration_into_surface = dot(object_motion_component->current_acceleration.as<units::meters_per_second_squared>(), collision_normal);

	//std::cout << "velocity into surface: " << velocity_into_surface << "\n";
	//std::cout << "acceleration into surface: " << acceleration_into_surface << "\n";

    velocity& vel = object_motion_component->current_velocity[object_collision_component.collision_information.get_axis()];
    acceleration& acc = object_motion_component->current_acceleration[object_collision_component.collision_information.get_axis()];

	//std::cout << "pre-collision velocity: " << object_motion_component->current_velocity.x.as_default_unit() << ", " << object_motion_component->current_velocity.y.as_default_unit() << ", " << object_motion_component->current_velocity.z.as_default_unit() << "\n";
	//std::cout << "pre-collision acceleration: " << object_motion_component->current_acceleration.x.as_default_unit() << ", " << object_motion_component->current_acceleration.y.as_default_unit() << ", " << object_motion_component->current_acceleration.z.as_default_unit() << "\n";

    if (velocity_into_surface < 0.0f) {
        vel = 0.0f;
    }
    if (acceleration_into_surface < 0.0f) {
        acc = 0.0f;
    }

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
    if (other_motion_component->position_locked) { other_motion_component->current_position += correction_b; }


    if (collision_normal.y < 0.f) { // Normal here is inverted because the collision normal points from the other object to this object
        object_motion_component->airborne = false;
        object_motion_component->most_recent_y_collision = object_motion_component->current_position.y;
    }

    int  face_axis;
    bool edge_edge_sim = false;
    bool box_a_face = false;
    auto* ref_obb = &object_collision_component.oriented_bounding_box;

    // decide which face we hit (or fall back to an edge–edge sim)
    if (sat_res.axis_index < 3) {
        face_axis = sat_res.axis_index;
        box_a_face = true;
        ref_obb = &object_collision_component.oriented_bounding_box;
    }
    else if (sat_res.axis_index < 6) {
        face_axis = sat_res.axis_index - 3;
        ref_obb = &other_collision_component.oriented_bounding_box;
    }
    else {
        // pure edge–edge: just pick any axis for fallback
        edge_edge_sim = true;
        face_axis = static_cast<int>(object_collision_component.collision_information.get_axis());
        ref_obb = &object_collision_component.oriented_bounding_box;
    }

    // get the geometric center of that face
    auto face_center = ref_obb->center;
    auto n = sat_res.normal;
    float offset_val = gse::dot(n, face_center.as<gse::units::meters>());
    plane clip_plane{ n, gse::meters(offset_val) };

    // build the face polygon for clipping
    bool positive = (n[face_axis] >= 0.f);
    auto face_verts = ref_obb->face_vertices(static_cast<gse::axis>(face_axis), positive);

    // final contact point
    gse::vec3<gse::length> contact_point;

    if (edge_edge_sim) {
        // project the face center onto the plane
        float d = gse::dot(n, face_center.as<gse::units::meters>()) - clip_plane.offset.as<gse::units::meters>();
        contact_point = face_center - n * gse::meters(d);
    }
    else {
        auto clipped = clip_polygon_against_plane(face_verts, clip_plane);
        if (clipped.size() >= 3) {
            // use the centroid of the clipped polygon
            contact_point = compute_contact_point(clipped);
        }
        else {
            // degenerate: fall back to projecting the face center
            float d = gse::dot(n, face_center.as<gse::units::meters>()) - clip_plane.offset.as<gse::units::meters>();
            contact_point = face_center - n * gse::meters(d);
        }
    }

    object_collision_component.collision_information.collision_point = contact_point;
	gse::debug_renderer::add_debug_point(object_collision_component.parent_id, contact_point);
	//std::cout << "contact point: " << contact_point.x.as_default_unit() << ", " << contact_point.y.as_default_unit() << ", " << contact_point.z.as_default_unit() << "\n";

	const auto r_a = contact_point - object_motion_component->current_position;
	const auto r_b = contact_point - other_collision_component.oriented_bounding_box.center;
    const mat3 inv_i_a = object_motion_component->get_inverse_inertia_tensor_world();
	const mat3 inv_i_b = other_motion_component->get_inverse_inertia_tensor_world();

    const auto rcross_a = cross(r_a.as<units::meters>(), collision_normal);
    const auto rcross_b = cross(r_b.as<units::meters>(), collision_normal);


	const auto contact_velocity_a = object_motion_component->current_velocity.as<units::meters_per_second>() + cross(object_motion_component->angular_velocity.as<units::radians_per_second>(), r_a.as<units::meters>());
	const auto contact_velocity_b = other_motion_component->current_velocity.as<units::meters_per_second>() + cross(other_motion_component->angular_velocity.as<units::radians_per_second>(), r_b.as<units::meters>());
	const auto contact_velocity = contact_velocity_a - contact_velocity_b;
	const float relative_velocity_along_normal = dot(contact_velocity, object_collision_component.collision_information.collision_normal);

    std::cout << "contact velocity a: " << contact_velocity_a.x << ", " << contact_velocity_a.y << ", " << contact_velocity_a.z << "\n";
    std::cout << "contact velocity b: " << contact_velocity_b.x << ", " << contact_velocity_b.y << ", " << contact_velocity_b.z << "\n";
    std::cout << "contact normal : " << object_collision_component.collision_information.collision_normal.x << ", "
        << object_collision_component.collision_information.collision_normal.y << ", "
        << object_collision_component.collision_information.collision_normal.z << "\n";

    std::cout << "Relative velocity along normal: " << relative_velocity_along_normal << "\n";

    const float rot_term_a = dot(
        collision_normal,
        cross(inv_i_a * rcross_a, r_a.as<units::meters>())
    );

    const float rot_term_b = other_motion_component->position_locked ? 0.f : dot(
        collision_normal,
        cross(inv_i_b * rcross_b, r_b.as<units::meters>())
    );

    const float denom =
        1.f / object_motion_component->mass.as<units::kilograms>()
        + rot_term_a
        + rot_term_b;


    // Arbitrary coefficient; should be based off material.
    const float restitution = 0.5f;

 

    if (relative_velocity_along_normal < 0.0f) {
        const float j = -(1.f + restitution) * relative_velocity_along_normal / denom;
        
        auto torque_impulse_a = gse::vec3<gse::torque>(cross(r_a.as<units::meters>(), collision_normal * j));
        object_motion_component->current_torque += torque_impulse_a;
        object_motion_component->current_velocity += vec3<velocity>(collision_normal * (j * inv_mass_a));
        gse::debug_renderer::add_debug_vector(object_collision_component.parent_id, contact_point, torque_impulse_a);
		std::cout << "applied torque impulse: " << torque_impulse_a.x.as_default_unit() << ", " << torque_impulse_a.y.as_default_unit() << ", " << torque_impulse_a.z.as_default_unit() << "\n";

        if (!other_motion_component->position_locked) {
            auto torque_impulse_b = gse::vec3<gse::torque>(cross(r_b.as<units::meters>(), collision_normal * j));
            other_motion_component->current_torque -= torque_impulse_b;
            other_motion_component->current_velocity -= vec3<velocity>(collision_normal * (j * inv_mass_b));
            gse::debug_renderer::add_debug_vector(other_collision_component.parent_id, contact_point, torque_impulse_b);
        }


    }   

	object_collision_component.collision_information.collision_point = contact_point;

}

auto adjust_orientations_together(gse::physics::motion_component* object_motion_component, gse::physics::collision_component& other_collision_component, float constant) -> void {
    /*take object 1's quaternion orientation and halve the gap between it and object 2's quaternion orientation*/

    const auto& q1 = object_motion_component->orientation;
    auto q2 = other_collision_component.oriented_bounding_box.orientation;

    // dot product
    float dot = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.s * q2.s;

    // If dot < 0, negate q2
    if (dot < 0.0f) {
        q2 = -q2;
        dot = -dot;
    }

    // Clamp dot
    dot = std::clamp(dot, -1.0f, 1.0f);

    // Compute angle
    float theta_0 = std::acos(dot);
    float theta = theta_0 * constant;

    // Orthonormal basis
    gse::quat q3 = q2 - q1 * dot;
    q3 = gse::normalize(q3);

    // Final slerp
    gse::quat result = q1 * std::cos(theta) + q3 * std::sin(theta);

    // Save back
    object_motion_component->orientation = result;

}

auto gse::narrow_phase_collision::resolve_static_collision(physics::motion_component* object_motion_component, physics::collision_component& object_collision_component, physics::collision_component& other_collision_component) -> void {
	sat_result sat_res;
    std::vector<gse::vec3<gse::length>> contact_points;

    

    //print both motion component orientations
	/*std::cout << "Object OBB orientation: "
		<< object_collision_component.oriented_bounding_box.axes[0].x << ", " << object_collision_component.oriented_bounding_box.axes[0].y << ", " << object_collision_component.oriented_bounding_box.axes[0].z << " | "
		<< object_collision_component.oriented_bounding_box.axes[1].x << ", " << object_collision_component.oriented_bounding_box.axes[1].y << ", " << object_collision_component.oriented_bounding_box.axes[1].z << " | "
		<< object_collision_component.oriented_bounding_box.axes[2].x << ", " << object_collision_component.oriented_bounding_box.axes[2].y << ", " << object_collision_component.oriented_bounding_box.axes[2].z << "\n";
	std::cout << "Other OBB orientation: "
		<< other_collision_component.oriented_bounding_box.axes[0].x << ", " << other_collision_component.oriented_bounding_box.axes[0].y << ", " << other_collision_component.oriented_bounding_box.axes[0].z << " | "
		<< other_collision_component.oriented_bounding_box.axes[1].x << ", " << other_collision_component.oriented_bounding_box.axes[1].y << ", " << other_collision_component.oriented_bounding_box.axes[1].z << " | "
		<< other_collision_component.oriented_bounding_box.axes[2].x << ", " << other_collision_component.oriented_bounding_box.axes[2].y << ", " << other_collision_component.oriented_bounding_box.axes[2].z << "\n";*/

    if (!sat_collision(object_collision_component.oriented_bounding_box, other_collision_component.oriented_bounding_box, sat_res)) {
        return;
    }
    if (object_motion_component->position_locked) {
        return;
    }

        //console output "collision detected betweenb [name of object 1] [object 2]"
   /* std::cout << "collision detected" << std::endl;*/

    //get_obb_overlap_vertices(object_collision_component.oriented_bounding_box, other_collision_component.oriented_bounding_box, contact_points);
	std::cout << "Contact points size: " << contact_points.size() << "\n";
    object_collision_component.collision_information.colliding = true;
    object_collision_component.collision_information.collision_normal = sat_res.normal;
    object_collision_component.collision_information.penetration = sat_res.penetration;

    const auto  collision_normal = object_collision_component.collision_information.collision_normal;
    const float penetration_depth = object_collision_component.collision_information.penetration.as_default_unit();
    //if (penetration_depth > 0.1f) {adjust_orientations_together(object_motion_component, other_collision_component, 0.01f);
    //}

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

    const auto main_contact_point = compute_contact_point(clip_polygon_against_plane(
        object_collision_component.oriented_bounding_box.face_vertices(object_collision_component.collision_information.get_axis(), true),
        create_plane(object_motion_component->current_position, object_collision_component.collision_information.collision_normal)
    ));
	contact_points.push_back(main_contact_point);
    /*//int  face_axis;
    //bool edge_edge_sim = false;
    //bool box_a_face = false;
    //auto* ref_obb = &object_collision_component.oriented_bounding_box;

    //// decide which face we hit (or fall back to an edge–edge sim)
    //if (sat_res.axis_index < 3) {
    //    face_axis = sat_res.axis_index;
    //    box_a_face = true;
    //    ref_obb = &object_collision_component.oriented_bounding_box;
    //}
    //else if (sat_res.axis_index < 6) {
    //    face_axis = sat_res.axis_index - 3;
    //    ref_obb = &other_collision_component.oriented_bounding_box;
    //}
    //else {
    //    // pure edge–edge: just pick any axis for fallback
    //    edge_edge_sim = true;
    //    face_axis = static_cast<int>(object_collision_component.collision_information.get_axis());
    //    ref_obb = &object_collision_component.oriented_bounding_box;
    //}

    //// get the geometric center of that face
    //auto face_center = ref_obb->center;
    //auto n = sat_res.normal;
    //float offset_val = gse::dot(n, face_center.as<gse::units::meters>());
    //plane clip_plane{ n, gse::meters(offset_val) };

    //// build the face polygon for clipping
    //bool positive = (n[face_axis] >= 0.f);
    //auto face_verts = object_collision_component.oriented_bounding_box.face_vertices(static_cast<gse::axis>(face_axis), positive);

    //// final contact point
    //gse::vec3<gse::length> contact_point;


    //if (edge_edge_sim) {
    //    // project the face center onto the plane
    //    float d = gse::dot(n, face_center.as<gse::units::meters>())
    //        - clip_plane.offset.as<gse::units::meters>();
    //    contact_point = face_center - n * gse::meters(d);
    //}
    //else {
    //    auto clipped = clip_polygon_against_plane(face_verts, clip_plane);
    //    if (clipped.size() >= 3) {
    //        // use the centroid of the clipped polygon
    //        contact_point = compute_contact_point(clipped);
    //    }
    //    else {
    //        // degenerate: fall back to projecting the face center
    //        float d = gse::dot(n, face_center.as<gse::units::meters>())
    //            - clip_plane.offset.as<gse::units::meters>();
    //        contact_point = face_center - n * gse::meters(d);
    //    }
    //}*/

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
	    std::cout << "contact normal : " << object_collision_component.collision_information.collision_normal.x << ", "
	        << object_collision_component.collision_information.collision_normal.y << ", "
	        << object_collision_component.collision_information.collision_normal.z << "\n";

	    std::cout << "Relative velocity along normal: " << relative_velocity_along_normal << "\n";

	//std::cout << "collision normal: " << collision_normal.x << ", " << collision_normal.y << ", " << collision_normal.z << "\n";
	//	std::cout << "velocity into surface: " << velocity_into_surface << "\n";
	//	std::cout << "acceleration into surface: " << acceleration_into_surface << "\n";
	//	std::cout << "pre-collision velocity: " << object_motion_component->current_velocity.x.as_default_unit() << ", " << object_motion_component->current_velocity.y.as_default_unit() << ", " << object_motion_component->current_velocity.z.as_default_unit() << "\n";
	//	std::cout << "pre-collision acceleration: " << object_motion_component->current_acceleration.x.as_default_unit() << ", " << object_motion_component->current_acceleration.y.as_default_unit() << ", " << object_motion_component->current_acceleration.z.as_default_unit() << "\n";
	//	std::cout << "lever arm: " << r_a.x.as_default_unit() << ", " << r_a.y.as_default_unit() << ", " << r_a.z.as_default_unit() << "\n";
	    if (relative_velocity_along_normal < 0.0f) {
	        const float j = -(1.f + restitution) * relative_velocity_along_normal / denom;
			//std::cout << "rotation term a: " << rot_term_a << "\n";
			//std::cout << "rotation term b: " << rot_term_b << "\n";
			//std::cout << "j: " << j << "\n";
			//std::cout << "denom: " << denom << "\n";

	        auto torque_impulse = gse::vec3<gse::torque>(cross(r_a.as<units::meters>(), collision_normal * j)) * 1500.f;
	        //torque_impulse *= 3.f;
	        //torque_impulse *= 1 / std::min(corrected_penetration, 0.0001f);
	        //constexpr float epsilon_torque = 0.01f;
	        //if (std::abs(torque_impulse.x.as_default_unit()) < epsilon_torque) {
	        //    torque_impulse.x = epsilon_torque * std::copysign(1.0f, torque_impulse.x.as_default_unit());
	        //}
	        object_motion_component->current_torque += torque_impulse;
	        //object_motion_component->current_velocity += vec3<velocity>(collision_normal * (j * inv_mass));
		    gse::debug_renderer::add_debug_vector(object_collision_component.parent_id, contact_point, torque_impulse);
			std::cout << "applied torque impulse: " << torque_impulse.x.as_default_unit() << ", " << torque_impulse.y.as_default_unit() << ", " << torque_impulse.z.as_default_unit() << "\n";

	        //slightly adjust object angular acceleration in the direction of the torque impulse to prevent sticking
			//object_motion_component->angular_acceleration += (torque_impulse / object_motion_component->moment_of_inertia);
			
	         


	    }

	}
    object_collision_component.collision_information.collision_point = main_contact_point;
    gse::debug_renderer::add_debug_point(object_collision_component.parent_id, main_contact_point);

	//std::cout all the same diagnositcs as in the dynamic collision function
	//if (velocity_into_surface < 0.0f) {
 //   vel = 0.0f;
 //   }
 //   if (acceleration_into_surface < 0.0f) {
 //       acc = 0.0f;
 //   }
 //   
    //const auto lever_arm = contact_point - object_motion_component->current_position;
    //const auto torque = cross(lever_arm.as<units::meters>(), object_collision_component.collision_information.collision_normal);
    //object_motion_component->current_torque -= gse::vec3<gse::torque>(torque);
}