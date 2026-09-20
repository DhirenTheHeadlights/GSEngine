
static const uint ELEMS_PER_THREAD = (max_bodies + adjacency_workgroup_size - 1) / adjacency_workgroup_size;
groupshared uint gs_scan_c[adjacency_workgroup_size];
groupshared uint gs_scan_j[adjacency_workgroup_size];

void collision_build_adjacency(uint3 dispatch_id, uint3 group_thread_id) {
    uint tid = group_thread_id.x;
uint body_count = pc.body_count;
uint num_contacts = min(collision_state[state_contact_count_index], max_contacts);
uint num_joints = min(pc.joint_count, max_joints);

[unroll]
for (uint k0 = 0; k0 < ELEMS_PER_THREAD; k0++) {
    uint bi = tid + k0 * adjacency_workgroup_size;
    if (bi < body_count) {
        contact_counts[bi] = 0;
        joint_counts[bi] = 0;
    }
}
for (uint gi = tid; gi < (body_count + 31u) / 32u; gi += adjacency_workgroup_size) {
    grounded_bits[gi] = 0;
}
if (tid == 0) unowned_contacts[0] = 0;
AllMemoryBarrierWithGroupSync();

for (uint ci = tid; ci < num_contacts; ci += adjacency_workgroup_size) {
    contact_constraint c = contact_data[ci];
    if (c.body_a < body_count && c.body_b < body_count) {
        uint dummy;
        InterlockedAdd(contact_counts[c.body_a], 1u, dummy);
        InterlockedAdd(contact_counts[c.body_b], 1u, dummy);
        if (c.normal.y > 0.7) {
            InterlockedOr(grounded_bits[c.body_a / 32u], 1u << (c.body_a % 32u), dummy);
        }
        if (c.normal.y < -0.7) {
            InterlockedOr(grounded_bits[c.body_b / 32u], 1u << (c.body_b % 32u), dummy);
        }
    }
}

for (uint ji = tid; ji < num_joints; ji += adjacency_workgroup_size) {
    joint_constraint j = joint_data[ji];
    if (j.body_a < body_count && j.body_b < body_count) {
        uint dummy;
        InterlockedAdd(joint_counts[j.body_a], 1u, dummy);
        InterlockedAdd(joint_counts[j.body_b], 1u, dummy);
    }
}
AllMemoryBarrierWithGroupSync();

uint lane_first = tid * ELEMS_PER_THREAD;
uint cv[ELEMS_PER_THREAD];
uint jv[ELEMS_PER_THREAD];
uint lane_c = 0;
uint lane_j = 0;
[unroll]
for (uint k1 = 0; k1 < ELEMS_PER_THREAD; k1++) {
    uint bi = lane_first + k1;
    bool valid = bi < body_count;
    uint c = valid ? contact_counts[bi] : 0u;
    uint j = valid ? joint_counts[bi] : 0u;
    cv[k1] = lane_c;
    jv[k1] = lane_j;
    lane_c += c;
    lane_j += j;
}
gs_scan_c[tid] = lane_c;
gs_scan_j[tid] = lane_j;
GroupMemoryBarrierWithGroupSync();

for (uint stride = 1; stride < adjacency_workgroup_size; stride <<= 1) {
    uint add_c = tid >= stride ? gs_scan_c[tid - stride] : 0u;
    uint add_j = tid >= stride ? gs_scan_j[tid - stride] : 0u;
    GroupMemoryBarrierWithGroupSync();
    gs_scan_c[tid] += add_c;
    gs_scan_j[tid] += add_j;
    GroupMemoryBarrierWithGroupSync();
}

uint base_c = tid > 0 ? gs_scan_c[tid - 1] : 0u;
uint base_j = tid > 0 ? gs_scan_j[tid - 1] : 0u;
[unroll]
for (uint k2 = 0; k2 < ELEMS_PER_THREAD; k2++) {
    uint bi = lane_first + k2;
    if (bi < body_count) {
        contact_offsets[bi] = min(base_c + cv[k2], max_contact_adjacency);
        joint_offsets[bi] = min(base_j + jv[k2], max_joint_adjacency);
        contact_counts[bi] = 0;
        joint_counts[bi] = 0;
    }
}
AllMemoryBarrierWithGroupSync();

for (uint ci = tid; ci < num_contacts; ci += adjacency_workgroup_size) {
    contact_constraint c = contact_data[ci];
    if (c.body_a < body_count && c.body_b < body_count) {
        uint slot_a;
        InterlockedAdd(contact_counts[c.body_a], 1u, slot_a);
        uint off_a = contact_offsets[c.body_a] + slot_a;
        if (off_a < max_contact_adjacency) {
            contact_adjacency[off_a] = ci;
        }

        uint slot_b;
        InterlockedAdd(contact_counts[c.body_b], 1u, slot_b);
        uint off_b = contact_offsets[c.body_b] + slot_b;
        if (off_b < max_contact_adjacency) {
            contact_adjacency[off_b] = ci;
        }
    }
}

for (uint ji = tid; ji < num_joints; ji += adjacency_workgroup_size) {
    joint_constraint j = joint_data[ji];
    if (j.body_a < body_count && j.body_b < body_count) {
        uint slot_a;
        InterlockedAdd(joint_counts[j.body_a], 1u, slot_a);
        uint off_a = joint_offsets[j.body_a] + slot_a;
        if (off_a < max_joint_adjacency) {
            joint_adjacency[off_a] = ji;
        }

        uint slot_b;
        InterlockedAdd(joint_counts[j.body_b], 1u, slot_b);
        uint off_b = joint_offsets[j.body_b] + slot_b;
        if (off_b < max_joint_adjacency) {
            joint_adjacency[off_b] = ji;
        }
    }
}
}
