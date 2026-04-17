import common;
import vbd_shared;
import vbd_physics;

[[vk::push_constant]]
ConstantBuffer<vbd_push_constants> pc;

[Layout(LayoutKind::VBDPhysics)]
[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint bi = dispatch_id.x;
    if (bi >= pc.body_count) return;
    if (collision_state[CONVERGED_FLAG_INDEX] != 0) return;

    GpuBody body = body_data[bi];
    if ((body.flags & FLAG_LOCKED) != 0) return;
    if (body.sleep_counter >= SLEEP_THRESHOLD) return;

    float3 delta_x = solve_deltas[bi * 2].xyz;
    float3 delta_theta = solve_deltas[bi * 2 + 1].xyz;

    body.predicted_position.xyz += delta_x;

    float angle = length(delta_theta);
    if (angle > 1e-7) {
        float3 axis = delta_theta / angle;
        float half_angle = angle * 0.5;
        float sh = sin(half_angle);
        float ch = cos(half_angle);
        float4 delta_q = float4(ch, sh * axis.x, sh * axis.y, sh * axis.z);
        body.predicted_orientation = quat_normalize(quat_mul(delta_q, body.predicted_orientation));
    }

    body_data[bi] = body;
}
