float loco_hinge_angle_about(float4 parent, float4 child, float3 axis_local) {
    float3 theta = to_axis_angle(quat_mul(child, quat_conjugate(parent)));
    float3 axis_world = rotate_vector(parent, axis_local);
    return dot(axis_world, theta);
}

float loco_hinge_angle_about_x(float4 parent, float4 child) {
    return loco_hinge_angle_about(parent, child, float3(1, 0, 0));
}

float loco_excess_past_edge(float value, float lo, float hi) {
    if (value < lo) return value - lo;
    if (value > hi) return value - hi;
    return 0.0;
}

float3 loco_origin_of(uint bi) {
    body_state b = loco_body_data[bi];
    return body_origin(b.position.xyz, b.orientation, b.com_local.xyz);
}

void loco_observe(uint3 dispatch_id) {
    uint e = dispatch_id.x;
    if (e >= pc.env_count) return;

    loco_env_desc env = loco_env_data[e];
    uint base = pc.obs_offset + e * pc.obs_stride;
    uint rbase = pc.env_count * pc.obs_stride + e * loco_reward_stride;

    if (env.valid == 0) {
        for (uint k = 0; k < pc.obs_dim; ++k) {
            loco_obs_data[base + k] = 0.0;
        }
        loco_prev_data[e * loco_prev_stride + loco_prev_valid] = 0.0;
        if (pc.write_reward != 0) {
            for (uint k = 0; k < loco_reward_stride; ++k) {
                loco_obs_data[rbase + k] = 0.0;
            }
        }
        return;
    }

    body_state pelvis = loco_body_data[env.pelvis];
    float3 pelvis_pos = body_origin(pelvis.position.xyz, pelvis.orientation, pelvis.com_local.xyz);
    float4 pelvis_q = pelvis.orientation;
    float3 pelvis_forward = rotate_vector(pelvis_q, float3(0, 0, -1));
    float3 pelvis_right = rotate_vector(pelvis_q, float3(1, 0, 0));

    body_state foot_l = loco_body_data[env.foot_l];
    body_state foot_r = loco_body_data[env.foot_r];
    float3 l_min = foot_l.aabb_min.xyz;
    float3 l_max = foot_l.aabb_max.xyz;
    float3 r_min = foot_r.aabb_min.xyz;
    float3 r_max = foot_r.aabb_max.xyz;

    if (env.has_toe_l != 0) {
        body_state toe = loco_body_data[env.toe_l];
        l_min = min(l_min, toe.aabb_min.xyz);
        l_max = max(l_max, toe.aabb_max.xyz);
    }
    if (env.has_toe_r != 0) {
        body_state toe = loco_body_data[env.toe_r];
        r_min = min(r_min, toe.aabb_min.xyz);
        r_max = max(r_max, toe.aabb_max.xyz);
    }

    float contact_y = pc.ground_y + pc.ground_tolerance;
    bool grounded_l = l_min.y <= contact_y;
    bool grounded_r = r_min.y <= contact_y;
    bool any_grounded = grounded_l || grounded_r;
    bool double_support = grounded_l && grounded_r;

    bool swing_support = env.gait_present != 0 && env.gait_swing != 0;

    float3 support_min;
    float3 support_max;
    if (swing_support && env.gait_swing_right != 0) {
        support_min = l_min;
        support_max = l_max;
    }
    else if (swing_support && env.gait_swing_right == 0) {
        support_min = r_min;
        support_max = r_max;
    }
    else if (double_support || !any_grounded) {
        support_min = min(l_min, r_min);
        support_max = max(l_max, r_max);
    }
    else if (grounded_l) {
        support_min = l_min;
        support_max = l_max;
    }
    else {
        support_min = r_min;
        support_max = r_max;
    }

    float3 support_center = lerp(support_min, support_max, 0.5);

    float total_mass = env.pelvis_mass + env.upper_body_mass + (env.thigh_mass + env.shin_mass + env.foot_mass) * 2.0;
    float3 torso_pos = loco_origin_of(env.torso);
    float3 thigh_l_pos = loco_origin_of(env.thigh_l);
    float3 shin_l_pos = loco_origin_of(env.shin_l);
    float3 thigh_r_pos = loco_origin_of(env.thigh_r);
    float3 shin_r_pos = loco_origin_of(env.shin_r);
    float3 foot_l_pos = body_origin(foot_l.position.xyz, foot_l.orientation, foot_l.com_local.xyz);
    float3 foot_r_pos = body_origin(foot_r.position.xyz, foot_r.orientation, foot_r.com_local.xyz);

    float3 com_offset = float3(0, 0, 0);
    com_offset += (torso_pos - pelvis_pos) * (env.upper_body_mass / total_mass);
    com_offset += (thigh_l_pos - pelvis_pos) * (env.thigh_mass / total_mass);
    com_offset += (shin_l_pos - pelvis_pos) * (env.shin_mass / total_mass);
    com_offset += (foot_l_pos - pelvis_pos) * (env.foot_mass / total_mass);
    com_offset += (thigh_r_pos - pelvis_pos) * (env.thigh_mass / total_mass);
    com_offset += (shin_r_pos - pelvis_pos) * (env.shin_mass / total_mass);
    com_offset += (foot_r_pos - pelvis_pos) * (env.foot_mass / total_mass);
    float3 com_world = pelvis_pos + com_offset;

    float com_height = max(com_world.y - pc.ground_y, 0.30);
    float pendulum_time = sqrt(com_height / 9.81);

    float3 lean_world = float3(
        loco_excess_past_edge(com_world.x, support_min.x, support_max.x),
        0.0,
        loco_excess_past_edge(com_world.z, support_min.z, support_max.z)
    );

    float3 velocity_world = float3(pelvis.velocity.x, 0.0, pelvis.velocity.z);

    float4 inverse_pelvis = quat_conjugate(pelvis_q);
    float3 lean_body = rotate_vector(inverse_pelvis, lean_world);
    float3 velocity_body = rotate_vector(inverse_pelvis, velocity_world);
    float3 capture_offset_world = lean_world + velocity_world * pendulum_time;
    float3 capture_offset_body = rotate_vector(inverse_pelvis, capture_offset_world);
    float capture_right = capture_offset_body.x;
    float capture_forward = -capture_offset_body.z;

    float4 thigh_l_q = loco_body_data[env.thigh_l].orientation;
    float4 shin_l_q = loco_body_data[env.shin_l].orientation;
    float4 thigh_r_q = loco_body_data[env.thigh_r].orientation;
    float4 shin_r_q = loco_body_data[env.shin_r].orientation;

    float hip_angle_l = loco_hinge_angle_about_x(pelvis_q, thigh_l_q);
    float knee_angle_l = loco_hinge_angle_about_x(thigh_l_q, shin_l_q);
    float ankle_angle_l = loco_hinge_angle_about_x(shin_l_q, foot_l.orientation);
    float hip_angle_r = loco_hinge_angle_about_x(pelvis_q, thigh_r_q);
    float knee_angle_r = loco_hinge_angle_about_x(thigh_r_q, shin_r_q);
    float ankle_angle_r = loco_hinge_angle_about_x(shin_r_q, foot_r.orientation);

    uint pbase = e * loco_prev_stride;
    bool prev_valid = loco_prev_data[pbase + loco_prev_valid] != 0.0;
    float inv_dt = pc.dt > 0.0 ? 1.0 / pc.dt : 0.0;

    float hip_rate_l = 0.0;
    float knee_rate_l = 0.0;
    float ankle_rate_l = 0.0;
    float hip_rate_r = 0.0;
    float knee_rate_r = 0.0;
    float ankle_rate_r = 0.0;

    if (prev_valid && pc.dt > 0.0) {
        hip_rate_l = (hip_angle_l - loco_prev_data[pbase + 0]) * inv_dt;
        knee_rate_l = (knee_angle_l - loco_prev_data[pbase + 1]) * inv_dt;
        ankle_rate_l = (ankle_angle_l - loco_prev_data[pbase + 2]) * inv_dt;
        hip_rate_r = (hip_angle_r - loco_prev_data[pbase + 3]) * inv_dt;
        knee_rate_r = (knee_angle_r - loco_prev_data[pbase + 4]) * inv_dt;
        ankle_rate_r = (ankle_angle_r - loco_prev_data[pbase + 5]) * inv_dt;
    }

    if (pc.write_prev != 0) {
        loco_prev_data[pbase + 0] = hip_angle_l;
        loco_prev_data[pbase + 1] = knee_angle_l;
        loco_prev_data[pbase + 2] = ankle_angle_l;
        loco_prev_data[pbase + 3] = hip_angle_r;
        loco_prev_data[pbase + 4] = knee_angle_r;
        loco_prev_data[pbase + 5] = ankle_angle_r;
        loco_prev_data[pbase + loco_prev_valid] = 1.0;
    }

    float pelvis_pitch = asin(clamp(pelvis_forward.y, -1.0, 1.0));
    float pelvis_pitch_rate = dot(pelvis_right, pelvis.angular_velocity.xyz);
    float pelvis_roll = -asin(clamp(pelvis_right.y, -1.0, 1.0));
    float pelvis_roll_rate = dot(pelvis_forward, pelvis.angular_velocity.xyz);

    float current_yaw = atan2(-pelvis_forward.x, -pelvis_forward.z);
    float yaw_diff = env.desired_yaw - current_yaw;
    float two_pi = 6.28318530717958647692;
    float yaw_error = yaw_diff - two_pi * round(yaw_diff / two_pi);
    float heading_error = env.has_heading != 0 ? yaw_error : 0.0;

    loco_obs_data[base + 0] = pelvis_pos.y - support_center.y;
    loco_obs_data[base + 1] = pelvis_pitch;
    loco_obs_data[base + 2] = pelvis_pitch_rate;
    loco_obs_data[base + 3] = velocity_body.x;
    loco_obs_data[base + 4] = velocity_body.y;
    loco_obs_data[base + 5] = velocity_body.z;
    loco_obs_data[base + 6] = hip_angle_l;
    loco_obs_data[base + 7] = knee_angle_l;
    loco_obs_data[base + 8] = ankle_angle_l;
    loco_obs_data[base + 9] = hip_angle_r;
    loco_obs_data[base + 10] = knee_angle_r;
    loco_obs_data[base + 11] = ankle_angle_r;
    loco_obs_data[base + 12] = hip_rate_l;
    loco_obs_data[base + 13] = knee_rate_l;
    loco_obs_data[base + 14] = ankle_rate_l;
    loco_obs_data[base + 15] = hip_rate_r;
    loco_obs_data[base + 16] = knee_rate_r;
    loco_obs_data[base + 17] = ankle_rate_r;
    loco_obs_data[base + 18] = grounded_l ? 1.0 : 0.0;
    loco_obs_data[base + 19] = grounded_r ? 1.0 : 0.0;
    loco_obs_data[base + 20] = capture_forward;
    loco_obs_data[base + 21] = capture_right;
    loco_obs_data[base + 22] = lean_body.x;
    loco_obs_data[base + 23] = lean_body.z;
    loco_obs_data[base + 24] = env.cmd_forward;
    loco_obs_data[base + 25] = env.cmd_strafe;
    loco_obs_data[base + 26] = env.cmd_sprint;
    loco_obs_data[base + 27] = heading_error;
    float phase = env.swap_horizon > 0.0 ? 0.5 * min((prev_valid ? loco_prev_data[pbase + 15] : 0.0) / env.swap_horizon, 1.0) : env.phase;
    loco_obs_data[base + 28] = sin(phase * 6.28318530717958647692);
    loco_obs_data[base + 29] = cos(phase * 6.28318530717958647692);
    loco_obs_data[base + 30] = pelvis_roll;
    loco_obs_data[base + 31] = pelvis_roll_rate;
    loco_obs_data[base + 32] = loco_hinge_angle_about(pelvis_q, thigh_l_q, float3(0, 1, 0));
    loco_obs_data[base + 33] = loco_hinge_angle_about(pelvis_q, thigh_l_q, float3(0, 0, 1));
    loco_obs_data[base + 34] = loco_hinge_angle_about(pelvis_q, thigh_r_q, float3(0, 1, 0));
    loco_obs_data[base + 35] = loco_hinge_angle_about(pelvis_q, thigh_r_q, float3(0, 0, 1));

    for (uint a = 0; a < pc.act_dim; ++a) {
        loco_obs_data[base + pc.obs_dim + a] = loco_act_data[e * pc.act_dim + a];
    }

    if (pc.write_reward == 0) return;

    float sin_yaw = sin(env.desired_yaw);
    float cos_yaw = cos(env.desired_yaw);
    float forward_speed = loco_track_heading_frame != 0 ? -velocity_world.x * sin_yaw - velocity_world.z * cos_yaw : -velocity_body.z;
    float right_speed = loco_track_heading_frame != 0 ? velocity_world.x * cos_yaw - velocity_world.z * sin_yaw : velocity_body.x;
    bool stand_command = env.cmd_forward == 0.0 && env.cmd_strafe == 0.0;
    float tolerance = stand_command && loco_stand_tolerance > 0.0 ? loco_stand_tolerance : pc.track_tolerance;
    float err_forward = (forward_speed - env.cmd_forward) / tolerance;
    float err_right = (right_speed - env.cmd_strafe) / tolerance;
    float track = exp(-(err_forward * err_forward + err_right * err_right));

    float upright = clamp(cross(pelvis_right, pelvis_forward).y, 0.0, 1.0);

    float pelvis_height = pelvis_pos.y - support_center.y;
    float target_above_support = env.target_height - support_center.y;
    float height = clamp(pelvis_height / target_above_support, 0.0, 1.0);

    float heading_ratio = yaw_error / pc.heading_limit;
    float heading = exp(-heading_ratio * heading_ratio);

    float stability = upright * height;

    float energy = 0.0;
    float hold = 0.0;
    float smooth = 0.0;
    for (uint k = 0; k < loco_target_dim; ++k) {
        float a = loco_act_data[e * pc.act_count + k];
        float da = a - loco_prev_data[pbase + loco_prev_action + k];
        energy += a * a;
        smooth += da * da;
    }
    float inv_act = 1.0 / float(loco_target_dim);

    float relative[10] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
    float cocontract = 0.0;
    if (loco_variable_stiffness != 0) {
        for (uint k = 0; k < 10; ++k) {
            relative[k] = loco_relative_stiffness(loco_act_data[e * pc.act_count + loco_target_dim + k]);
            cocontract += loco_stiffness_base * relative[k];
        }
        cocontract *= loco_r_cocontract / 10.0;
    }

    float hip_yaw_rate_l = 0.0;
    float hip_roll_rate_l = 0.0;
    float hip_yaw_rate_r = 0.0;
    float hip_roll_rate_r = 0.0;
    if (prev_valid && pc.dt > 0.0) {
        hip_yaw_rate_l = (loco_obs_data[base + 32] - loco_prev_data[pbase + 8]) * inv_dt;
        hip_roll_rate_l = (loco_obs_data[base + 33] - loco_prev_data[pbase + 9]) * inv_dt;
        hip_yaw_rate_r = (loco_obs_data[base + 34] - loco_prev_data[pbase + 10]) * inv_dt;
        hip_roll_rate_r = (loco_obs_data[base + 35] - loco_prev_data[pbase + 11]) * inv_dt;
    }

    if (pc.energy_effort != 0 || pc.energy_power != 0) {
        float angles[10] = {
            loco_obs_data[base + 6], loco_obs_data[base + 32], loco_obs_data[base + 33], loco_obs_data[base + 7], loco_obs_data[base + 8],
            loco_obs_data[base + 9], loco_obs_data[base + 34], loco_obs_data[base + 35], loco_obs_data[base + 10], loco_obs_data[base + 11],
        };
        float rates[10] = {
            hip_rate_l, hip_yaw_rate_l, hip_roll_rate_l, knee_rate_l, ankle_rate_l,
            hip_rate_r, hip_yaw_rate_r, hip_roll_rate_r, knee_rate_r, ankle_rate_r,
        };
        float saturation[10] = {
            pc.sat_hip_pitch, pc.sat_hip_yaw, pc.sat_hip_roll, pc.sat_knee, pc.sat_ankle,
            pc.sat_hip_pitch, pc.sat_hip_yaw, pc.sat_hip_roll, pc.sat_knee, pc.sat_ankle,
        };
        float max_torque[10] = {
            pc.max_hip, pc.max_hip, pc.max_hip, pc.max_knee, pc.max_ankle,
            pc.max_hip, pc.max_hip, pc.max_hip, pc.max_knee, pc.max_ankle,
        };
        energy = 0.0;
        for (uint k = 0; k < 10; ++k) {
            float load = (angles[k] - loco_act_data[e * pc.act_count + k]) / saturation[k];
            if (loco_variable_stiffness != 0) {
                load *= relative[k];
            }
            if (pc.energy_power != 0) {
                float clamped = clamp(load, -1.0, 1.0);
                float work = load * rates[k] < 0.0 ? pc.work_positive : pc.work_negative;
                energy += max_torque[k] * abs(clamped) * abs(rates[k]) / pc.power_reference * work;
                hold += clamped * clamped;
                if (k == 2 || k == 7) {
                    float passive = max((abs(angles[k]) - pc.hip_roll_limit) / pc.hip_roll_limit, 0.0);
                    hold += passive * passive;
                }
                if ((k == 1 || k == 6) && env.hip_yaw_limit > 0.0) {
                    float passive = max((abs(angles[k]) - env.hip_yaw_limit) / env.hip_yaw_limit, 0.0);
                    hold += passive * passive;
                }
            }
            else {
                energy += load * load;
            }
        }
        float grounded_count = (grounded_l ? 1.0 : 0.0) + (grounded_r ? 1.0 : 0.0);
        if (pc.energy_power != 0 && pc.slip_power != 0 && grounded_count > 0.0) {
            float slip_speed = 0.0;
            if (grounded_l) {
                slip_speed += length(float2(foot_l.velocity.x, foot_l.velocity.z));
            }
            if (grounded_r) {
                slip_speed += length(float2(foot_r.velocity.x, foot_r.velocity.z));
            }
            float normal = total_mass * pc.gravity / grounded_count;
            energy += normal * pc.ground_friction * slip_speed / pc.power_reference;
        }
    }

    float slip = 0.0;
    if (grounded_l) {
        float2 v = float2(foot_l.velocity.x, foot_l.velocity.z) / pc.slip_reference;
        slip += dot(v, v);
    }
    if (grounded_r) {
        float2 v = float2(foot_r.velocity.x, foot_r.velocity.z) / pc.slip_reference;
        slip += dot(v, v);
    }

    float touchdowns = 0.0;
    float mode_l = prev_valid ? loco_prev_data[pbase + 12] : 0.0;
    float mode_r = prev_valid ? loco_prev_data[pbase + 13] : 0.0;
    float prev_bits = prev_valid ? loco_prev_data[pbase + 7] : 0.0;
    bool planted_l = grounded_l;
    bool planted_r = grounded_r;
    bool contact_l = mode_l > 0.0;
    bool contact_r = mode_r > 0.0;
    if (loco_swing_clearance > 0.0) {
        planted_l = grounded_l || (l_min.y - pc.ground_y <= loco_swing_clearance && mode_l >= 0.0);
        planted_r = grounded_r || (r_min.y - pc.ground_y <= loco_swing_clearance && mode_r >= 0.0);
        contact_l = fmod(prev_bits, 16.0) >= 8.0;
        contact_r = prev_bits >= 16.0;
    }
    bool lifted_l = loco_swing_clearance > 0.0 ? mode_l < 0.0 : mode_l <= 0.0;
    bool lifted_r = loco_swing_clearance > 0.0 ? mode_r < 0.0 : mode_r <= 0.0;
    if (prev_valid) {
        touchdowns += (planted_l && lifted_l) ? 1.0 : 0.0;
        touchdowns += (planted_r && lifted_r) ? 1.0 : 0.0;
    }
    float scuff = 0.0;
    float2 now_heel_l = (foot_l_pos + rotate_vector(foot_l.orientation, float3(0, -loco_scuff_drop, loco_scuff_reach))).xz;
    float2 now_toe_l = (foot_l_pos + rotate_vector(foot_l.orientation, float3(0, -loco_scuff_drop, -loco_scuff_reach))).xz;
    float2 now_heel_r = (foot_r_pos + rotate_vector(foot_r.orientation, float3(0, -loco_scuff_drop, loco_scuff_reach))).xz;
    float2 now_toe_r = (foot_r_pos + rotate_vector(foot_r.orientation, float3(0, -loco_scuff_drop, -loco_scuff_reach))).xz;
    float2 heel_l = now_heel_l;
    float2 toe_l = now_toe_l;
    float2 heel_r = now_heel_r;
    float2 toe_r = now_toe_r;
    if (loco_r_scuff != 0.0) {
        if (grounded_l && contact_l) {
            heel_l = float2(loco_prev_data[pbase + 16], loco_prev_data[pbase + 17]);
            toe_l = float2(loco_prev_data[pbase + 18], loco_prev_data[pbase + 19]);
            float drift = min(length(now_heel_l - heel_l), length(now_toe_l - toe_l));
            float over = clamp((drift - loco_scuff_free) / loco_scuff_free, 0.0, 1.0);
            scuff += over * over;
        }
        if (grounded_r && contact_r) {
            heel_r = float2(loco_prev_data[pbase + 20], loco_prev_data[pbase + 21]);
            toe_r = float2(loco_prev_data[pbase + 22], loco_prev_data[pbase + 23]);
            float drift = min(length(now_heel_r - heel_r), length(now_toe_r - toe_r));
            float over = clamp((drift - loco_scuff_free) / loco_scuff_free, 0.0, 1.0);
            scuff += over * over;
        }
    }
    float stand_lift = stand_command ? (grounded_l ? 0.0 : 1.0) + (grounded_r ? 0.0 : 1.0) : 0.0;
    float stride = 0.0;
    float3 feet = foot_r_pos - foot_l_pos;
    float order = feet.x * pelvis_forward.x + feet.z * pelvis_forward.z;
    float behind_bits = fmod(prev_bits, 4.0);
    if (env.r_stride != 0.0) {
        bool behind_l = fmod(behind_bits, 2.0) >= 1.0;
        bool behind_r = behind_bits >= 2.0;
        float landing = clamp(order / loco_stride_reference, -1.0, 1.0);
        if (planted_l && mode_l < 0.0 && behind_l) {
            stride -= landing;
        }
        if (planted_r && mode_r < 0.0 && behind_r) {
            stride += landing;
        }
        if (!planted_l && mode_l >= 0.0) {
            behind_l = order > 0.0;
        }
        if (!planted_r && mode_r >= 0.0) {
            behind_r = order < 0.0;
        }
        behind_bits = (behind_l ? 1.0 : 0.0) + (behind_r ? 2.0 : 0.0);
    }
    mode_l = planted_l ? (mode_l > 0.0 ? mode_l + pc.dt : pc.dt) : (mode_l < 0.0 ? mode_l - pc.dt : -pc.dt);
    mode_r = planted_r ? (mode_r > 0.0 ? mode_r + pc.dt : pc.dt) : (mode_r < 0.0 ? mode_r - pc.dt : -pc.dt);
    float air = (planted_l != planted_r) ? min(min(abs(mode_l), abs(mode_r)), env.air_threshold) / env.air_threshold : 0.0;
    float lead = prev_valid ? loco_prev_data[pbase + 14] : 0.0;
    float since_swap = prev_valid ? loco_prev_data[pbase + 15] : 0.0;
    float lead_now = abs(order) >= env.swap_margin ? sign(order) : 0.0;
    bool swung = fmod(prev_bits, 8.0) >= 4.0 || !planted_l || !planted_r;
    if (lead_now != 0.0 && lead_now != lead && (loco_swing_clearance <= 0.0 || swung)) {
        lead = lead_now;
        since_swap = 0.0;
        swung = false;
    }
    else {
        since_swap += pc.dt;
    }
    bool stale = env.swap_timeout > 0.0 && since_swap > env.swap_timeout;

    float knee_cost = 0.0;
    if (pc.r_stance_knee != 0.0) {
        if (grounded_l) {
            float over = max((abs(loco_obs_data[base + 7]) - pc.stance_knee_free) / pc.stance_knee_free, 0.0);
            knee_cost += over * over;
        }
        if (grounded_r) {
            float over = max((abs(loco_obs_data[base + 10]) - pc.stance_knee_free) / pc.stance_knee_free, 0.0);
            knee_cost += over * over;
        }
    }

    float width_cost = 0.0;
    if (pc.r_width != 0.0) {
        float3 feet = foot_r_pos - foot_l_pos;
        float over = max((abs(feet.x * pelvis_right.x + feet.z * pelvis_right.z) - pc.stance_width_free) / pc.stance_width_free, 0.0);
        width_cost = over * over;
    }

    float pose = 0.0;
    if (env.has_pose != 0) {
        float d_hip_l = loco_obs_data[base + 6] - env.ref_hip_l;
        float d_knee_l = loco_obs_data[base + 7] - env.ref_knee_l;
        float d_ankle_l = loco_obs_data[base + 8] - env.ref_ankle_l;
        float d_hip_r = loco_obs_data[base + 9] - env.ref_hip_r;
        float d_knee_r = loco_obs_data[base + 10] - env.ref_knee_r;
        float d_ankle_r = loco_obs_data[base + 11] - env.ref_ankle_r;
        float sq = d_hip_l * d_hip_l + d_knee_l * d_knee_l + d_ankle_l * d_ankle_l + d_hip_r * d_hip_r + d_knee_r * d_knee_r + d_ankle_r * d_ankle_r;
        pose = exp(-sq / (6.0 * pc.pose_tolerance * pc.pose_tolerance));
    }

    if (pc.write_prev != 0) {
        for (uint k = 0; k < pc.act_count; ++k) {
            loco_prev_data[pbase + loco_prev_action + k] = loco_act_data[e * pc.act_count + k];
        }
        loco_prev_data[pbase + 8] = loco_obs_data[base + 32];
        loco_prev_data[pbase + 9] = loco_obs_data[base + 33];
        loco_prev_data[pbase + 10] = loco_obs_data[base + 34];
        loco_prev_data[pbase + 11] = loco_obs_data[base + 35];
        loco_prev_data[pbase + 12] = mode_l;
        loco_prev_data[pbase + 13] = mode_r;
        if (loco_swing_clearance > 0.0) {
            loco_prev_data[pbase + 7] = behind_bits + (swung ? 4.0 : 0.0) + (grounded_l ? 8.0 : 0.0) + (grounded_r ? 16.0 : 0.0);
        }
        else if (env.r_stride != 0.0) {
            loco_prev_data[pbase + 7] = behind_bits;
        }
        loco_prev_data[pbase + 14] = lead;
        loco_prev_data[pbase + 15] = since_swap;
        if (loco_r_scuff != 0.0) {
            loco_prev_data[pbase + 16] = heel_l.x;
            loco_prev_data[pbase + 17] = heel_l.y;
            loco_prev_data[pbase + 18] = toe_l.x;
            loco_prev_data[pbase + 19] = toe_l.y;
            loco_prev_data[pbase + 20] = heel_r.x;
            loco_prev_data[pbase + 21] = heel_r.y;
            loco_prev_data[pbase + 22] = toe_r.x;
            loco_prev_data[pbase + 23] = toe_r.y;
        }
    }

    bool fell = pelvis_pos.y < pc.fall_height || upright < pc.fall_upright;
    bool fallen = fell || stale;
    bool timeout = pc.max_steps != 0 && env.episode_steps >= pc.max_steps;

    loco_obs_data[rbase + 0] = pc.r_alive - (fell ? pc.r_fall : stale ? env.stale_penalty : 0.0);
    loco_obs_data[rbase + 1] = pc.r_upright * upright;
    loco_obs_data[rbase + 2] = pc.r_height * height;
    float progress_speed = forward_speed;
    float progress_command = env.cmd_forward;
    if (loco_progress_along != 0) {
        float cmd_speed = sqrt(env.cmd_forward * env.cmd_forward + env.cmd_strafe * env.cmd_strafe);
        progress_speed = cmd_speed > 0.0 ? (forward_speed * env.cmd_forward + right_speed * env.cmd_strafe) / cmd_speed : 0.0;
        progress_command = cmd_speed;
    }
    float progress_cap = pc.progress_cap;
    if (loco_progress_to_command != 0) {
        progress_cap = min(progress_cap, progress_command);
    }
    loco_obs_data[rbase + 3] = stability * (pc.r_track * track + pc.r_progress * min(progress_speed, progress_cap));
    loco_obs_data[rbase + 4] = stability * pc.r_heading * heading;
    loco_obs_data[rbase + 5] = -(pc.r_energy * energy + pc.r_hold * hold) * inv_act - cocontract - pc.r_slip * slip - loco_r_scuff * scuff - loco_r_stand_lift * stand_lift - pc.r_touchdown * touchdowns - pc.r_stance_knee * knee_cost - pc.r_width * width_cost;
    loco_obs_data[rbase + 6] = -pc.r_smooth * smooth * inv_act;
    float gait = env.r_air * air + env.r_stride * stride;
    if (pc.r_gait != 0.0 && env.has_pose != 0) {
        float score_l = grounded_l == (env.ref_grounded_l != 0) ? 1.0 : -1.0;
        float score_r = grounded_r == (env.ref_grounded_r != 0) ? 1.0 : -1.0;
        gait += pc.r_gait * 0.5 * (score_l + score_r);
    }
    if (env.lead_weight != 0.0) {
        float3 feet = foot_r_pos - foot_l_pos;
        float order = feet.x * pelvis_forward.x + feet.z * pelvis_forward.z;
        if (env.lead_graded != 0) {
            gait += env.lead_weight * clamp(order / env.lead_reference, -1.0, 1.0);
        }
        else if (abs(order) >= loco_lead_margin) {
            gait += (order > 0.0) == (env.lead_reference > 0.0) ? env.lead_weight : -env.lead_weight;
        }
    }

    loco_obs_data[rbase + 7] = pc.r_pose * pose + gait;
    loco_obs_data[rbase + loco_reward_done] = (fallen || timeout) ? 1.0 : 0.0;
}
