static const uint MAX_BODIES = 500;
static const uint MAX_CONTACTS = 2000;
static const uint MAX_COLLISION_PAIRS = 8192;

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
    uint n = pc.body_count;
    uint total_pairs = n * (n - 1) / 2;
    uint tid = dispatch_id.x;
    if (tid >= total_pairs) return;

    float fn = float(n);
    float ftid = float(tid);
    float term = 2.0f * fn - 1.0f;
    uint i = (uint)floor((term - sqrt(term * term - 8.0f * ftid)) * 0.5f);
    uint j = tid - i * (2 * n - i - 1) / 2 + i + 1;

    if (i >= n || j >= n || i >= j) return;

    float3 min_a = body_data[i].aabb_min.xyz;
    float3 max_a = body_data[i].aabb_max.xyz;
    float3 min_b = body_data[j].aabb_min.xyz;
    float3 max_b = body_data[j].aabb_max.xyz;

    bool overlap =
        min_a.x <= max_b.x && max_a.x >= min_b.x &&
        min_a.y <= max_b.y && max_a.y >= min_b.y &&
        min_a.z <= max_b.z && max_a.z >= min_b.z;

    if (!overlap) return;

    uint slot;
    InterlockedAdd(collision_pairs[0], 1, slot);

    if (slot >= MAX_COLLISION_PAIRS) return;

    collision_pairs[1 + slot * 2] = i;
    collision_pairs[1 + slot * 2 + 1] = j;
}
