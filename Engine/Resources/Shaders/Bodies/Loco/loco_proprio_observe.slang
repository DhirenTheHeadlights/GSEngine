float loco_proprio_hinge(float4 parent, float4 child, float3 axis_local) {
    float3 theta = to_axis_angle(quat_mul(child, quat_conjugate(parent)));
    return dot(rotate_vector(parent, axis_local), theta);
}

float loco_proprio_share(uint b) {
    if (b < 4) return loco_proprio_share_a[b];
    if (b < 8) return loco_proprio_share_b[b - 4];
    return loco_proprio_share_c[b - 8];
}

float3 loco_proprio_axis(uint k) {
    if (k == 0) return loco_proprio_axis_0;
    if (k == 1) return loco_proprio_axis_1;
    if (k == 2) return loco_proprio_axis_2;
    if (k == 3) return loco_proprio_axis_3;
    if (k == 4) return loco_proprio_axis_4;
    return loco_proprio_axis_5;
}

body_state loco_proprio_body(uint e, uint b) {
    return loco_body_data[loco_env_body_data[e * loco_proprio_body_count + b]];
}

float3 loco_proprio_heading(float3 v, float sin_yaw, float cos_yaw) {
    return float3(-v.x * sin_yaw - v.z * cos_yaw, v.x * cos_yaw - v.z * sin_yaw, v.y);
}

void loco_proprio_observe(uint3 dispatch_id) {
    uint e = dispatch_id.x;
    if (e >= pc.env_count) return;

    loco_muscle_row row = loco_muscle_table_data[e];
    loco_env_desc env = loco_env_data[e];
    uint prev = e * loco_proprio_prev_stride;
    uint base = e * pc.obs_stride + pc.obs_base;

    if (env.valid == 0 || row.driving == 0 || env.framed == 0) {
        for (uint k = 0; k < loco_proprio_width; ++k) {
            loco_obs_data[base + k] = 0.0;
        }
        loco_proprio_prev_data[prev + 18] = 0.0;
        return;
    }

    bool primed = loco_proprio_prev_data[prev + 18] != 0.0 && row.fresh == 0;
    float inv_dt = 1.0 / pc.dt;

    body_state pelvis = loco_proprio_body(e, 0);
    float4 pelvis_q = quat_mul(pelvis.orientation, env.segment_frame);
    float4 torso_q = quat_mul(loco_proprio_body(e, 11).orientation, env.segment_frame);
    float4 femur_l_q = quat_mul(loco_proprio_body(e, 6).orientation, env.segment_frame);
    float4 femur_r_q = quat_mul(loco_proprio_body(e, 1).orientation, env.segment_frame);

    float3 lumbar = float3(
        loco_proprio_hinge(pelvis_q, torso_q, float3(1, 0, 0)),
        loco_proprio_hinge(pelvis_q, torso_q, float3(0, 1, 0)),
        loco_proprio_hinge(pelvis_q, torso_q, float3(0, 0, 1))
    );
    float hip[4] = {
        loco_proprio_hinge(pelvis_q, femur_l_q, float3(0, 1, 0)),
        loco_proprio_hinge(pelvis_q, femur_l_q, float3(0, 0, 1)),
        loco_proprio_hinge(pelvis_q, femur_r_q, float3(0, 1, 0)),
        loco_proprio_hinge(pelvis_q, femur_r_q, float3(0, 0, 1)),
    };

    uint parents[6] = { 7, 8, 9, 2, 3, 4 };
    float foot[6];
    for (uint k = 0; k < 6; ++k) {
        foot[k] = loco_proprio_hinge(loco_proprio_body(e, parents[k]).orientation, loco_proprio_body(e, parents[k] + 1).orientation, loco_proprio_axis(k));
    }

    float3 forward = rotate_vector(pelvis_q, float3(0, 0, -1));
    float yaw = atan2(-forward.x, -forward.z);
    float pelvis_y = body_origin(pelvis.position.xyz, pelvis.orientation, pelvis.com_local.xyz).y;

    float3 com = pelvis.position.xyz;
    for (uint b = 1; b < loco_proprio_body_count; ++b) {
        com += (loco_proprio_body(e, b).position.xyz - pelvis.position.xyz) * loco_proprio_share(b);
    }
    body_state talus_l = loco_proprio_body(e, 8);
    body_state talus_r = loco_proprio_body(e, 3);
    float3 ankle_l = body_origin(talus_l.position.xyz, talus_l.orientation, talus_l.com_local.xyz);
    float3 ankle_r = body_origin(talus_r.position.xyz, talus_r.orientation, talus_r.com_local.xyz);
    float3 ankles = ankle_l + (ankle_r - ankle_l) * 0.5;
    float sin_yaw = sin(yaw);
    float cos_yaw = cos(yaw);

    float contact_y = pc.ground_y + pc.ground_tolerance;
    float contact[4] = {
        loco_proprio_body(e, 9).aabb_min.y <= contact_y ? 1.0 : 0.0,
        loco_proprio_body(e, 10).aabb_min.y <= contact_y ? 1.0 : 0.0,
        loco_proprio_body(e, 4).aabb_min.y <= contact_y ? 1.0 : 0.0,
        loco_proprio_body(e, 5).aabb_min.y <= contact_y ? 1.0 : 0.0,
    };

    for (uint k = 0; k < 3; ++k) {
        loco_obs_data[base + k] = lumbar[k];
        loco_obs_data[base + 3 + k] = primed ? (lumbar[k] - loco_proprio_prev_data[prev + k]) * inv_dt : 0.0;
        loco_proprio_prev_data[prev + k] = lumbar[k];
    }
    for (uint k = 0; k < 6; ++k) {
        loco_obs_data[base + 6 + k] = foot[k];
        loco_obs_data[base + 12 + k] = primed ? (foot[k] - loco_proprio_prev_data[prev + 3 + k]) * inv_dt : 0.0;
        loco_proprio_prev_data[prev + 3 + k] = foot[k];
    }
    for (uint k = 0; k < 4; ++k) {
        loco_obs_data[base + 18 + k] = primed ? (hip[k] - loco_proprio_prev_data[prev + 9 + k]) * inv_dt : 0.0;
        loco_proprio_prev_data[prev + 9 + k] = hip[k];
    }
    float yaw_step = yaw - loco_proprio_prev_data[prev + 13];
    loco_obs_data[base + 22] = primed ? atan2(sin(yaw_step), cos(yaw_step)) * inv_dt : 0.0;
    loco_proprio_prev_data[prev + 13] = yaw;
    loco_obs_data[base + 23] = primed ? (pelvis_y - loco_proprio_prev_data[prev + 14]) * inv_dt : 0.0;
    loco_proprio_prev_data[prev + 14] = pelvis_y;

    float3 com_before = float3(loco_proprio_prev_data[prev + 15], loco_proprio_prev_data[prev + 16], loco_proprio_prev_data[prev + 17]);
    float3 com_over = loco_proprio_heading(com - ankles, sin_yaw, cos_yaw);
    float3 com_velocity = loco_proprio_heading(primed ? (com - com_before) * inv_dt : float3(0, 0, 0), sin_yaw, cos_yaw);
    for (uint k = 0; k < 3; ++k) {
        loco_obs_data[base + 24 + k] = com_over[k];
        loco_obs_data[base + 27 + k] = com_velocity[k];
        loco_proprio_prev_data[prev + 15 + k] = com[k];
    }
    for (uint k = 0; k < 4; ++k) {
        loco_obs_data[base + 30 + k] = contact[k];
    }
    loco_proprio_prev_data[prev + 18] = 1.0;
}
