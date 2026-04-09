import common;
import vbd_shared;
import vbd_physics;

[[vk::push_constant]]
ConstantBuffer<vbd_push_constants> pc;

typedef float3x3 mat3;

mat3 outer_product3(float3 a, float3 b) {
    return mat3(
        a * b.x,
        a * b.y,
        a * b.z
    );
}

mat3 mat3_identity(float s) {
    return mat3(
        float3(s, 0, 0),
        float3(0, s, 0),
        float3(0, 0, s)
    );
}

mat3 mat3_add(mat3 a, mat3 b) {
    return mat3(a[0] + b[0], a[1] + b[1], a[2] + b[2]);
}

float3 mat3_mul_vec(mat3 m, float3 v) {
    return m[0] * v.x + m[1] * v.y + m[2] * v.z;
}

mat3 mat3_mul(mat3 a, mat3 b) {
    return mat3(
        mat3_mul_vec(a, b[0]),
        mat3_mul_vec(a, b[1]),
        mat3_mul_vec(a, b[2])
    );
}

mat3 mat3_scale(mat3 m, float s) {
    return mat3(m[0] * s, m[1] * s, m[2] * s);
}

mat3 mat3_transpose(mat3 m) {
    return mat3(
        float3(m[0].x, m[1].x, m[2].x),
        float3(m[0].y, m[1].y, m[2].y),
        float3(m[0].z, m[1].z, m[2].z)
    );
}

float mat3_det(mat3 m) {
    return m[0].x * (m[1].y * m[2].z - m[2].y * m[1].z)
         - m[1].x * (m[0].y * m[2].z - m[2].y * m[0].z)
         + m[2].x * (m[0].y * m[1].z - m[1].y * m[0].z);
}

bool is_finite3(float3 v) {
    return !isinf(v.x) && !isinf(v.y) && !isinf(v.z)
        && !isnan(v.x) && !isnan(v.y) && !isnan(v.z);
}

mat3 mat3_inverse(mat3 m) {
    float d = mat3_det(m);
    float diag_max = max(abs(m[0].x), max(abs(m[1].y), abs(m[2].z)));
    float threshold = diag_max * diag_max * diag_max * 1e-12;
    if (abs(d) < max(threshold, 1e-20)) {
        float s = (diag_max > 1e-20) ? 1.0 / diag_max : 1.0;
        return mat3_identity(s);
    }
    float inv_d = 1.0 / d;
    mat3 result;
    result[0] = float3(
        (m[1].y * m[2].z - m[2].y * m[1].z) * inv_d,
        (m[2].y * m[0].z - m[0].y * m[2].z) * inv_d,
        (m[0].y * m[1].z - m[1].y * m[0].z) * inv_d
    );
    result[1] = float3(
        (m[2].x * m[1].z - m[1].x * m[2].z) * inv_d,
        (m[0].x * m[2].z - m[2].x * m[0].z) * inv_d,
        (m[1].x * m[0].z - m[0].x * m[1].z) * inv_d
    );
    result[2] = float3(
        (m[1].x * m[2].y - m[2].x * m[1].y) * inv_d,
        (m[2].x * m[0].y - m[0].x * m[2].y) * inv_d,
        (m[0].x * m[1].y - m[1].x * m[0].y) * inv_d
    );
    return result;
}

void accumulate_geometric_stiffness(float3 r, float3 dir, float abs_force, inout mat3 ang_hess) {
    float rd = dot(r, dir);
    for (int j = 0; j < 3; j++) {
        float col_norm = 0.0;
        for (int i = 0; i < 3; i++) {
            float h = r[i] * dir[j];
            if (i == j) h -= rd;
            col_norm += abs(h);
        }
        ang_hess[j][j] += col_norm * abs_force;
    }
}

