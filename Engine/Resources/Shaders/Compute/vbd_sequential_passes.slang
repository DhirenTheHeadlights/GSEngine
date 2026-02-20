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
    float4 half_extents;
    float4 aabb_min;
    float4 aabb_max;
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
    uint feature_packed;
};

struct GpuWarmStart {
    uint body_a;
    uint body_b;
    uint feature_packed;
    float lambda;
};

static const uint FLAG_LOCKED = 1;
static const uint FLAG_UPDATE_ORIENTATION = 2;
static const uint SLEEP_THRESHOLD = 300;
static const uint MAX_BODIES = 500;
static const uint MAX_CONTACTS = 2000;
static const uint MAX_COLLISION_PAIRS = 8192;
static const uint MAX_COLORS = 32;

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
[[vk::binding(2, 0)]] StructuredBuffer<uint> motor_data;
[[vk::binding(3, 0)]] StructuredBuffer<uint> color_data;
[[vk::binding(4, 0)]] RWStructuredBuffer<uint> body_contact_map;
[[vk::binding(5, 0)]] RWStructuredBuffer<float4> solve_state;
[[vk::binding(6, 0)]] RWStructuredBuffer<uint> collision_pairs;
[[vk::binding(7, 0)]] RWStructuredBuffer<uint> collision_state;
[[vk::binding(8, 0)]] StructuredBuffer<GpuWarmStart> warm_starts;

[[vk::push_constant]]
ConstantBuffer<push_constants> pc;

float3 rotate_vector(float4 q, float3 v) {
    float3 u = q.yzw;
    float s = q.x;
    return 2.0 * dot(u, v) * u + (s * s - dot(u, u)) * v + 2.0 * s * cross(u, v);
}

typedef float3x3 mat3;

float3 mat3_mul_vec(mat3 m, float3 v) {
    return m[0] * v.x + m[1] * v.y + m[2] * v.z;
}

