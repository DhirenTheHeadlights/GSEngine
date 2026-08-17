
groupshared uint hash_accum;

uint chash_quat(uint h, float4 q) {
    h = chash_step(h, chash_bits(q.x));
    h = chash_step(h, chash_bits(q.y));
    h = chash_step(h, chash_bits(q.z));
    h = chash_step(h, chash_bits(q.w));
    return h;
}

void vbd_hash_state(uint3 dispatch_id, uint3 group_thread_id) {
    uint tid = group_thread_id.x;

    if (tid == 0) {
        hash_accum = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint local_hash = 0;

    for (uint bi = tid; bi < pc.body_count; bi += adjacency_workgroup_size) {
        body_state b = body_data[bi];
        uint h = chash_step(0x9e3779b9u, bi);

        h = chash_vec3(h, b.position.xyz);
        h = chash_vec3(h, b.predicted_position.xyz);
        h = chash_vec3(h, b.inertia_target.xyz);
        h = chash_vec3(h, b.old_position.xyz);
        h = chash_vec3(h, b.velocity.xyz);
        h = chash_vec3(h, b.angular_velocity.xyz);
        h = chash_vec3(h, b.motor_target.xyz);
        h = chash_vec3(h, b.com_local.xyz);
        h = chash_vec3(h, b.shape_params.xyz);
        h = chash_vec3(h, b.half_extents.xyz);
        h = chash_vec3(h, b.aabb_min.xyz);
        h = chash_vec3(h, b.aabb_max.xyz);

        h = chash_quat(h, b.orientation);
        h = chash_quat(h, b.predicted_orientation);
        h = chash_quat(h, b.angular_inertia_target);
        h = chash_quat(h, b.old_orientation);

        for (uint r = 0; r < 3; r++) {
            for (uint c = 0; c < 3; c++) {
                h = chash_step(h, chash_bits(b.inv_inertia[r][c]));
            }
        }

        h = chash_step(h, chash_bits(b.mass));
        h = chash_step(h, chash_bits(b.accel_weight));
        h = chash_step(h, chash_bits(b.restitution));
        h = chash_step(h, b.locked);
        h = chash_step(h, b.update_orientation);
        h = chash_step(h, b.affected_by_gravity);
        h = chash_step(h, b.sleep_counter);
        h = chash_step(h, b.shape_kind);
        h = chash_step(h, b.reset_pending);

        local_hash += h;
    }

    uint dummy;
    InterlockedAdd(hash_accum, local_hash, dummy);
    GroupMemoryBarrierWithGroupSync();

    if (tid == 0) {
        collision_state[state_pass_hash_base_index + pc.substep * pass_hash_stage_count + pc.iteration] = hash_accum;
    }
}
