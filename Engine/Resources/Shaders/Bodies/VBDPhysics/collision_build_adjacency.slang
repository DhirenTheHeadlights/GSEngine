
static const uint ELEMS_PER_THREAD = (max_bodies + adjacency_workgroup_size - 1) / adjacency_workgroup_size;

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

[unroll]
for (uint k1 = 0; k1 < ELEMS_PER_THREAD; k1++) {
    uint bi = tid + k1 * adjacency_workgroup_size;
    if (bi < body_count) {
        contact_offsets[bi] = contact_counts[bi];
        joint_offsets[bi] = joint_counts[bi];
    }
}
AllMemoryBarrierWithGroupSync();

for (uint stride = 1; stride < body_count; stride <<= 1) {
    uint cv[ELEMS_PER_THREAD];
    uint jv[ELEMS_PER_THREAD];
    [unroll]
    for (uint kr = 0; kr < ELEMS_PER_THREAD; kr++) {
        uint bi = tid + kr * adjacency_workgroup_size;
        bool valid = bi < body_count && bi >= stride;
        cv[kr] = valid ? contact_offsets[bi - stride] : 0u;
        jv[kr] = valid ? joint_offsets[bi - stride] : 0u;
    }
    AllMemoryBarrierWithGroupSync();
    [unroll]
    for (uint kw = 0; kw < ELEMS_PER_THREAD; kw++) {
        uint bi = tid + kw * adjacency_workgroup_size;
        if (bi < body_count) {
            contact_offsets[bi] += cv[kw];
            joint_offsets[bi] += jv[kw];
        }
    }
    AllMemoryBarrierWithGroupSync();
}

[unroll]
for (uint k2 = 0; k2 < ELEMS_PER_THREAD; k2++) {
    uint bi = tid + k2 * adjacency_workgroup_size;
    if (bi < body_count) {
        uint cs = contact_offsets[bi] - contact_counts[bi];
        uint js = joint_offsets[bi] - joint_counts[bi];
        contact_offsets[bi] = min(cs, max_contact_adjacency);
        joint_offsets[bi] = min(js, max_joint_adjacency);
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