void accumulate_joint_for_body(
    uint bi, GpuJoint j, bool update_orient,
    inout float3 grad, inout mat3 hess,
    inout float3 ang_grad, inout mat3 ang_hess,
    inout mat3 hess_xt
) {
    GpuBody body_a = body_data[j.body_a];
    GpuBody body_b = body_data[j.body_b];
    bool is_a = (j.body_a == bi);
    float sign = is_a ? 1.0 : -1.0;

    float3 r_aw = rotate_vector(body_a.predicted_orientation, j.local_anchor_a.xyz);
    float3 r_bw = rotate_vector(body_b.predicted_orientation, j.local_anchor_b.xyz);
    float3 p_a = body_a.predicted_position.xyz + r_aw;
    float3 p_b = body_b.predicted_position.xyz + r_bw;
    float3 d = p_a - p_b;
    float3 r = is_a ? r_aw : r_bw;

    if (j.type == JOINT_TYPE_DISTANCE) {
        float d_mag = length(d);
        float3 d_hat = d_mag > 1e-7 ? d / d_mag : float3(0, 1, 0);
        float c = (d_mag - j.target_distance) - j.pos_c0.x * pc.current_alpha;
        if (j.damping > 0.0) {
            float3 vel_a = body_a.velocity.xyz + cross(body_a.angular_velocity.xyz, r_aw);
            float3 vel_b = body_b.velocity.xyz + cross(body_b.angular_velocity.xyz, r_bw);
            float v_rel = dot(d_hat, vel_a - vel_b);
            c += v_rel * pc.dt * j.damping;
        }
        float f = j.pos_penalty.x * c + j.pos_lambda.x;

        float3 j_lin = d_hat * sign;
        float3 j_ang = cross(r, d_hat) * sign;

        grad += j_lin * f;
        hess = mat3_add(hess, mat3_scale(outer_product3(j_lin, j_lin), j.pos_penalty.x));

        if (update_orient) {
            ang_grad += j_ang * f;
            ang_hess = mat3_add(ang_hess, mat3_scale(outer_product3(j_ang, j_ang), j.pos_penalty.x));
            hess_xt = mat3_add(hess_xt, mat3_scale(outer_product3(j_lin, j_ang), j.pos_penalty.x));
            accumulate_geometric_stiffness(r, d_hat, abs(f), ang_hess);
        }
    }
    else if (j.type == JOINT_TYPE_FIXED || j.type == JOINT_TYPE_HINGE) {
        float3 dirs[3] = { float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1) };
        for (int k = 0; k < 3; k++) {
            float c = dot(dirs[k], d) - j.pos_c0[k] * pc.current_alpha;
            float f = j.pos_penalty[k] * c + j.pos_lambda[k];

            float3 j_lin = dirs[k] * sign;
            float3 j_ang = cross(r, dirs[k]) * sign;

            grad += j_lin * f;
            hess = mat3_add(hess, mat3_scale(outer_product3(j_lin, j_lin), j.pos_penalty[k]));

            if (update_orient) {
                ang_grad += j_ang * f;
                ang_hess = mat3_add(ang_hess, mat3_scale(outer_product3(j_ang, j_ang), j.pos_penalty[k]));
                hess_xt = mat3_add(hess_xt, mat3_scale(outer_product3(j_lin, j_ang), j.pos_penalty[k]));
                accumulate_geometric_stiffness(r, dirs[k], abs(f), ang_hess);
            }
        }

        if (update_orient) {
            float4 q_error = quat_mul(body_b.predicted_orientation, quat_mul(quat_conjugate(body_a.predicted_orientation), quat_conjugate(j.rest_orientation)));
            float3 theta = to_axis_angle(q_error);

            if (j.type == JOINT_TYPE_FIXED) {
                for (int k = 0; k < 3; k++) {
                    float c_ang = theta[k] - j.ang_c0[k] * pc.current_alpha;
                    float f_ang = j.ang_penalty[k] * c_ang + j.ang_lambda[k];
                    float3 j_ang_dir = dirs[k] * (-sign);
                    ang_grad += j_ang_dir * f_ang;
                    ang_hess = mat3_add(ang_hess, mat3_scale(outer_product3(j_ang_dir, j_ang_dir), j.ang_penalty[k]));
                }
            }
            else {
                float3 axis_a = rotate_vector(body_a.predicted_orientation, j.local_axis_a.xyz);
                float3 axis_b = rotate_vector(body_b.predicted_orientation, j.local_axis_b.xyz);
                float3 swing_error = cross(axis_a, axis_b);

                float3 perp_u = cross(axis_a, float3(0, 1, 0));
                if (length(perp_u) < 1e-6) perp_u = cross(axis_a, float3(1, 0, 0));
                perp_u = normalize(perp_u);
                float3 perp_v = normalize(cross(axis_a, perp_u));

                float c_u = dot(perp_u, swing_error) - j.ang_c0.x * pc.current_alpha;
                float c_v = dot(perp_v, swing_error) - j.ang_c0.y * pc.current_alpha;
                float f_u = j.ang_penalty.x * c_u + j.ang_lambda.x;
                float f_v = j.ang_penalty.y * c_v + j.ang_lambda.y;

                float3 j_ang_u = perp_u * (-sign);
                float3 j_ang_v = perp_v * (-sign);

                ang_grad += j_ang_u * f_u;
                ang_hess = mat3_add(ang_hess, mat3_scale(outer_product3(j_ang_u, j_ang_u), j.ang_penalty.x));
                ang_grad += j_ang_v * f_v;
                ang_hess = mat3_add(ang_hess, mat3_scale(outer_product3(j_ang_v, j_ang_v), j.ang_penalty.y));

                if (j.limits_enabled != 0) {
                    float twist = dot(axis_a, theta);
                    bool limit_active = false;
                    float c_limit = 0.0;

                    if (twist < j.limit_lower) {
                        c_limit = (twist - j.limit_lower) - j.limit_c0 * pc.current_alpha;
                        limit_active = true;
                    }
                    else if (twist > j.limit_upper) {
                        c_limit = (twist - j.limit_upper) - j.limit_c0 * pc.current_alpha;
                        limit_active = true;
                    }

                    if (limit_active) {
                        float f_limit = j.limit_penalty * c_limit + j.limit_lambda;
                        float f_clamped = (c_limit < 0.0) ? min(f_limit, 0.0) : max(f_limit, 0.0);
                        float3 j_limit = axis_a * (-sign);
                        ang_grad += j_limit * f_clamped;
                        ang_hess = mat3_add(ang_hess, mat3_scale(outer_product3(j_limit, j_limit), j.limit_penalty));
                    }
                }
            }
        }
    }
    else if (j.type == JOINT_TYPE_SLIDER) {
        float3 axis_w = normalize(rotate_vector(body_a.predicted_orientation, j.local_axis_a.xyz));
        float3 perp0 = cross(axis_w, float3(0, 1, 0));
        if (length(perp0) < 1e-6) perp0 = cross(axis_w, float3(1, 0, 0));
        perp0 = normalize(perp0);
        float3 perp1 = normalize(cross(axis_w, perp0));

        float3 perps[2] = { perp0, perp1 };
        for (int k = 0; k < 2; k++) {
            float c = dot(perps[k], d) - j.pos_c0[k] * pc.current_alpha;
            float f = j.pos_penalty[k] * c + j.pos_lambda[k];

            float3 j_lin = perps[k] * sign;
            float3 j_ang = cross(r, perps[k]) * sign;

            grad += j_lin * f;
            hess = mat3_add(hess, mat3_scale(outer_product3(j_lin, j_lin), j.pos_penalty[k]));

            if (update_orient) {
                ang_grad += j_ang * f;
                ang_hess = mat3_add(ang_hess, mat3_scale(outer_product3(j_ang, j_ang), j.pos_penalty[k]));
                hess_xt = mat3_add(hess_xt, mat3_scale(outer_product3(j_lin, j_ang), j.pos_penalty[k]));
                accumulate_geometric_stiffness(r, perps[k], abs(f), ang_hess);
            }
        }

        if (update_orient) {
            float4 q_error = quat_mul(body_b.predicted_orientation, quat_mul(quat_conjugate(body_a.predicted_orientation), quat_conjugate(j.rest_orientation)));
            float3 theta = to_axis_angle(q_error);
            float3 dirs[3] = { float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1) };

            for (int k = 0; k < 3; k++) {
                float c_ang = theta[k] - j.ang_c0[k] * pc.current_alpha;
                float f_ang = j.ang_penalty[k] * c_ang + j.ang_lambda[k];
                float3 j_ang_dir = dirs[k] * (-sign);
                ang_grad += j_ang_dir * f_ang;
                ang_hess = mat3_add(ang_hess, mat3_scale(outer_product3(j_ang_dir, j_ang_dir), j.ang_penalty[k]));
            }
        }
    }
}

