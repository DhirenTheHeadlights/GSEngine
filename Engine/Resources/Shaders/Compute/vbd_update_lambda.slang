static const uint MAX_CONTACTS = 2000;

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
    float accel_weight;
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
    uint feature_key_hi;
    uint feature_key_lo;
    uint sticking;
    float4 normal;
    float4 tangent_u;
    float4 tangent_v;
    float4 r_a;
    float4 r_b;
    float4 c0_friction;
    float4 lambda;
    float4 penalty;
};

struct GpuWarmStart {
    uint body_a;
    uint body_b;
    uint feature_key_hi;
    uint feature_key_lo;
    uint sticking;
    float4 normal;
    float4 tangent_u;
    float4 tangent_v;
    float4 local_anchor_a;
    float4 local_anchor_b;
    float4 lambda;
    float4 penalty;
};

static const uint FLAG_LOCKED = 1;

struct push_constants {
    uint body_count;
    uint contact_count;
    uint motor_count;
    uint color_offset;
    uint color_count;
    uint warm_start_count;
    uint post_stabilize;
    uint _pad0;
    float h_squared;
    float dt;
    float beta;
    float linear_damping;
    float velocity_sleep_threshold;
    float current_alpha;
    float collision_margin;
    float friction_coefficient;
    float penalty_min;
    float penalty_max;
    float gamma;
    float solver_alpha;
    float speculative_margin;
    float stick_threshold;
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

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint ci = dispatch_id.x;
    uint actual_contact_count = min(collision_state[0], MAX_CONTACTS);
    if (ci >= actual_contact_count) return;

    GpuContact c = contact_data[ci];
    GpuBody body_a = body_data[c.body_a];
    GpuBody body_b = body_data[c.body_b];

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

    float4 lambda = c.lambda;
    float4 penalty = c.penalty;

    lambda.x = min(penalty.x * C.x + lambda.x, 0.0);

    float friction_bound = abs(lambda.x) * c.c0_friction.w;
    lambda.y = clamp(penalty.y * C.y + lambda.y, -friction_bound, friction_bound);
    lambda.z = clamp(penalty.z * C.z + lambda.z, -friction_bound, friction_bound);

    if (lambda.x < 0.0) {
        penalty.x = min(penalty.x + pc.beta * abs(C.x), pc.penalty_max);
    }

    if (friction_bound > 1e-10) {
        if (abs(lambda.y) < friction_bound) {
            penalty.y = min(penalty.y + pc.beta * abs(C.y), pc.penalty_max);
        }
        if (abs(lambda.z) < friction_bound) {
            penalty.z = min(penalty.z + pc.beta * abs(C.z), pc.penalty_max);
        }
    }

    contact_data[ci].lambda = lambda;
    contact_data[ci].penalty = penalty;
}
