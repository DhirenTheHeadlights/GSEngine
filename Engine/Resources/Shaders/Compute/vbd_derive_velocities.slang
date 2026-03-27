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

    float lin_speed = length(body.velocity.xyz);
    float ang_speed = length(body.angular_velocity.xyz);
    if (lin_speed < pc.velocity_sleep_threshold && ang_speed < pc.angular_sleep_threshold) {
        body.sleep_counter = body.sleep_counter + 1;
    } else {
        body.sleep_counter = body.sleep_counter / 2;
    }

    body_data[bi] = body;
}
