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

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint bi = dispatch_id.x;
    if (bi >= pc.body_count) return;

    GpuBody body = body_data[bi];
    bool locked = (body.flags & FLAG_LOCKED) != 0;
    bool sleeping = body.sleep_counter >= SLEEP_THRESHOLD;
    bool update_orient = (body.flags & FLAG_UPDATE_ORIENTATION) != 0;

    if (locked || sleeping) return;

    bool bad_pp = isinf(body.predicted_position.x) || isinf(body.predicted_position.y) || isinf(body.predicted_position.z)
               || isnan(body.predicted_position.x) || isnan(body.predicted_position.y) || isnan(body.predicted_position.z);
    if (bad_pp) {
        body.predicted_position = body.old_position;
        body.predicted_orientation = body.old_orientation;
        body.velocity = float4(0, 0, 0, 0);
        body.angular_velocity = float4(0, 0, 0, 0);
        body.position = body.old_position;
        body.orientation = body.old_orientation;
        body_data[bi] = body;
        return;
    }

    float q_len_sq = body.predicted_orientation.x * body.predicted_orientation.x
                   + body.predicted_orientation.y * body.predicted_orientation.y
                   + body.predicted_orientation.z * body.predicted_orientation.z
                   + body.predicted_orientation.w * body.predicted_orientation.w;
    if (q_len_sq < 0.5 || isnan(q_len_sq)) {
        body.predicted_orientation = body.orientation;
        body.angular_velocity = float4(0, 0, 0, 0);
    }

    float lin_spd = length(body.velocity.xyz);
    if (lin_spd > 30.0) body.velocity = body.velocity * (30.0 / lin_spd);

    if (update_orient) {
        float ang_spd = length(body.angular_velocity.xyz);
        if (ang_spd > 20.0) body.angular_velocity = body.angular_velocity * (20.0 / ang_spd);
    }

    body.position = body.predicted_position;
    if (update_orient) {
        body.orientation = body.predicted_orientation;
    }

    float3 he = body.half_extents.xyz;
    if (dot(he, he) > 0.0) {
        float4 q = body.orientation;
        float3 ax = rotate_vector(q, float3(1, 0, 0));
        float3 ay = rotate_vector(q, float3(0, 1, 0));
        float3 az = rotate_vector(q, float3(0, 0, 1));

        float3 aabb_he;
        aabb_he.x = abs(ax.x) * he.x + abs(ay.x) * he.y + abs(az.x) * he.z;
        aabb_he.y = abs(ax.y) * he.x + abs(ay.y) * he.y + abs(az.y) * he.z;
        aabb_he.z = abs(ax.z) * he.x + abs(ay.z) * he.y + abs(az.z) * he.z;

        body.aabb_min = float4(body.position.xyz - aabb_he, 0);
        body.aabb_max = float4(body.position.xyz + aabb_he, 0);
    }

    body_data[bi] = body;
}
