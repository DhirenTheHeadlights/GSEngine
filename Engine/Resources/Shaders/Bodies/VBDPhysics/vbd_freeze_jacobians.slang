
void vbd_freeze_jacobians(uint3 dispatch_id) {
    uint ci = dispatch_id.x;
    uint actual_count = collision_state[state_contact_count_index];
    if (ci >= actual_count) return;

    contact_constraint c = contact_data[ci];
    if (c.body_a >= pc.body_count || c.body_b >= pc.body_count) return;

    body_state a = body_data[c.body_a];
    body_state b = body_data[c.body_b];

    float3 r_aw = rotate_vector(a.predicted_orientation, c.local_anchor_a.xyz);
    float3 r_bw = rotate_vector(b.predicted_orientation, c.local_anchor_b.xyz);

    float3 dirs[3] = { c.normal.xyz, c.tangent_u.xyz, c.tangent_v.xyz };

    frozen_jacobian fj;
    fj.world_r_a = r_aw;
    fj.world_r_b = r_bw;
    fj.j_ang_a = float3x3(
        cross(r_aw, dirs[0]),
        cross(r_aw, dirs[1]),
        cross(r_aw, dirs[2])
    );
    fj.j_ang_b = float3x3(
        cross(r_bw, dirs[0]),
        cross(r_bw, dirs[1]),
        cross(r_bw, dirs[2])
    );

    frozen_jacobians[ci] = fj;
}
