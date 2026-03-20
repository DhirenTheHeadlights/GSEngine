import vbd_shared;

[[vk::binding(0, 0)]] RWStructuredBuffer<GpuBody> body_data;
[[vk::binding(1, 0)]] RWStructuredBuffer<GpuContact> contact_data;
[[vk::binding(2, 0)]] StructuredBuffer<GpuMotor> motor_data;
[[vk::binding(3, 0)]] RWStructuredBuffer<uint> color_data;
[[vk::binding(4, 0)]] RWStructuredBuffer<uint> body_contact_map;
[[vk::binding(5, 0)]] RWStructuredBuffer<float4> solve_state;
[[vk::binding(6, 0)]] RWStructuredBuffer<uint> collision_pairs;
[[vk::binding(7, 0)]] RWStructuredBuffer<uint> collision_state;
[[vk::binding(8, 0)]] StructuredBuffer<GpuWarmStart> warm_starts;
[[vk::binding(9, 0)]] RWStructuredBuffer<GpuJoint> joint_data;

[[vk::push_constant]]
ConstantBuffer<vbd_push_constants> pc;

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
            float wake_thresh = pc.velocity_sleep_threshold * 10.0;
            uint co = body_contact_map[bi];
            uint cc = body_contact_map[MAX_BODIES + bi];
            for (uint k = 0; k < cc; k++) {
                uint ci = body_contact_map[MAX_BODIES * 2 + co + k];
                GpuContact c = contact_data[ci];
                uint other = (c.body_a == bi) ? c.body_b : c.body_a;
                GpuBody other_body = body_data[other];
                bool other_locked = (other_body.flags & FLAG_LOCKED) != 0;
                bool other_sleeping = other_body.sleep_counter >= SLEEP_THRESHOLD;
                float other_speed = length(other_body.velocity.xyz);

                if (!other_locked && !other_sleeping && other_speed > wake_thresh) {
                    body.sleep_counter = 0;
                    sleeping = false;
                    break;
                }
            }

            if (sleeping) {
                uint j_off = body_contact_map[JOINT_MAP_BASE + bi];
                uint jc = body_contact_map[JOINT_MAP_BASE + MAX_BODIES + bi];
                for (uint k = 0; k < jc; k++) {
                    uint ji = body_contact_map[JOINT_MAP_BASE + MAX_BODIES * 2 + j_off + k];
                    GpuJoint jnt = joint_data[ji];
                    uint other = (jnt.body_a == bi) ? jnt.body_b : jnt.body_a;
                    GpuBody other_body = body_data[other];
                    bool other_locked = (other_body.flags & FLAG_LOCKED) != 0;
                    bool other_sleeping = other_body.sleep_counter >= SLEEP_THRESHOLD;
                    float other_speed = length(other_body.velocity.xyz);

                    if (!other_locked && !other_sleeping && other_speed > wake_thresh) {
                        body.sleep_counter = 0;
                        sleeping = false;
                        break;
                    }
                }
            }
        }
    }

    body.old_position = body.position;
    body.old_orientation = body.orientation;

    if (locked || sleeping) {
        body.predicted_position = body.position;
        body.inertia_target = body.position;
        body.predicted_orientation = body.orientation;
        body.angular_inertia_target = body.orientation;
        body.motor_target = body.position;
        body.predicted_velocity = float4(0, 0, 0, 0);
        body.predicted_angular_velocity = float4(0, 0, 0, 0);
        body_data[bi] = body;
        return;
    }

    float3 angular_velocity = body.angular_velocity.xyz;
    float ang_speed = length(angular_velocity);
    if (ang_speed > 50.0) {
        angular_velocity *= (50.0 / ang_speed);
        body.angular_velocity = float4(angular_velocity, 0);
    }

    float4 gravity_step = gravity ? float4(0, -9.8 * pc.dt * pc.dt, 0, 0) : float4(0, 0, 0, 0);
    body.inertia_target = body.position + body.velocity * pc.dt + gravity_step;
    body.predicted_position = body.position + body.velocity * pc.dt + gravity_step * body.accel_weight;
    body.predicted_velocity = body.velocity;
    body.motor_target = body.inertia_target;

    if (update_orient) {
        body.predicted_angular_velocity = body.angular_velocity;
        float3 w = angular_velocity;
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
