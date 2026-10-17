
void vbd_update_joint_lambda(uint3 dispatch_id) {
    if (collision_state[state_converged_flag_index] != 0) return;

    uint ji = dispatch_id.x;
    if (ji >= pc.joint_count) return;

    joint_constraint j = joint_data[ji];
    body_state body_a = body_data[j.body_a];
    body_state body_b = body_data[j.body_b];

    bool a_locked = body_a.locked != 0;
    bool b_locked = body_b.locked != 0;
    bool a_sleeping = body_a.sleep_counter >= sleep_threshold;
    bool b_sleeping = body_b.sleep_counter >= sleep_threshold;
    if ((a_locked || a_sleeping) && (b_locked || b_sleeping)) return;

    bool is_coupled = j.type == joint_type::coupled;
    coupled_frame coupled = {};
    float3 r_aw = rotate_vector(body_a.predicted_orientation, j.local_anchor_a.xyz);
    if (is_coupled) {
        coupled = coupled_frame_of(j.curve_words, body_a.predicted_orientation, body_b.predicted_orientation);
        r_aw += coupled.target_offset;
    }
    float3 r_bw = rotate_vector(body_b.predicted_orientation, j.local_anchor_b.xyz);
    float3 p_a = body_a.predicted_position.xyz + r_aw;
    float3 p_b = body_b.predicted_position.xyz + r_bw;
    float3 d = p_a - p_b;

    if (j.type == joint_type::distance) {
        float c = (length(d) - j.target_distance) - j.pos_c0.x * pc.current_alpha;
        float penalty = j.compliance > 0.0 ? j.pos_penalty.x : muscle_floored_penalty(j.pos_penalty.x, j.body_a, j.body_b);
        j.pos_lambda.x = penalty * c + j.pos_lambda.x;
        float penalty_cap = solver_config_data[0].penalty_max;
        if (j.compliance > 0.0) {
            penalty_cap = min(1.0 / max(j.compliance * pc.h_squared, 1e-10), solver_config_data[0].penalty_max);
        }
        j.pos_penalty.x = min(j.pos_penalty.x + solver_config_data[0].beta * abs(c), penalty_cap);
        InterlockedMax(collision_state[state_convergence_max_delta_index], asuint(abs(c)));
    }
    else if (j.type == joint_type::fixed || j.type == joint_type::hinge || j.type == joint_type::ball || j.type == joint_type::universal || is_coupled) {
        float3 dirs[3] = { float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1) };
        float3 c_rows = float3(0, 0, 0);
        float carried = joint_carried_fraction(j.pos_c0, pc.current_alpha, solver_config_data[0].joint_correction_speed, pc.dt);
        for (int k = 0; k < 3; k++) {
            float c = dot(dirs[k], d) - j.pos_c0[k] * carried;
            c_rows[k] = c;
            j.pos_lambda[k] = muscle_floored_penalty(j.pos_penalty[k], j.body_a, j.body_b) * c + j.pos_lambda[k];
            j.pos_penalty[k] = min(j.pos_penalty[k] + solver_config_data[0].beta * abs(c), solver_config_data[0].penalty_max);
            InterlockedMax(collision_state[state_convergence_max_delta_index], asuint(abs(c)));
        }

        uint trace_body = solver_config_data[0].trace_body;
        if (pc.substep == 0 && pc.iteration < iteration_trace_slots && (j.body_a == trace_body || j.body_b == trace_body)) {
            uint base = state_joint_trace_base_index + pc.iteration * joint_trace_uints;
            for (int k = 0; k < 3; k++) {
                collision_state[base + k * 3] = asuint(c_rows[k]);
                collision_state[base + k * 3 + 1] = asuint(j.pos_lambda[k]);
                collision_state[base + k * 3 + 2] = asuint(j.pos_penalty[k]);
            }
        }

        if (j.type != joint_type::ball || j.limited_rows != 0) {
            float4 q_error = quat_mul(body_b.predicted_orientation, quat_mul(quat_conjugate(body_a.predicted_orientation), quat_conjugate(j.rest_orientation)));
            float3 theta = to_axis_angle(q_error);

            if (j.type == joint_type::fixed) {
                for (int k = 0; k < 3; k++) {
                    float c_ang = theta[k] - j.ang_c0[k] * pc.current_alpha;
                    j.ang_lambda[k] = j.ang_penalty[k] * c_ang + j.ang_lambda[k];
                    j.ang_penalty[k] = min(j.ang_penalty[k] + solver_config_data[0].ang_beta * abs(c_ang), solver_config_data[0].ang_penalty_max);
                    InterlockedMax(collision_state[state_convergence_max_angular_delta_index], asuint(abs(c_ang)));
                }
            }
            else if (j.type == joint_type::ball) {
                for (int k = 0; k < 3; k++) {
                    if ((j.limited_rows & (1u << k)) == 0) continue;
                    float c_limit;
                    if (angular_limit_dual(dot(rotate_vector(body_a.predicted_orientation, dirs[k]), theta), j.limit_lower[k], j.limit_upper[k], j.ang_c0[k] * pc.current_alpha, 1.0, solver_config_data[0].ang_beta, solver_config_data[0].ang_penalty_max, j.ang_lambda[k], j.ang_penalty[k], c_limit)) {
                        InterlockedMax(collision_state[state_convergence_max_angular_delta_index], asuint(abs(c_limit)));
                    }
                }
            }
            else if (j.type == joint_type::universal) {
                float3 axis_a = rotate_vector(body_a.predicted_orientation, j.local_axis_a.xyz);
                float c_twist = dot(axis_a, theta) - j.ang_c0.x * pc.current_alpha;
                j.ang_lambda.x = j.ang_penalty.x * c_twist + j.ang_lambda.x;
                j.ang_penalty.x = min(j.ang_penalty.x + solver_config_data[0].ang_beta * abs(c_twist), solver_config_data[0].ang_penalty_max);
                InterlockedMax(collision_state[state_convergence_max_angular_delta_index], asuint(abs(c_twist)));
            }
            else if (is_coupled) {
                float3 phi = coupled_swing(coupled);
                for (int k = 0; k < int(3 - coupled.count); k++) {
                    float c_ang = dot(coupled.perpendiculars[k], phi) - j.ang_c0[k] * pc.current_alpha;
                    j.ang_lambda[k] = j.ang_penalty[k] * c_ang + j.ang_lambda[k];
                    j.ang_penalty[k] = min(j.ang_penalty[k] + solver_config_data[0].ang_beta * abs(c_ang), solver_config_data[0].ang_penalty_max);
                    InterlockedMax(collision_state[state_convergence_max_angular_delta_index], asuint(abs(c_ang)));
                }

                for (uint k = 0; k < coupled.count; k++) {
                    int slot = coupled_slot(coupled, k);
                    if ((j.limited_rows & (1u << slot)) == 0) continue;
                    float c_limit;
                    if (angular_limit_dual(coupled.coordinates[k], j.limit_lower[slot], j.limit_upper[slot], j.ang_c0[slot] * pc.current_alpha, coupled_limit_scale(coupled.coordinate_axes[k], solver_config_data[0].coupled_limit_normalized), solver_config_data[0].ang_beta, solver_config_data[0].ang_penalty_max, j.ang_lambda[slot], j.ang_penalty[slot], c_limit)) {
                        InterlockedMax(collision_state[state_convergence_max_angular_delta_index], asuint(abs(c_limit)));
                    }
                }
            }
            else {
                float3 axis_a = rotate_vector(body_a.predicted_orientation, j.local_axis_a.xyz);
                float3 axis_b = rotate_vector(body_b.predicted_orientation, j.local_axis_b.xyz);
                float3 swing_error = cross(axis_a, axis_b);
                float3 perp_u = body_perpendicular(body_a.predicted_orientation, j.local_axis_a.xyz, 0);
                float3 perp_v = body_perpendicular(body_a.predicted_orientation, j.local_axis_a.xyz, 1);

                float c_u = dot(perp_u, swing_error) - j.ang_c0.x * pc.current_alpha;
                float c_v = dot(perp_v, swing_error) - j.ang_c0.y * pc.current_alpha;

                j.ang_lambda.x = j.ang_penalty.x * c_u + j.ang_lambda.x;
                j.ang_penalty.x = min(j.ang_penalty.x + solver_config_data[0].ang_beta * abs(c_u), solver_config_data[0].ang_penalty_max);
                InterlockedMax(collision_state[state_convergence_max_angular_delta_index], asuint(abs(c_u)));

                j.ang_lambda.y = j.ang_penalty.y * c_v + j.ang_lambda.y;
                j.ang_penalty.y = min(j.ang_penalty.y + solver_config_data[0].ang_beta * abs(c_v), solver_config_data[0].ang_penalty_max);
                InterlockedMax(collision_state[state_convergence_max_angular_delta_index], asuint(abs(c_v)));

                if (j.limited_rows != 0) {
                    int row = int(hinge_limit_row);
                    float c_limit;
                    if (angular_limit_dual(dot(axis_a, theta), j.limit_lower[row], j.limit_upper[row], j.ang_c0[row] * pc.current_alpha, 1.0, solver_config_data[0].ang_beta, solver_config_data[0].ang_penalty_max, j.ang_lambda[row], j.ang_penalty[row], c_limit)) {
                        InterlockedMax(collision_state[state_convergence_max_angular_delta_index], asuint(abs(c_limit)));
                    }
                }
            }
        }
    }
    else if (j.type == joint_type::slider) {
        float3 perps[2] = { body_perpendicular(body_a.predicted_orientation, j.local_axis_a.xyz, 0), body_perpendicular(body_a.predicted_orientation, j.local_axis_a.xyz, 1) };
        for (int k = 0; k < 2; k++) {
            float c = dot(perps[k], d) - j.pos_c0[k] * pc.current_alpha;
            j.pos_lambda[k] = muscle_floored_penalty(j.pos_penalty[k], j.body_a, j.body_b) * c + j.pos_lambda[k];
            j.pos_penalty[k] = min(j.pos_penalty[k] + solver_config_data[0].beta * abs(c), solver_config_data[0].penalty_max);
            InterlockedMax(collision_state[state_convergence_max_delta_index], asuint(abs(c)));
        }

        float4 q_error = quat_mul(body_b.predicted_orientation, quat_mul(quat_conjugate(body_a.predicted_orientation), quat_conjugate(j.rest_orientation)));
        float3 theta = to_axis_angle(q_error);

        for (int k = 0; k < 3; k++) {
            float c_ang = theta[k] - j.ang_c0[k] * pc.current_alpha;
            j.ang_lambda[k] = j.ang_penalty[k] * c_ang + j.ang_lambda[k];
            j.ang_penalty[k] = min(j.ang_penalty[k] + solver_config_data[0].ang_beta * abs(c_ang), solver_config_data[0].ang_penalty_max);
            InterlockedMax(collision_state[state_convergence_max_angular_delta_index], asuint(abs(c_ang)));
        }
    }

    if (ji == 0) {
        collision_state[state_joint_lambda_index] = asuint(abs(j.pos_lambda[0]));
        collision_state[state_joint_penalty_index] = asuint(abs(j.pos_penalty[0]));
        collision_state[state_joint_c0_index] = asuint(abs(j.pos_c0[0]));
        collision_state[state_joint_c_index] = asuint(length(d));
    }

    joint_data[ji] = j;
}
