struct GpuBody {
    float4 position;
    float4 predicted_position;
    float4 inertia_target;
    float4 old_position;
    float4 velocity;
    float4 predicted_velocity;
    float4 orientation;
    float4 predicted_orientation;
    float4 angular_inertia_target;
    float4 old_orientation;
    float4 angular_velocity;
    float4 predicted_angular_velocity;
    float4 motor_target;
    float mass;
    uint flags;
    uint sleep_counter;
    float _pad0;
    float4 inv_inertia_col0;
    float4 inv_inertia_col1;
    float4 inv_inertia_col2;
};

struct GpuContact {
    uint body_a;
    uint body_b;
    float initial_separation;
    float lambda;
    float4 normal;
    float4 r_a;
    float4 r_b;
    float friction_coeff;
    float mass_a;
    float mass_b;
    float _pad0;
};

struct GpuMotor {
    uint body_index;
    float compliance;
    float max_force;
    uint horizontal_only;
    float4 target_velocity;
};

static const uint FLAG_LOCKED = 1;
static const uint FLAG_UPDATE_ORIENTATION = 2;
static const uint FLAG_AFFECTED_BY_GRAVITY = 4;
static const uint SLEEP_THRESHOLD = 300;
static const uint MAX_BODIES = 500;
static const uint MAX_CONTACTS = 2000;

struct push_constants {
    uint body_count;
    uint contact_count;
    uint motor_count;
    uint color_offset;
    uint color_count;
    float h_squared;
    float dt;
    float rho;
    float linear_damping;
    float velocity_sleep_threshold;
    uint substep;
    uint iteration;
};

[[vk::binding(0, 0)]] RWStructuredBuffer<GpuBody> body_data;
[[vk::binding(1, 0)]] RWStructuredBuffer<GpuContact> contact_data;
[[vk::binding(2, 0)]] StructuredBuffer<GpuMotor> motor_data;
[[vk::binding(3, 0)]] StructuredBuffer<uint> color_data;
[[vk::binding(4, 0)]] StructuredBuffer<uint> body_contact_map;
[[vk::binding(5, 0)]] RWStructuredBuffer<float4> solve_state;

[[vk::push_constant]]
ConstantBuffer<push_constants> pc;

float4 quat_mul(float4 a, float4 b) {
    return float4(
        a.x * b.x - a.y * b.y - a.z * b.z - a.w * b.w,
        a.x * b.y + a.y * b.x + a.z * b.w - a.w * b.z,
        a.x * b.z - a.y * b.w + a.z * b.x + a.w * b.y,
        a.x * b.w + a.y * b.z - a.z * b.y + a.w * b.x
    );
}

float4 quat_normalize(float4 q) {
    float len = length(q);
    return len > 1e-10 ? q / len : float4(1, 0, 0, 0);
}

float4 quat_conjugate(float4 q) {
    return float4(q.x, -q.y, -q.z, -q.w);
}

float3 rotate_vector(float4 q, float3 v) {
    float3 u = q.yzw;
    float s = q.x;
    return 2.0 * dot(u, v) * u + (s * s - dot(u, u)) * v + 2.0 * s * cross(u, v);
}

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

mat3 mat3_inverse(mat3 m) {
    float d = mat3_det(m);
    if (abs(d) < 1e-20) return mat3_identity(1.0);
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

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint thread_idx = dispatch_id.x;
    if (thread_idx >= pc.color_count) return;

    uint bi = color_data[32 * 2 + pc.color_offset + thread_idx];
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

        float3 world_r_a = rotate_vector(body_a.predicted_orientation, c.r_a.xyz);
        float3 world_r_b = rotate_vector(body_b.predicted_orientation, c.r_b.xyz);

        float3 p_a = body_a.predicted_position.xyz + world_r_a;
        float3 p_b = body_b.predicted_position.xyz + world_r_b;

        float gap = dot(p_b - p_a, c.normal.xyz) + c.initial_separation;
        if (gap >= 0.0) continue;

        mat3 n_outer_n = outer_product3(c.normal.xyz, c.normal.xyz);

        bool is_a = (c.body_a == bi);

        float mass_self = is_a ? c.mass_a : c.mass_b;
        float mass_other = is_a ? c.mass_b : c.mass_a;
        bool other_locked = is_a ? ((body_b.flags & FLAG_LOCKED) != 0) : ((body_a.flags & FLAG_LOCKED) != 0);

        float mass_scale = other_locked ? 1.0 : min(1.0, mass_other / max(mass_self, 1e-10));
        float rho_val = pc.rho * mass_self * mass_scale / pc.h_squared;
        float effective = rho_val * gap;

        float3 r_unitless = is_a ? world_r_a : world_r_b;
        float3 r_cross_n = cross(r_unitless, c.normal.xyz);

        if (is_a) {
            grad -= c.normal.xyz * effective;
        } else {
            grad += c.normal.xyz * effective;
        }
        hess = mat3_add(hess, mat3_scale(n_outer_n, rho_val));

        if (update_orient) {
            if (is_a) {
                ang_grad -= r_cross_n * effective;
            } else {
                ang_grad += r_cross_n * effective;
            }
            ang_hess = mat3_add(ang_hess, mat3_scale(outer_product3(r_cross_n, r_cross_n), rho_val));
            if (is_a) {
                hess_xt = mat3_add(hess_xt, mat3_scale(outer_product3(c.normal.xyz, r_cross_n), rho_val));
            } else {
                hess_xt = mat3_add(hess_xt, mat3_scale(outer_product3(c.normal.xyz, r_cross_n), rho_val));
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

        grad += motor_grad;

        mat3 motor_hess = mat3_identity(0);
        motor_hess[0].x = stiffness;
        motor_hess[1].y = (m.horizontal_only != 0) ? 0.0 : stiffness;
        motor_hess[2].z = stiffness;
        hess = mat3_add(hess, motor_hess);
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

    mat3 h_tt = mat3_add(mat3_scale(i_mat, ang_scale), ang_hess);
    h_tt[0].x += reg;
    h_tt[1].y += reg;
    h_tt[2].z += reg;

    mat3 h_tx = mat3_transpose(hess_xt);
    mat3 h_xx_inv = mat3_inverse(h_xx);
    mat3 s = mat3_add(h_tt, mat3_scale(mat3_mul(h_tx, mat3_mul(h_xx_inv, hess_xt)), -1.0));
    mat3 s_inv = mat3_inverse(s);

    float3 delta_theta = -mat3_mul_vec(s_inv, g_ang - mat3_mul_vec(mat3_mul(h_tx, h_xx_inv), g_lin));
    float3 delta_x = -mat3_mul_vec(h_xx_inv, g_lin + mat3_mul_vec(hess_xt, delta_theta));

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
