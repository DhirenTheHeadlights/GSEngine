import common;
import vbd_shared;
import vbd_physics;

[[vk::push_constant]]
ConstantBuffer<vbd_push_constants> pc;

[Layout(LayoutKind::VBDPhysics)]
[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint ci = dispatch_id.x;
    uint actual_count = collision_state[0];
    if (ci >= actual_count) return;

    GpuContact c = contact_data[ci];
    if (c.body_a >= pc.body_count || c.body_b >= pc.body_count) return;

    GpuBody a = body_data[c.body_a];
    GpuBody b = body_data[c.body_b];

    float3 r_aw = rotate_vector(a.predicted_orientation, c.r_a.xyz);
    float3 r_bw = rotate_vector(b.predicted_orientation, c.r_b.xyz);

    float3 dirs[3] = { c.normal.xyz, c.tangent_u.xyz, c.tangent_v.xyz };

    FrozenJacobian fj;
    fj.world_r_a = float4(r_aw, 0);
    fj.world_r_b = float4(r_bw, 0);
    fj.J_ang_a = float3x3(
        cross(r_aw, dirs[0]),
        cross(r_aw, dirs[1]),
        cross(r_aw, dirs[2])
    );
    fj.J_ang_b = float3x3(
        cross(r_bw, dirs[0]),
        cross(r_bw, dirs[1]),
        cross(r_bw, dirs[2])
    );

    frozen_jacobians[ci] = fj;
}
