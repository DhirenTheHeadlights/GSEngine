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

struct GpuMotor {
    uint body_index;
    float compliance;
    float max_force;
    uint horizontal_only;
    float4 target_velocity;
};

struct GpuWarmStart {
    uint body_a;
    uint body_b;
    uint feature_packed;
    float lambda;
};

static const uint FLAG_LOCKED = 1;
static const uint FLAG_UPDATE_ORIENTATION = 2;
static const uint FLAG_AFFECTED_BY_GRAVITY = 4;
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
[[vk::binding(2, 0)]] StructuredBuffer<GpuMotor> motor_data;
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

float4 quat_normalize(float4 q) {
    float len = length(q);
    return len > 1e-10 ? q / len : float4(1, 0, 0, 0);
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
    bool gravity = (body.flags & FLAG_AFFECTED_BY_GRAVITY) != 0;

    if (pc.substep == 0) {
        for (uint mi = 0; mi < pc.motor_count; mi++) {
            GpuMotor m = motor_data[mi];
            if (m.body_index == bi && sleeping && length(m.target_velocity.xyz) > 0.01) {
                body.sleep_counter = 0;
                sleeping = false;
            }
        }

        if (sleeping) {
            uint co = body_contact_map[bi];
            uint cc = body_contact_map[MAX_BODIES + bi];
            for (uint k = 0; k < cc; k++) {
                uint ci = body_contact_map[MAX_BODIES * 2 + co + k];
                GpuContact c = contact_data[ci];
                uint other = (c.body_a == bi) ? c.body_b : c.body_a;
                GpuBody other_body = body_data[other];
                bool other_locked = (other_body.flags & FLAG_LOCKED) != 0;
                bool other_sleeping = other_body.sleep_counter >= SLEEP_THRESHOLD;

                if (!other_locked && !other_sleeping) {
                    body.sleep_counter = 0;
                    sleeping = false;
                    break;
                }
            }
        }
    }

    bool bad_pos = isinf(body.position.x) || isinf(body.position.y) || isinf(body.position.z)
                || isnan(body.position.x) || isnan(body.position.y) || isnan(body.position.z);
    bool bad_vel = isinf(body.velocity.x) || isinf(body.velocity.y) || isinf(body.velocity.z)
                || isnan(body.velocity.x) || isnan(body.velocity.y) || isnan(body.velocity.z);
    bool bad_ang = isinf(body.angular_velocity.x) || isinf(body.angular_velocity.y) || isinf(body.angular_velocity.z)
                || isnan(body.angular_velocity.x) || isnan(body.angular_velocity.y) || isnan(body.angular_velocity.z);
    float q_len_sq = body.orientation.x * body.orientation.x + body.orientation.y * body.orientation.y
                   + body.orientation.z * body.orientation.z + body.orientation.w * body.orientation.w;
    bool bad_quat = q_len_sq < 0.5 || isnan(q_len_sq);

    if (bad_pos || bad_vel || bad_ang || bad_quat) {
        if (bad_pos) body.position = body.old_position;
        body.velocity = float4(0, 0, 0, 0);
        body.angular_velocity = float4(0, 0, 0, 0);
        if (bad_quat) body.orientation = float4(1, 0, 0, 0);
        body.predicted_position = body.position;
        body.inertia_target = body.position;
        body.motor_target = body.position;
        body.predicted_velocity = float4(0, 0, 0, 0);
        body.predicted_orientation = body.orientation;
        body.angular_inertia_target = body.orientation;
        body.predicted_angular_velocity = float4(0, 0, 0, 0);
        body.old_position = body.position;
        body.old_orientation = body.orientation;
        body_data[bi] = body;
        return;
    }

    body.old_position = body.position;
    body.old_orientation = body.orientation;

    if (locked || sleeping) {
        body.predicted_position = body.position;
        body.inertia_target = body.position;
        body.motor_target = body.position;
        body.predicted_velocity = float4(0, 0, 0, 0);
        body.predicted_orientation = body.orientation;
        body.angular_inertia_target = body.orientation;
        body.predicted_angular_velocity = float4(0, 0, 0, 0);
        body_data[bi] = body;
        return;
    }

    if (gravity) {
        body.predicted_velocity = body.velocity + float4(0, -9.8 * pc.dt, 0, 0);
    } else {
        body.predicted_velocity = body.velocity;
    }

    body.inertia_target = body.position + body.predicted_velocity * pc.dt;
    body.predicted_position = body.inertia_target;
    body.motor_target = body.inertia_target;

    if (update_orient) {
        body.predicted_angular_velocity = body.angular_velocity;
        float3 w = body.angular_velocity.xyz;
        float omega_mag = length(w);
        if (omega_mag > 1e-7) {
            float angle = omega_mag * pc.dt;
            float3 axis = w / omega_mag;
            float half_a = angle * 0.5;
            float sh = sin(half_a);
            float ch = cos(half_a);
            float4 dq = float4(ch, sh * axis.x, sh * axis.y, sh * axis.z);
            body.predicted_orientation = quat_normalize(quat_mul(dq, body.orientation));
        } else {
            body.predicted_orientation = body.orientation;
        }
    } else {
        body.predicted_orientation = body.orientation;
        body.predicted_angular_velocity = float4(0, 0, 0, 0);
    }

    body.angular_inertia_target = body.predicted_orientation;

    for (uint mi = 0; mi < pc.motor_count; mi++) {
        GpuMotor m = motor_data[mi];
        if (m.body_index != bi) continue;

        float4 target = body.old_position + m.target_velocity * pc.dt;
        if (m.horizontal_only != 0) {
            body.motor_target.x = target.x;
            body.motor_target.z = target.z;
        } else {
            body.motor_target = target;
        }
    }

    body_data[bi] = body;
}
