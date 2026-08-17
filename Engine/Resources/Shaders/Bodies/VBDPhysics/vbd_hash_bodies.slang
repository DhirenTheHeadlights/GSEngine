
groupshared uint bodies_hash_accum;

void vbd_hash_bodies(uint3 dispatch_id, uint3 group_thread_id) {
    uint tid = group_thread_id.x;

    if (tid == 0) {
        bodies_hash_accum = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint local_hash = 0;

    for (uint bi = tid; bi < pc.body_count; bi += adjacency_workgroup_size) {
        body_state b = body_data[bi];
        uint h = asuint(b.position.x) * 0x9E3779B1u;
        h ^= asuint(b.position.y) * 0x85EBCA77u;
        h ^= asuint(b.position.z) * 0xC2B2AE3Du;
        h ^= asuint(b.predicted_position.x) * 0x27D4EB2Fu;
        h ^= asuint(b.predicted_position.y) * 0x165667B1u;
        h ^= asuint(b.predicted_position.z) * 0x9E3779B1u;
        h ^= asuint(b.velocity.x) * 0xC2B2AE3Du;
        h ^= asuint(b.velocity.y) * 0x27D4EB2Fu;
        h ^= asuint(b.velocity.z) * 0x165667B1u;
        h ^= asuint(b.orientation.x) * 0x9E3779B1u;
        h ^= asuint(b.orientation.y) * 0x85EBCA77u;
        h ^= asuint(b.orientation.z) * 0xC2B2AE3Du;
        h ^= asuint(b.orientation.w) * 0x27D4EB2Fu;
        h ^= asuint(b.angular_velocity.x) * 0x165667B1u;
        h ^= asuint(b.angular_velocity.y) * 0x9E3779B1u;
        h ^= asuint(b.angular_velocity.z) * 0x85EBCA77u;
        h ^= asuint(b.aabb_min.x) * 0xC2B2AE3Du;
        h ^= asuint(b.aabb_min.y) * 0x27D4EB2Fu;
        h ^= asuint(b.aabb_min.z) * 0x165667B1u;
        h ^= asuint(b.aabb_max.x) * 0x9E3779B1u;
        h ^= asuint(b.aabb_max.y) * 0x85EBCA77u;
        h ^= asuint(b.aabb_max.z) * 0xC2B2AE3Du;
        h ^= (bi + 1) * 0x85EBCA77u;
        local_hash ^= h;
    }

    uint dummy;
    InterlockedXor(bodies_hash_accum, local_hash, dummy);
    GroupMemoryBarrierWithGroupSync();

    if (tid == 0) {
        collision_state[state_pass_hash_base_index + pc.substep * pass_hash_stage_count + pc.iteration] = bodies_hash_accum;
    }
}
