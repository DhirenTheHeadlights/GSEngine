float3 start_muscle_point(muscle_path_point p) {
    body_state b = body_data[p.body];
    if (p.frame_words == no_curve_words) {
        return b.position.xyz + rotate_vector(b.orientation, p.local_point.xyz);
    }
    return b.position.xyz + moving_point_of(p.frame_words, b.orientation, body_data[p.frame_body].orientation, p.local_point.xyz).offset;
}

float muscle_body_floor(uint bi) {
    float total = 0.0;
    uint offset = joint_offsets[bi];
    uint count = joint_counts[bi];
    for (uint k = 0; k < count; k++) {
        uint ji = safe_load_uint(joint_adjacency, offset + k, max_joint_adjacency);
        if ((ji & muscle_adjacency_tag) == 0) continue;
        muscle_constraint m = muscle_data[ji & ~muscle_adjacency_tag];
        float isometric = muscle_tension(m, m.path_length_start, m.path_length_start, pc.dt).y;
        if (isometric <= 0.0) continue;
        float3 g = float3(0, 0, 0);
        if (m.wrap_count != 0) {
            start_wrap_poses poses;
            float3 anchor = wrap_anchor(poses, m);
            g = wrapped_linear_gradient(poses, m, anchor, wrap_muscle_path(poses, m, anchor), bi);
        }
        else {
            for (uint p = 0; p < m.path_count; p++) {
                muscle_path_point point = muscle_path_data[m.path_first + p];
                if (point.body != bi) continue;
                float3 here = start_muscle_point(point);
                if (p > 0) {
                    float3 d = here - start_muscle_point(muscle_path_data[m.path_first + p - 1]);
                    if (length(d) > 1e-7) g += normalize(d);
                }
                if (p + 1 < m.path_count) {
                    float3 d = start_muscle_point(muscle_path_data[m.path_first + p + 1]) - here;
                    if (length(d) > 1e-7) g -= normalize(d);
                }
            }
        }
        total += isometric * dot(g, g);
    }
    return min(total, solver_config_data[0].penalty_max);
}

float muscle_floor(uint bi) {
    return solve_deltas[2 * max_bodies + bi].x;
}

float muscle_floored_penalty(float stored, uint body_a, uint body_b) {
    if (solver_config_data[0].muscle_count == 0) return stored;
    return min(max(stored, max(muscle_floor(body_a), muscle_floor(body_b))), solver_config_data[0].penalty_max);
}

float contact_normal_penalty(contact_constraint c) {
    if (c.lambda.x < -1e-3 || c.c0.x < -1e-4) {
        return muscle_floored_penalty(c.penalty.x, c.body_a, c.body_b);
    }
    return c.penalty.x;
}
