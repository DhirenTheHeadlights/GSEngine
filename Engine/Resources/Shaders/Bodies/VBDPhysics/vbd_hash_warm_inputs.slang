
groupshared uint warm_hash_accum;

void vbd_hash_warm_inputs(uint3 dispatch_id, uint3 group_thread_id) {
    uint tid = group_thread_id.x;

    if (tid == 0) {
        warm_hash_accum = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint local_hash = 0;

    for (uint bi = tid; bi < pc.body_count; bi += adjacency_workgroup_size) {
        uint count = min(contact_counts[bi], max_contact_adjacency);
        uint offset = contact_offsets[bi];
        local_hash ^= (bi + 1) * 0x9E3779B1u ^ count * 0x85EBCA77u;
        for (uint k = 0; k < count; k++) {
            uint slot = offset + k;
            if (slot >= max_contact_adjacency) {
                break;
            }
            uint ci = contact_adjacency[slot];
            if (ci >= max_contacts) {
                continue;
            }
            contact_constraint w = warm_starts[ci];
            uint e = (w.body_a + 1) * 0x9E3779B1u;
            e ^= (w.body_b + 1) * 0x85EBCA77u;
            e ^= uint(w.feature_key) * 0xC2B2AE3Du;
            e ^= uint(w.feature_key >> 32) * 0x27D4EB2Fu;
            e ^= asuint(w.lambda.x) * 0x165667B1u;
            e ^= asuint(w.lambda.y) * 0x9E3779B1u;
            e ^= asuint(w.lambda.z) * 0x85EBCA77u;
            e ^= asuint(w.penalty.x) * 0xC2B2AE3Du;
            e ^= asuint(w.penalty.y) * 0x27D4EB2Fu;
            e ^= asuint(w.penalty.z) * 0x165667B1u;
            e ^= asuint(w.local_anchor_a.x) * 0x9E3779B1u;
            e ^= asuint(w.local_anchor_a.y) * 0x85EBCA77u;
            e ^= asuint(w.local_anchor_a.z) * 0xC2B2AE3Du;
            e ^= asuint(w.local_anchor_b.x) * 0x27D4EB2Fu;
            e ^= asuint(w.local_anchor_b.y) * 0x165667B1u;
            e ^= asuint(w.local_anchor_b.z) * 0x9E3779B1u;
            e ^= w.sticking * 0x85EBCA77u;
            local_hash ^= (bi + 1) * 0xC2B2AE3Du ^ e;
        }
    }

    uint dummy;
    InterlockedXor(warm_hash_accum, local_hash, dummy);
    GroupMemoryBarrierWithGroupSync();

    if (tid == 0) {
        collision_state[state_pass_hash_base_index + pc.substep * pass_hash_stage_count + pc.iteration] = warm_hash_accum;
    }
}
