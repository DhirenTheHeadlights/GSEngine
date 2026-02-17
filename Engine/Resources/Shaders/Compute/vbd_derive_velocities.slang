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
static const uint SLEEP_THRESHOLD = 300;
static const uint MAX_BODIES = 500;

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
        float wake_dist = length(body.predicted_position.xyz - body.position.xyz);
        if (wake_dist > 0.001) body.sleep_counter = 0;
        body_data[bi] = body;
        return;
    }

    float3 new_vel = (body.predicted_position.xyz - body.old_position.xyz) / pc.dt;
    if (any(isnan(new_vel)) || any(isinf(new_vel))) {
        body.velocity = float4(0, 0, 0, 0);
        body.predicted_position = body.old_position;
        body_data[bi] = body;
        return;
    }
    float linear_damping_factor = max(0.0, 1.0 - pc.linear_damping * pc.dt);
    new_vel *= linear_damping_factor;
    static const float MAX_SPEED = 100.0;
    float speed = length(new_vel);
    if (speed > MAX_SPEED) new_vel *= (MAX_SPEED / speed);
    body.velocity = float4(new_vel, 0);

    float lin_speed = length(new_vel);
    bool lin_at_rest = lin_speed < pc.velocity_sleep_threshold;

    bool ang_at_rest = true;
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

        if (any(isnan(ang_vel)) || any(isinf(ang_vel))) ang_vel = float3(0, 0, 0);
        float angular_damping = 1.0;
        float angular_damping_factor = max(0.0, 1.0 - angular_damping * pc.dt);
        ang_vel *= angular_damping_factor;
        float ang_spd = length(ang_vel);
        if (ang_spd > 50.0) ang_vel *= (50.0 / ang_spd);
        body.angular_velocity = float4(ang_vel, 0);

        float ang_speed = length(ang_vel);
        ang_at_rest = ang_speed < pc.velocity_sleep_threshold;
    } else {
        body.angular_velocity = float4(0, 0, 0, 0);
    }

    bool has_contact = body_contact_map[MAX_BODIES + bi] > 0;
    if (lin_at_rest && ang_at_rest && has_contact) {
        body.sleep_counter++;
    } else if (!lin_at_rest || !ang_at_rest) {
        body.sleep_counter = 0;
    }

    body_data[bi] = body;
}
