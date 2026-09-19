float loco_hinge_angle_about_x(float4 parent, float4 child) {
    float3 theta = to_axis_angle(quat_mul(child, quat_conjugate(parent)));
    float3 axis_world = rotate_vector(parent, float3(1, 0, 0));
    return dot(axis_world, theta);
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

    float heading_error = 0.0;
    if (env.has_heading != 0) {
        float current_yaw = atan2(-pelvis_forward.x, -pelvis_forward.z);
        float diff = env.desired_yaw - current_yaw;
        float two_pi = 6.28318530717958647692;
        heading_error = diff - two_pi * round(diff / two_pi);
    }

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
    loco_obs_data[base + 28] = sin(env.phase * 6.28318530717958647692);
    loco_obs_data[base + 29] = cos(env.phase * 6.28318530717958647692);

    for (uint a = 0; a < pc.act_dim; ++a) {
        loco_obs_data[base + pc.obs_dim + a] = loco_act_data[e * pc.act_dim + a];
    }

    if (pc.write_reward == 0) return;

    float forward_speed = -velocity_body.z;
    float right_speed = velocity_body.x;
    float err_forward = (forward_speed - env.cmd_forward) / pc.track_tolerance;
    float err_right = (right_speed - env.cmd_strafe) / pc.track_tolerance;
    float track = exp(-(err_forward * err_forward + err_right * err_right));

    float upright = clamp(cross(pelvis_right, pelvis_forward).y, 0.0, 1.0);

    float pelvis_height = pelvis_pos.y - support_center.y;
    float target_above_support = env.target_height - support_center.y;
    float height = clamp(pelvis_height / target_above_support, 0.0, 1.0);

    float heading_ratio = atan2(-pelvis_forward.x, -pelvis_forward.z) / pc.heading_limit;
    float heading = exp(-heading_ratio * heading_ratio);

    float stability = upright * height;

    float energy = 0.0;
    float smooth = 0.0;
    for (uint k = 0; k < pc.act_count; ++k) {
        float a = loco_act_data[e * pc.act_count + k];
        float da = a - loco_prev_data[pbase + loco_prev_action + k];
        energy += a * a;
        smooth += da * da;
    }
    float inv_act = 1.0 / float(pc.act_count);

    if (pc.write_prev != 0) {
        for (uint k = 0; k < pc.act_count; ++k) {
            loco_prev_data[pbase + loco_prev_action + k] = loco_act_data[e * pc.act_count + k];
        }
    }

    loco_obs_data[rbase + 0] = pc.r_alive;
    loco_obs_data[rbase + 1] = pc.r_upright * upright;
    loco_obs_data[rbase + 2] = pc.r_height * height;
    loco_obs_data[rbase + 3] = stability * pc.r_track * track;
    loco_obs_data[rbase + 4] = stability * pc.r_heading * heading;
    loco_obs_data[rbase + 5] = -pc.r_energy * energy * inv_act;
    loco_obs_data[rbase + 6] = -pc.r_smooth * smooth * inv_act;
    bool fallen = pelvis_pos.y < pc.fall_height || upright < pc.fall_upright;
    bool timeout = pc.max_steps != 0 && env.episode_steps >= pc.max_steps;
    loco_obs_data[rbase + loco_reward_done] = (fallen || timeout) ? 1.0 : 0.0;
}
