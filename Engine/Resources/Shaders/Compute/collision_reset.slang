static const uint MAX_BODIES = 500;
static const uint MAX_CONTACTS = 2000;

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

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint bi = dispatch_id.x;

    if (bi == 0) {
        collision_pairs[0] = 0;
        collision_state[0] = 0;
        collision_state[1] = 0;
    }

    if (bi < pc.body_count) {
        body_contact_map[bi] = 0;
        body_contact_map[MAX_BODIES + bi] = 0;
    }
}
