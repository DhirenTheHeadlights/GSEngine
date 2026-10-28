uint curve_word(uint index) {
    return hull_data[curve_words_base + index];
}

float curve_float(uint index) {
    return asfloat(curve_word(index));
}

float3 curve_vec3(uint index) {
    return float3(curve_float(index), curve_float(index + 1), curve_float(index + 2));
}

float4 curve_quat(uint index) {
    return float4(curve_float(index), curve_float(index + 1), curve_float(index + 2), curve_float(index + 3));
}

float4 quat_from_axis_angle(float3 axis, float angle) {
    float half_angle = angle * 0.5;
    return float4(cos(half_angle), axis * sin(half_angle));
}

float2 sample_curve(uint base, uint header, float theta) {
    uint count = curve_word(header + 5);
    if (count == 0) return float2(0.0, 0.0);
    uint first = base + curve_word(header + 4);
    uint at = first;
    for (uint i = 1; i < count; i++) {
        uint candidate = first + i * curve_segment_uints;
        if (theta < curve_float(candidate)) break;
        at = candidate;
    }
    float u = theta - curve_float(at);
    float c0 = curve_float(at + 1);
    float c1 = curve_float(at + 2);
    float c2 = curve_float(at + 3);
    float c3 = curve_float(at + 4);
    return float2(c0 + u * (c1 + u * (c2 + u * c3)), c1 + u * (2.0 * c2 + u * 3.0 * c3));
}

float twist_about(float4 q, float3 axis) {
    float s = q.x >= 0.0 ? 1.0 : -1.0;
    return 2.0 * atan2(s * dot(q.yzw, axis), s * q.x);
}

struct spatial_sample {
    float4 rotation;
    float3 translation;
    float3 rotation_rates[3];
    float3 translation_rates[3];
};

spatial_sample sample_spatial(uint base, uint spatial, float3 coordinates) {
    spatial_sample result;
    result.rotation = float4(1, 0, 0, 0);
    result.translation = float3(0, 0, 0);
    for (uint c = 0; c < 3; c++) {
        result.rotation_rates[c] = float3(0, 0, 0);
        result.translation_rates[c] = float3(0, 0, 0);
    }
    [loop]
    for (uint k = 0; k < 6; k++) {
        uint header = spatial + k * curve_axis_uints;
        float3 axis = curve_vec3(header);
        uint coordinate = curve_word(header + 3);
        float2 s = sample_curve(base, header, coordinates[coordinate]);
        if (k < 3) {
            result.rotation_rates[coordinate] += rotate_vector(result.rotation, axis) * s.y;
            result.rotation = quat_mul(result.rotation, quat_from_axis_angle(axis, s.x));
        }
        else {
            result.translation += axis * s.x;
            result.translation_rates[coordinate] += axis * s.y;
        }
    }
    return result;
}

float3 coupled_initial_coordinates(uint spatial, float4 relative, uint count) {
    float3 a0 = curve_vec3(spatial);
    if (count == 1) return float3(twist_about(relative, a0), 0.0, 0.0);
    float3 a1 = curve_vec3(spatial + curve_axis_uints);
    float3 a2 = curve_vec3(spatial + 2 * curve_axis_uints);
    float handed = dot(a0, cross(a1, a2));
    float3 r0 = rotate_vector(relative, a0);
    float3 r1 = rotate_vector(relative, a1);
    float3 r2 = rotate_vector(relative, a2);
    return float3(
        atan2(-handed * dot(a1, r2), dot(a2, r2)),
        asin(clamp(handed * dot(a0, r2), -1.0, 1.0)),
        atan2(-handed * dot(a0, r1), dot(a0, r0)));
}

void coupled_pseudo_inverse(float3 rates[3], out float3 rows[3]) {
    mat3 gram;
    for (uint c = 0; c < 3; c++) {
        for (uint r = 0; r < 3; r++) {
            gram[c][r] = dot(rates[r], rates[c]) + (r == c ? coupled_damping : 0.0);
        }
    }
    mat3 gram_inverse = mat3_inverse(gram);
    for (uint k = 0; k < 3; k++) {
        rows[k] = rates[0] * gram_inverse[k][0] + rates[1] * gram_inverse[k][1] + rates[2] * gram_inverse[k][2];
    }
}

float3 coupled_perpendicular(float3 axis, int row) {
    float3 perp_u = cross(axis, float3(0, 1, 0));
    if (length(perp_u) < 1e-6) perp_u = cross(axis, float3(1, 0, 0));
    perp_u = normalize(perp_u);
    return row == 0 ? perp_u : normalize(cross(axis, perp_u));
}

float3 body_perpendicular(float4 orientation, float3 local_axis, int row) {
    return rotate_vector(orientation, coupled_perpendicular(local_axis, row));
}

struct coupled_frame {
    float4 child;
    float4 target;
    uint count;
    float3 coordinates;
    float3 perpendiculars[2];
    float3 coordinate_axes[3];
    float3 translation_rates[3];
    float3 target_offset;
};

