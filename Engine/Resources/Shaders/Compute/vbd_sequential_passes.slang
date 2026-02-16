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

static const uint FLAG_LOCKED = 1;
static const uint FLAG_UPDATE_ORIENTATION = 2;

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
[[vk::binding(4, 0)]] StructuredBuffer<uint> body_contact_map;
[[vk::binding(5, 0)]] RWStructuredBuffer<float4> solve_state;

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

uint hash_rng(uint seed) {
    seed ^= seed >> 16;
    seed *= 0x45d9f3b;
    seed ^= seed >> 16;
    seed *= 0x45d9f3b;
    seed ^= seed >> 16;
    return seed;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint num_contacts = pc.contact_count;
    if (num_contacts == 0) return;

    for (int pass = 0; pass < 3; pass++) {
        for (uint ci = 0; ci < num_contacts; ci++) {
            GpuContact c = contact_data[ci];
            if (c.lambda <= 0.0) continue;

            GpuBody ba = body_data[c.body_a];
            GpuBody bb = body_data[c.body_b];

            bool a_locked = (ba.flags & FLAG_LOCKED) != 0;
            bool b_locked = (bb.flags & FLAG_LOCKED) != 0;
            bool a_orient = (ba.flags & FLAG_UPDATE_ORIENTATION) != 0;
            bool b_orient = (bb.flags & FLAG_UPDATE_ORIENTATION) != 0;

            float3 world_r_a = rotate_vector(ba.predicted_orientation, c.r_a.xyz);
            float3 world_r_b = rotate_vector(bb.predicted_orientation, c.r_b.xyz);

            float3 v_a = ba.velocity.xyz + cross(ba.angular_velocity.xyz, world_r_a);
            float3 v_b = bb.velocity.xyz + cross(bb.angular_velocity.xyz, world_r_b);

            float v_n = dot(v_a - v_b, c.normal.xyz);
            if (v_n >= 0.0) continue;

            float w_a_lin = a_locked ? 0.0 : (1.0 / max(ba.mass, 1e-10));
            float w_b_lin = b_locked ? 0.0 : (1.0 / max(bb.mass, 1e-10));

            mat3 inv_i_a = mat3(ba.inv_inertia_col0.xyz, ba.inv_inertia_col1.xyz, ba.inv_inertia_col2.xyz);
            mat3 inv_i_b = mat3(bb.inv_inertia_col0.xyz, bb.inv_inertia_col1.xyz, bb.inv_inertia_col2.xyz);

            float3 rcross_a = cross(world_r_a, c.normal.xyz);
            float3 rcross_b = cross(world_r_b, c.normal.xyz);

            float3 iircn_a = mat3_mul_vec(inv_i_a, rcross_a);
            float3 iircn_b = mat3_mul_vec(inv_i_b, rcross_b);

            float w_rot_a = (a_locked || !a_orient) ? 0.0 : dot(rcross_a, iircn_a);
            float w_rot_b = (b_locked || !b_orient) ? 0.0 : dot(rcross_b, iircn_b);

            float w_total = w_a_lin + w_b_lin + w_rot_a + w_rot_b;
            if (w_total < 1e-10) continue;

            float delta_lambda = -v_n / w_total;

            if (!a_locked) {
                ba.velocity.xyz += c.normal.xyz * (w_a_lin * delta_lambda);
                if (a_orient) ba.angular_velocity.xyz += iircn_a * delta_lambda;
            }
            if (!b_locked) {
                bb.velocity.xyz -= c.normal.xyz * (w_b_lin * delta_lambda);
                if (b_orient) bb.angular_velocity.xyz -= iircn_b * delta_lambda;
            }

            body_data[c.body_a] = ba;
            body_data[c.body_b] = bb;
        }
    }

    uint rng_state = 42 + pc.substep * 1337;
    for (int vi = 0; vi < 8; vi++) {
        for (uint raw_ci = 0; raw_ci < num_contacts; raw_ci++) {
            uint ci = hash_rng(rng_state + raw_ci + vi * num_contacts) % num_contacts;

            GpuContact c = contact_data[ci];
            if (c.lambda <= 0.0) continue;

            GpuBody ba = body_data[c.body_a];
            GpuBody bb = body_data[c.body_b];

            bool a_locked = (ba.flags & FLAG_LOCKED) != 0;
            bool b_locked = (bb.flags & FLAG_LOCKED) != 0;
            bool a_orient = (ba.flags & FLAG_UPDATE_ORIENTATION) != 0;
            bool b_orient = (bb.flags & FLAG_UPDATE_ORIENTATION) != 0;

            float w_a_lin = a_locked ? 0.0 : (1.0 / max(ba.mass, 1e-10));
            float w_b_lin = b_locked ? 0.0 : (1.0 / max(bb.mass, 1e-10));

            mat3 inv_i_a = mat3(ba.inv_inertia_col0.xyz, ba.inv_inertia_col1.xyz, ba.inv_inertia_col2.xyz);
            mat3 inv_i_b = mat3(bb.inv_inertia_col0.xyz, bb.inv_inertia_col1.xyz, bb.inv_inertia_col2.xyz);

            float3 world_r_a = rotate_vector(ba.predicted_orientation, c.r_a.xyz);
            float3 world_r_b = rotate_vector(bb.predicted_orientation, c.r_b.xyz);

            float3 v_a = ba.velocity.xyz + cross(ba.angular_velocity.xyz, world_r_a);
            float3 v_b = bb.velocity.xyz + cross(bb.angular_velocity.xyz, world_r_b);

            float3 v_rel = v_a - v_b;
            float3 v_t = v_rel - dot(v_rel, c.normal.xyz) * c.normal.xyz;
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
            float max_friction = c.friction_coeff * c.lambda * pc.h_squared;
            delta_lambda_t = clamp(delta_lambda_t, -max_friction, max_friction);

            if (!a_locked) {
                ba.velocity.xyz += t * (w_a_lin * delta_lambda_t);
                if (a_orient) ba.angular_velocity.xyz += iirct_a * delta_lambda_t;
            }
            if (!b_locked) {
                bb.velocity.xyz -= t * (w_b_lin * delta_lambda_t);
                if (b_orient) bb.angular_velocity.xyz -= iirct_b * delta_lambda_t;
            }

            body_data[c.body_a] = ba;
            body_data[c.body_b] = bb;
        }
    }
}
