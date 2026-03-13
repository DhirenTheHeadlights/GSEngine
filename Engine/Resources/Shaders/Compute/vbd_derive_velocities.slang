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

float4 quat_mul(float4 a, float4 b) {
    return float4(
        a.x * b.x - a.y * b.y - a.z * b.z - a.w * b.w,
        a.x * b.y + a.y * b.x + a.z * b.w - a.w * b.z,
        a.x * b.z - a.y * b.w + a.z * b.x + a.w * b.y,
        a.x * b.w + a.y * b.z - a.z * b.y + a.w * b.x
    );
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

float3 mat3_mul_vec(mat3 m, float3 v) {
    return m[0] * v.x + m[1] * v.y + m[2] * v.z;
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

    if (locked) {
        body.velocity = float4(0, 0, 0, 0);
        body.angular_velocity = float4(0, 0, 0, 0);
        body_data[bi] = body;
        return;
    }

    if (sleeping) {
        body.velocity = float4(0, 0, 0, 0);
        body.angular_velocity = float4(0, 0, 0, 0);
        body_data[bi] = body;
        return;
    }

    float3 new_vel = (body.predicted_position.xyz - body.old_position.xyz) / pc.dt;
    body.velocity = float4(new_vel, 0);

    if (update_orient) {
        float4 delta_q = quat_mul(body.predicted_orientation, quat_conjugate(body.old_orientation));
        float q_s = delta_q.x;
        float3 q_v = delta_q.yzw;
        if (q_s < 0.0) { q_s = -q_s; q_v = -q_v; }

        float3 ang_vel;
        if (q_s < 0.9999) {
            float angle = 2.0 * acos(clamp(q_s, 0.0, 1.0));
            float sin_half = sqrt(1.0 - q_s * q_s);
            if (sin_half > 1e-6) {
                ang_vel = (q_v / sin_half) * (angle / pc.dt);
            } else {
                ang_vel = q_v * (2.0 / pc.dt);
            }
        } else {
            ang_vel = q_v * (2.0 / pc.dt);
        }

        body.angular_velocity = float4(ang_vel, 0);
    } else {
        body.angular_velocity = float4(0, 0, 0, 0);
    }

    body_data[bi] = body;
}
