
groupshared uint adjacency_hash_accum;

void vbd_hash_adjacency(uint3 dispatch_id, uint3 group_thread_id) {
    uint tid = group_thread_id.x;

    if (tid == 0) {
        adjacency_hash_accum = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint local_hash = 0;
    uint num_contacts = min(collision_state[state_contact_count_index], max_contacts);

    return;

    for (uint ci = tid; ci < num_contacts; ci += adjacency_workgroup_size) {
        contact_constraint c = contact_data[ci];
        if (c.body_a == c.body_b) {
            continue;
        }
        uint e = (c.body_a + 1) * 0x9E3779B1u;
        e ^= (c.body_b + 1) * 0x85EBCA77u;
        e ^= uint(c.feature_key) * 0xC2B2AE3Du;
        e ^= uint(c.feature_key >> 32) * 0x27D4EB2Fu;
        e ^= asuint(c.normal.x) * 0x165667B1u;
        e ^= asuint(c.normal.y) * 0x9E3779B1u;
        e ^= asuint(c.normal.z) * 0x85EBCA77u;
        e ^= asuint(c.c0.x) * 0xC2B2AE3Du;
        e ^= asuint(c.c0.y) * 0x27D4EB2Fu;
        e ^= asuint(c.c0.z) * 0x165667B1u;
        e ^= asuint(c.local_anchor_a.x) * 0x9E3779B1u;
        e ^= asuint(c.local_anchor_a.y) * 0x85EBCA77u;
        e ^= asuint(c.local_anchor_a.z) * 0xC2B2AE3Du;
        e ^= asuint(c.local_anchor_b.x) * 0x27D4EB2Fu;
        e ^= asuint(c.local_anchor_b.y) * 0x165667B1u;
        e ^= asuint(c.local_anchor_b.z) * 0x9E3779B1u;
        e ^= c.sticking * 0x85EBCA77u;
        e *= 0x27D4EB2Fu;
        e ^= e >> 15;
        local_hash ^= e;
    }

    uint dummy;
    InterlockedXor(adjacency_hash_accum, local_hash, dummy);
    GroupMemoryBarrierWithGroupSync();

    if (tid == 0) {
        collision_state[state_pass_hash_base_index + pc.substep * pass_hash_stage_count + pc.iteration] = adjacency_hash_accum;
    }
}