void solve_body(uint bi) {
    GpuBody body = body_data[bi];

    bool locked = (body.flags & FLAG_LOCKED) != 0;
    bool sleeping = body.sleep_counter >= SLEEP_THRESHOLD;
    bool update_orient = (body.flags & FLAG_UPDATE_ORIENTATION) != 0;

    if (locked || sleeping) return;

    float inv_mass_val = (body.mass > 1e-10) ? (1.0 / body.mass) : 0.0;
    if (inv_mass_val < 1e-10) return;

    float3 grad = float3(0, 0, 0);
    mat3 hess = mat3_identity(0);
    float3 ang_grad = float3(0, 0, 0);
    mat3 ang_hess = mat3_identity(0);
    mat3 hess_xt = mat3_identity(0);

    uint contact_offset = body_contact_map[bi];
    uint contact_count = body_contact_map[MAX_BODIES + bi];

    for (uint ci_idx = 0; ci_idx < contact_count; ci_idx++) {
        uint ci = body_contact_map[MAX_BODIES * 2 + contact_offset + ci_idx];
        GpuContact c = contact_data[ci];

        GpuBody body_a = body_data[c.body_a];
        GpuBody body_b = body_data[c.body_b];

        bool is_a = (c.body_a == bi);

        float3 world_r_a = rotate_vector(body_a.predicted_orientation, c.r_a.xyz);
        float3 world_r_b = rotate_vector(body_b.predicted_orientation, c.r_b.xyz);

        float3 p_a = body_a.predicted_position.xyz + world_r_a;
        float3 p_b = body_b.predicted_position.xyz + world_r_b;

        float3 d = p_a - p_b;
        float3 Cn = float3(
            dot(c.normal.xyz, d) + pc.collision_margin,
            dot(c.tangent_u.xyz, d),
            dot(c.tangent_v.xyz, d)
        );
        float3 C = Cn - c.c0_friction.xyz * pc.current_alpha;

        float3 force = float3(0, 0, 0);
        force.x = min(c.penalty.x * C.x + c.lambda.x, 0.0);
        float friction_bound = abs(c.lambda.x) * c.c0_friction.w;
        force.y = clamp(c.penalty.y * C.y + c.lambda.y, -friction_bound, friction_bound);
        force.z = clamp(c.penalty.z * C.z + c.lambda.z, -friction_bound, friction_bound);

        float sign = is_a ? 1.0 : -1.0;
        float3 r = is_a ? world_r_a : world_r_b;
        float3 dirs[3] = { c.normal.xyz, c.tangent_u.xyz, c.tangent_v.xyz };

        for (uint row = 0; row < 3; row++) {
            if (row > 0 && friction_bound <= 1e-10) continue;
            if (abs(force[row]) < 1e-12 && c.penalty[row] < 1e-6) continue;

            float3 J_lin = dirs[row] * sign;
            float3 J_ang = cross(r, dirs[row]) * sign;

            grad += J_lin * force[row];
            hess = mat3_add(hess, mat3_scale(outer_product3(J_lin, J_lin), c.penalty[row]));

            if (update_orient) {
                ang_grad += J_ang * force[row];
                ang_hess = mat3_add(ang_hess, mat3_scale(outer_product3(J_ang, J_ang), c.penalty[row]));
                hess_xt = mat3_add(hess_xt, mat3_scale(outer_product3(J_lin, J_ang), c.penalty[row]));
            }
        }
    }

    uint motor_idx = body_contact_map[MAX_BODIES * 2 + MAX_CONTACTS * 2 + bi];
    if (motor_idx != 0xFFFFFFFF && motor_idx < pc.motor_count) {
        GpuMotor m = motor_data[motor_idx];
        float alpha = max(m.compliance, 1e-6);
        float inertia_weight = body.mass / pc.h_squared;
        float stiffness = inertia_weight / alpha;

        float3 diff = body.predicted_position.xyz - body.motor_target.xyz;
        if (m.horizontal_only != 0) diff.y = 0.0;

        float3 motor_grad = diff * stiffness;
        float grad_mag = length(motor_grad);
        if (grad_mag > m.max_force) {
            motor_grad *= (m.max_force / grad_mag);
        }

        float grad_before = length(motor_grad);

        for (uint ci_idx = 0; ci_idx < contact_count; ci_idx++) {
            uint ci = body_contact_map[MAX_BODIES * 2 + contact_offset + ci_idx];
            GpuContact c = contact_data[ci];
            bool is_a = (c.body_a == bi);
            uint other = is_a ? c.body_b : c.body_a;
            GpuBody other_body = body_data[other];
            bool blocking_body =
                ((other_body.flags & FLAG_LOCKED) != 0) ||
                (other_body.mass >= body.mass);
            if (!blocking_body) continue;

            GpuBody contact_body_a = body_data[c.body_a];
            GpuBody contact_body_b = body_data[c.body_b];
            float3 contact_r_a = rotate_vector(contact_body_a.predicted_orientation, c.r_a.xyz);
            float3 contact_r_b = rotate_vector(contact_body_b.predicted_orientation, c.r_b.xyz);
            float3 contact_p_a = contact_body_a.predicted_position.xyz + contact_r_a;
            float3 contact_p_b = contact_body_b.predicted_position.xyz + contact_r_b;
            float3 contact_d = contact_p_a - contact_p_b;
            float normal_gap = dot(c.normal.xyz, contact_d) + pc.collision_margin;
            if (normal_gap >= 0.0 && c.lambda.x >= -1e-3) continue;

            float3 push_dir = is_a ? c.normal.xyz : -c.normal.xyz;
            float proj = dot(motor_grad, push_dir);
            if (proj > 0.0) {
                motor_grad -= push_dir * proj;
            }
        }

        float grad_after = length(motor_grad);
        float motor_scale = (grad_before > 1e-10) ? (grad_after / grad_before) : 0.0;

        grad += motor_grad;

        mat3 motor_hess = mat3_identity(0);
        motor_hess[0].x = stiffness * motor_scale;
        motor_hess[1].y = (m.horizontal_only != 0) ? 0.0 : stiffness * motor_scale;
        motor_hess[2].z = stiffness * motor_scale;
        hess = mat3_add(hess, motor_hess);
    }

    uint joint_offset = body_contact_map[JOINT_MAP_BASE + bi];
    uint joint_count_for_body = body_contact_map[JOINT_MAP_BASE + MAX_BODIES + bi];

    for (uint ji_idx = 0; ji_idx < joint_count_for_body; ji_idx++) {
        uint ji = body_contact_map[JOINT_MAP_BASE + MAX_BODIES * 2 + joint_offset + ji_idx];
        GpuJoint j = joint_data[ji];
        accumulate_joint_for_body(bi, j, update_orient, grad, hess, ang_grad, ang_hess, hess_xt);
    }

    float inertia_weight = body.mass / pc.h_squared;
    float3 x_diff = body.predicted_position.xyz - body.inertia_target.xyz;
    float3 g_lin = x_diff * inertia_weight + grad;

    float reg = 1e-6;
    mat3 h_xx = mat3_add(mat3_identity(inertia_weight), hess);
    h_xx[0].x += reg;
    h_xx[1].y += reg;
    h_xx[2].z += reg;

    if (!update_orient) {
        mat3 inv_h = mat3_inverse(h_xx);
        float3 delta_x = -mat3_mul_vec(inv_h, g_lin);

        if (!is_finite3(delta_x)) { body_data[bi] = body; return; }
        float step_size = length(delta_x);
        if (step_size > 0.5) delta_x *= (0.5 / step_size);

        body.predicted_position.xyz += delta_x;
        body_data[bi] = body;
        return;
    }

    float4 q_rel = quat_mul(body.predicted_orientation, quat_conjugate(body.angular_inertia_target));
    float q_s = q_rel.x;
    float3 q_v = q_rel.yzw;
    if (q_s < 0.0) { q_s = -q_s; q_v = -q_v; }

    float3 theta_diff;
    if (q_s < 0.9999) {
        float angle = 2.0 * acos(clamp(q_s, 0.0, 1.0));
        float sin_half = sqrt(1.0 - q_s * q_s);
        theta_diff = (sin_half > 1e-6) ? q_v * (angle / sin_half) : q_v * 2.0;
    } else {
        theta_diff = q_v * 2.0;
    }

    mat3 i_inv = mat3(body.inv_inertia_col0.xyz, body.inv_inertia_col1.xyz, body.inv_inertia_col2.xyz);
    mat3 i_mat = mat3_inverse(i_inv);
    float ang_scale = 1.0 / pc.h_squared;

    float3 g_ang = mat3_mul_vec(i_mat, theta_diff) * ang_scale + ang_grad;

    float ang_reg = 1e-6;
    mat3 h_tt = mat3_add(mat3_scale(i_mat, ang_scale), ang_hess);
    h_tt[0].x += ang_reg;
    h_tt[1].y += ang_reg;
    h_tt[2].z += ang_reg;

    mat3 h_tx = mat3_transpose(hess_xt);
    mat3 h_xx_inv = mat3_inverse(h_xx);
    mat3 s = mat3_add(h_tt, mat3_scale(mat3_mul(h_tx, mat3_mul(h_xx_inv, hess_xt)), -1.0));
    mat3 s_inv = mat3_inverse(s);

    float3 delta_theta = -mat3_mul_vec(s_inv, g_ang - mat3_mul_vec(mat3_mul(h_tx, h_xx_inv), g_lin));
    float3 delta_x = -mat3_mul_vec(h_xx_inv, g_lin + mat3_mul_vec(hess_xt, delta_theta));

    if (!is_finite3(delta_x) || !is_finite3(delta_theta)) { body_data[bi] = body; return; }
    float lin_size = length(delta_x);
    if (lin_size > 0.5) delta_x *= (0.5 / lin_size);
    float ang_size = length(delta_theta);
    if (ang_size > 0.5) delta_theta *= (0.5 / ang_size);

    body.predicted_position.xyz += delta_x;

    float angle = length(delta_theta);
    if (angle > 1e-7) {
        float3 axis = delta_theta / angle;
        float half_angle = angle * 0.5;
        float sh = sin(half_angle);
        float ch = cos(half_angle);
        float4 delta_q = float4(ch, sh * axis.x, sh * axis.y, sh * axis.z);
        body.predicted_orientation = quat_normalize(quat_mul(delta_q, body.predicted_orientation));
    }

    body_data[bi] = body;
}