float3 coupled_coordinates_of(uint joint_words, float4 orientation_a, float4 orientation_b) {
    float4 frame = quat_mul(orientation_a, curve_quat(joint_words));
    float4 child = quat_mul(orientation_b, curve_quat(joint_words + 4));
    float4 relative = quat_mul(quat_conjugate(frame), child);
    uint spatial = joint_words + curve_word(joint_words + coupled_spatial_index);
    uint count = curve_word(joint_words + coupled_coordinate_count_index);
    uint iterations = curve_word(joint_words + coupled_newton_iterations_index);

    float3 coordinates = coupled_initial_coordinates(spatial, relative, count);
    float3 rows[3];
    [loop]
    for (uint i = 0; i < iterations; i++) {
        spatial_sample s = sample_spatial(joint_words, spatial, coordinates);
        coupled_pseudo_inverse(s.rotation_rates, rows);
        float3 phi = to_axis_angle(quat_mul(relative, quat_conjugate(s.rotation)));
        coordinates += float3(dot(rows[0], phi), dot(rows[1], phi), dot(rows[2], phi));
    }
    return coordinates;
}

coupled_frame coupled_frame_at(uint joint_words, float4 orientation_a, float4 orientation_b, float3 coordinates) {
    float4 frame = quat_mul(orientation_a, curve_quat(joint_words));
    float4 child = quat_mul(orientation_b, curve_quat(joint_words + 4));
    uint spatial = joint_words + curve_word(joint_words + coupled_spatial_index);
    uint count = curve_word(joint_words + coupled_coordinate_count_index);

    float3 rows[3];
    spatial_sample s = sample_spatial(joint_words, spatial, coordinates);
    coupled_pseudo_inverse(s.rotation_rates, rows);

    coupled_frame result;
    result.child = child;
    result.target = quat_mul(frame, s.rotation);
    result.count = count;
    result.coordinates = coordinates;
    float3 local_axis = normalize(s.rotation_rates[0]);
    result.perpendiculars[0] = body_perpendicular(frame, local_axis, 0);
    result.perpendiculars[1] = body_perpendicular(frame, local_axis, 1);
    for (uint k = 0; k < 3; k++) {
        result.coordinate_axes[k] = rotate_vector(frame, rows[k]);
        result.translation_rates[k] = rotate_vector(frame, s.translation_rates[k]);
    }
    result.target_offset = rotate_vector(frame, s.translation);
    return result;
}

coupled_frame coupled_frame_of(uint joint_words, float4 orientation_a, float4 orientation_b) {
    return coupled_frame_at(joint_words, orientation_a, orientation_b, coupled_coordinates_of(joint_words, orientation_a, orientation_b));
}

int coupled_slot(coupled_frame frame, uint coordinate) {
    return int(3 - frame.count + coordinate);
}

float3 coupled_translation_lever(coupled_frame frame, float3 direction) {
    float3 lever = float3(0, 0, 0);
    for (uint k = 0; k < 3; k++) {
        lever += frame.coordinate_axes[k] * dot(direction, frame.translation_rates[k]);
    }
    return lever;
}

float3 coupled_swing(coupled_frame frame) {
    return to_axis_angle(quat_mul(frame.child, quat_conjugate(frame.target)));
}

struct moving_point {
    float3 offset;
    float3 coordinate_axes[3];
    float3 rates[3];
};

struct moving_frame {
    float3 anchor;
    float4 placed;
    spatial_sample sample;
    float3 coordinate_axes[3];
};

uint moving_frame_joint_words(uint frame_words) {
    return frame_words - curve_word(frame_words);
}

moving_frame moving_frame_in(uint frame_words, float4 orientation_a, coupled_frame joint) {
    uint joint_words = moving_frame_joint_words(frame_words);
    moving_frame result;
    result.anchor = rotate_vector(orientation_a, curve_vec3(frame_words + 1));
    result.placed = quat_mul(orientation_a, curve_quat(frame_words + 4));
    result.sample = sample_spatial(joint_words, frame_words + curve_frame_header_uints, joint.coordinates);
    result.coordinate_axes = joint.coordinate_axes;
    return result;
}

moving_frame moving_frame_of(uint frame_words, float4 orientation_a, float4 orientation_b) {
    return moving_frame_in(frame_words, orientation_a, coupled_frame_of(moving_frame_joint_words(frame_words), orientation_a, orientation_b));
}

moving_point moving_point_in(moving_frame frame, float3 local_point) {
    float3 rotated = rotate_vector(frame.sample.rotation, local_point);
    moving_point result;
    result.offset = frame.anchor + rotate_vector(frame.placed, rotated + frame.sample.translation);
    for (uint k = 0; k < 3; k++) {
        result.coordinate_axes[k] = frame.coordinate_axes[k];
        result.rates[k] = rotate_vector(frame.placed, cross(frame.sample.rotation_rates[k], rotated) + frame.sample.translation_rates[k]);
    }
    return result;
}

moving_point moving_point_of(uint frame_words, float4 orientation_a, float4 orientation_b, float3 local_point) {
    return moving_point_in(moving_frame_of(frame_words, orientation_a, orientation_b), local_point);
}

float3 moving_point_lever(moving_point point, float3 direction) {
    float3 lever = float3(0, 0, 0);
    for (uint k = 0; k < 3; k++) {
        lever += point.coordinate_axes[k] * dot(direction, point.rates[k]);
    }
    return lever;
}
