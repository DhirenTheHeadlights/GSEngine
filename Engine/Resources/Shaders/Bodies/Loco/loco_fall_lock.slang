void loco_fall_lock(uint3 dispatch_id) {
    uint e = dispatch_id.x;
    if (e >= pc.env_count) return;

    loco_muscle_row row = loco_muscle_table_data[e];
    if (row.driving == 0) return;

    loco_env_desc env = loco_env_data[e];
    body_state pelvis = loco_body_lock_data[env.pelvis];
    float3 pelvis_pos = body_origin(pelvis.position.xyz, pelvis.orientation, pelvis.com_local.xyz);
    float upright = loco_upright_of(loco_framed_by(pelvis.orientation, env.framed, env.segment_frame));
    if (!loco_fallen(pelvis_pos.y, upright, pc.fall_height, pc.fall_upright)) return;

    for (uint b = 0; b < loco_muscle_body_count; ++b) {
        uint bi = loco_lock_body_data[e * loco_muscle_body_count + b];
        loco_body_lock_data[bi].locked = 1;
        loco_body_lock_data[bi].velocity = float3(0, 0, 0);
        loco_body_lock_data[bi].angular_velocity = float3(0, 0, 0);
    }
}