static const uint BARRIER_COUNTER_INDEX = 3;

groupshared uint gs_barrier_target;

void global_barrier(uint local_id, uint num_groups) {
    DeviceMemoryBarrierWithGroupSync();

    if (local_id == 0) {
        uint prev;
        InterlockedAdd(collision_state[BARRIER_COUNTER_INDEX], 1u, prev);
        gs_barrier_target = ((prev / num_groups) + 1u) * num_groups;
    }

    GroupMemoryBarrierWithGroupSync();

    if (local_id == 0) {
        uint target = gs_barrier_target;
        while (true) {
            uint val;
            InterlockedAdd(collision_state[BARRIER_COUNTER_INDEX], 0u, val);
            if (val >= target) break;
        }
    }

    DeviceMemoryBarrierWithGroupSync();
}

[Layout(LayoutKind::VBDPhysics)]
[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID) {
    uint idx = dispatch_id.x;
    uint local_id = group_thread_id.x;
    uint num_groups = (pc.body_count + 63u) / 64u;
    uint num_colors = pc.color_count;

    for (uint color = 0; color < num_colors; color++) {
        uint count = color_data[MAX_COLORS + color];
        if (idx < count) {
            uint offset = color_data[color];
            uint bi = color_data[MAX_COLORS * 2 + offset + idx];
            if (bi < pc.body_count) {
                solve_body(bi);
            }
        }

        if (color + 1u < num_colors) {
            global_barrier(local_id, num_groups);
        }
    }
}