void apply_velocity_correction(uint bi) {
    GpuBody my_body = body_data[bi];
    if ((my_body.flags & FLAG_LOCKED) != 0) return;
    if (my_body.sleep_counter >= SLEEP_THRESHOLD) return;
    bool my_orient = (my_body.flags & FLAG_UPDATE_ORIENTATION) != 0;

    uint co = body_contact_map[bi];
    uint cc = body_contact_map[MAX_BODIES + bi];

    bool do_normal = pc.iteration < 2;
    uint motor_map_offset = MAX_BODIES * 2 + MAX_CONTACTS * 2;

    for (uint ci_idx = 0; ci_idx < cc; ci_idx++) {
        uint ci = body_contact_map[MAX_BODIES * 2 + co + ci_idx];
        GpuContact ct = contact_data[ci];
        if (ct.lambda <= 0.0) continue;

        if (!do_normal) {
            bool a_has_motor = body_contact_map[motor_map_offset + ct.body_a] != 0xFFFFFFFF;
            bool b_has_motor = body_contact_map[motor_map_offset + ct.body_b] != 0xFFFFFFFF;
            if (a_has_motor || b_has_motor) continue;
        }

        GpuBody ba = body_data[ct.body_a];
        GpuBody bb = body_data[ct.body_b];
        bool is_a = (ct.body_a == bi);

        bool a_locked = (ba.flags & FLAG_LOCKED) != 0;
        bool b_locked = (bb.flags & FLAG_LOCKED) != 0;
        if (a_locked && b_locked) continue;
        bool a_orient = (ba.flags & FLAG_UPDATE_ORIENTATION) != 0;
        bool b_orient = (bb.flags & FLAG_UPDATE_ORIENTATION) != 0;

        float w_a_lin = a_locked ? 0.0 : (1.0 / max(ba.mass, 1e-10));
        float w_b_lin = b_locked ? 0.0 : (1.0 / max(bb.mass, 1e-10));

        mat3 inv_i_a = mat3(ba.inv_inertia_col0.xyz, ba.inv_inertia_col1.xyz, ba.inv_inertia_col2.xyz);
        mat3 inv_i_b = mat3(bb.inv_inertia_col0.xyz, bb.inv_inertia_col1.xyz, bb.inv_inertia_col2.xyz);

        float3 world_r_a = rotate_vector(ba.predicted_orientation, ct.r_a.xyz);
        float3 world_r_b = rotate_vector(bb.predicted_orientation, ct.r_b.xyz);

        float3 vel_a = is_a ? my_body.velocity.xyz : ba.velocity.xyz;
        float3 ang_a = is_a ? my_body.angular_velocity.xyz : ba.angular_velocity.xyz;
        float3 vel_b = is_a ? bb.velocity.xyz : my_body.velocity.xyz;
        float3 ang_b = is_a ? bb.angular_velocity.xyz : my_body.angular_velocity.xyz;

        float3 v_a = vel_a + cross(ang_a, world_r_a);
        float3 v_b = vel_b + cross(ang_b, world_r_b);
        float3 v_rel = v_a - v_b;

        if (do_normal) {
            float v_n = dot(v_rel, ct.normal.xyz);
            if (v_n >= 0.0) continue;

            float3 rcross_a_n = cross(world_r_a, ct.normal.xyz);
            float3 rcross_b_n = cross(world_r_b, ct.normal.xyz);

            float3 iircn_a = mat3_mul_vec(inv_i_a, rcross_a_n);
            float3 iircn_b = mat3_mul_vec(inv_i_b, rcross_b_n);

            float w_rot_a = (a_locked || !a_orient) ? 0.0 : dot(rcross_a_n, iircn_a);
            float w_rot_b = (b_locked || !b_orient) ? 0.0 : dot(rcross_b_n, iircn_b);

            float w_total = w_a_lin + w_b_lin + w_rot_a + w_rot_b;
            if (w_total < 1e-10) continue;

            float delta_lambda = -v_n / w_total;
            if (isnan(delta_lambda) || isinf(delta_lambda)) continue;

            if (is_a) {
                my_body.velocity.xyz += ct.normal.xyz * (w_a_lin * delta_lambda);
                if (my_orient) my_body.angular_velocity.xyz += iircn_a * delta_lambda;
            } else {
                my_body.velocity.xyz -= ct.normal.xyz * (w_b_lin * delta_lambda);
                if (my_orient) my_body.angular_velocity.xyz -= iircn_b * delta_lambda;
            }
        } else {
            float3 v_t = v_rel - dot(v_rel, ct.normal.xyz) * ct.normal.xyz;
            float v_t_mag = length(v_t);
            if (v_t_mag < 1e-7) continue;

            float3 t = v_t / v_t_mag;
            float3 rcross_a_t = cross(world_r_a, t);
            float3 rcross_b_t = cross(world_r_b, t);

            float3 iirct_a = mat3_mul_vec(inv_i_a, rcross_a_t);
            float3 iirct_b = mat3_mul_vec(inv_i_b, rcross_b_t);

            float w_rot_a_t = (a_locked || !a_orient) ? 0.0 : dot(rcross_a_t, iirct_a);
            float w_rot_b_t = (b_locked || !b_orient) ? 0.0 : dot(rcross_b_t, iirct_b);

            float w_total_t = w_a_lin + w_b_lin + w_rot_a_t + w_rot_b_t;
            if (w_total_t < 1e-10) continue;

            float delta_lambda_t = -v_t_mag / w_total_t;
            float max_friction = ct.friction_coeff * ct.lambda * sqrt(pc.h_squared);
            delta_lambda_t = clamp(delta_lambda_t, -max_friction, max_friction);
            if (isnan(delta_lambda_t) || isinf(delta_lambda_t)) continue;

            if (is_a) {
                my_body.velocity.xyz += t * (w_a_lin * delta_lambda_t);
                if (my_orient) my_body.angular_velocity.xyz += iirct_a * delta_lambda_t;
            } else {
                my_body.velocity.xyz -= t * (w_b_lin * delta_lambda_t);
                if (my_orient) my_body.angular_velocity.xyz -= iirct_b * delta_lambda_t;
            }
        }
    }

    body_data[bi].velocity = my_body.velocity;
    body_data[bi].angular_velocity = my_body.angular_velocity;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint idx = dispatch_id.x;
    uint color_idx = pc.color_offset;
    uint count = color_data[MAX_COLORS + color_idx];
    if (idx >= count) return;
    uint offset = color_data[color_idx];
    uint bi = color_data[MAX_COLORS * 2 + offset + idx];
    if (bi >= pc.body_count) return;
    apply_velocity_correction(bi);
}
