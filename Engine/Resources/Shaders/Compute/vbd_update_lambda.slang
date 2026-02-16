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

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint ci = dispatch_id.x;
    if (ci >= pc.contact_count) return;

    GpuContact c = contact_data[ci];
    GpuBody body_a = body_data[c.body_a];
    GpuBody body_b = body_data[c.body_b];

    float3 world_r_a = rotate_vector(body_a.predicted_orientation, c.r_a.xyz);
    float3 world_r_b = rotate_vector(body_b.predicted_orientation, c.r_b.xyz);

    float3 p_a = body_a.predicted_position.xyz + world_r_a;
    float3 p_b = body_b.predicted_position.xyz + world_r_b;

    float gap = dot(p_b - p_a, c.normal.xyz) + c.initial_separation;

    float lambda_mass = max(c.mass_a, c.mass_b);
    float rho_val = pc.rho * (lambda_mass / pc.h_squared);
    float new_lambda = clamp(c.lambda - rho_val * gap, 0.0, 1e6);

    contact_data[ci].lambda = new_lambda;
}
