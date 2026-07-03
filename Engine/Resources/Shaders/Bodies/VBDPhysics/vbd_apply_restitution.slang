
void vbd_apply_restitution(uint3 dispatch_id) {
    uint idx = dispatch_id.x;
    uint num_colors = min(collision_state[state_max_used_color_index], pc.color_count);
    uint color = pc.color_offset;
    if (color >= num_colors) return;

    uint count = color_data[max_colors + color];
    if (idx >= count) return;

    uint offset = color_data[color];
    uint bi = color_data[max_colors * 2 + offset + idx];
    if (bi >= pc.body_count) return;

    body_state body = body_data[bi];
    if (body.locked != 0) return;
    if (body.sleep_counter >= sleep_threshold) return;
    if (body.mass <= 1e-10) return;

    float w_self = 1.0 / body.mass;
    float3 v_delta = float3(0, 0, 0);

    uint co = contact_offsets[bi];
    uint cc = contact_counts[bi];
    for (uint k = 0; k < cc; k++) {
        uint ci = safe_load_uint(contact_adjacency, co + k, max_contact_adjacency);
        if (ci >= max_contacts) continue;

        contact_constraint c = contact_data[ci];
        if (c.restitution <= 0.0) continue;
        if (c.lambda.x >= 0.0) continue;
        if (c.approach_speed >= -solver_config_data[0].restitution_threshold) continue;

        bool is_a = (c.body_a == bi);
        uint other_idx = is_a ? c.body_b : c.body_a;
        if (other_idx >= pc.body_count) continue;
        body_state other = body_data[other_idx];

        float3 r_self = rotate_vector(body.orientation, is_a ? c.local_anchor_a : c.local_anchor_b);
        float3 r_other = rotate_vector(other.orientation, is_a ? c.local_anchor_b : c.local_anchor_a);
        float3 v_self = body.velocity + cross(body.angular_velocity, r_self);
        float3 v_other = other.velocity + cross(other.angular_velocity, r_other);
        float3 v_rel_vec = is_a ? (v_self - v_other) : (v_other - v_self);
        float v_rel_after = dot(c.normal, v_rel_vec);

        float speed_ratio = clamp(
            (-c.approach_speed - solver_config_data[0].restitution_threshold) / solver_config_data[0].restitution_threshold,
            0.0,
            1.0
        );
        float v_target = -c.restitution * speed_ratio * c.approach_speed;
        float dv = v_target - v_rel_after;
        if (dv <= 0.0) continue;

        float w_other = (other.locked != 0) ? 0.0 : 1.0 / other.mass;
        float w_total = w_self + w_other;
        if (w_total <= 0.0) continue;

        float impulse_mag = dv / w_total;
        float sign = is_a ? 1.0 : -1.0;
        v_delta += c.normal * (impulse_mag * w_self * sign);
    }

    if (any(v_delta != float3(0, 0, 0))) {
        body_data[bi].velocity = body.velocity + v_delta;
    }
}
