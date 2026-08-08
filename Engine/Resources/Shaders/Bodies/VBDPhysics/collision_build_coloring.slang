
static const uint ELEMS_PER_THREAD = (max_bodies + adjacency_workgroup_size - 1) / adjacency_workgroup_size;

groupshared uint gs_color_count[max_colors];
groupshared uint gs_color_offset[max_colors];
groupshared uint gs_color_fill[max_colors];
groupshared uint gs_max_contact_color;
groupshared uint gs_used_motor_only;
groupshared uint gs_pending;
groupshared uint gs_max_used_color;
groupshared uint gs_coloring_fallback;
groupshared uint gs_coloring_conflicts;

uint body_priority(uint bi) {
    uint state = bi * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

void collision_build_coloring(uint3 dispatch_id, uint3 group_thread_id) {
    uint tid = group_thread_id.x;
uint body_count = pc.body_count;
uint num_contacts = min(collision_state[state_contact_count_index], max_contacts);
uint num_joints = min(pc.joint_count, max_joints);

[unroll]
for (uint k0 = 0; k0 < ELEMS_PER_THREAD; k0++) {
    uint bi = tid + k0 * adjacency_workgroup_size;
    if (bi < max_bodies) {
        coloring_scratch[bi] = 0xFFFFFFFF;
        coloring_scratch[max_bodies + bi] = 0xFFFFFFFF;
    }
}
if (tid < max_colors) {
    gs_color_count[tid] = 0;
    gs_color_offset[tid] = 0;
    gs_color_fill[tid] = 0;
}
if (tid == 0) {
    gs_max_contact_color = 0;
    gs_used_motor_only = 0;
    gs_max_used_color = 0;
    gs_coloring_fallback = 0;
    gs_coloring_conflicts = 0;
}
AllMemoryBarrierWithGroupSync();

for (uint round = 0; round < coloring_rounds; round++) {
    if (tid == 0) {
        gs_pending = 0;
    }
    AllMemoryBarrierWithGroupSync();

    [unroll]
    for (uint kr = 0; kr < ELEMS_PER_THREAD; kr++) {
        uint bi = tid + kr * adjacency_workgroup_size;
        if (bi >= body_count) continue;

        uint current = coloring_scratch[bi];
        if (current != 0xFFFFFFFF) {
            coloring_scratch[max_bodies + bi] = current;
            continue;
        }

        if (body_data[bi].locked != 0) {
            coloring_scratch[max_bodies + bi] = 0xFFFFFFFF;
            continue;
        }

        uint cc = contact_counts[bi];
        uint jc = joint_counts[bi];
        if (cc == 0 && jc == 0) {
            coloring_scratch[max_bodies + bi] = 0xFFFFFFFF;
            continue;
        }

        uint my_priority = body_priority(bi);
        bool blocked = false;
        uint neighbor_colors = 0;

        uint off = contact_offsets[bi];
        for (uint k = 0; k < cc; k++) {
            uint idx = off + k;
            if (idx >= max_contact_adjacency) break;
            uint ci = contact_adjacency[idx];
            if (ci >= num_contacts) continue;
            contact_constraint c = contact_data[ci];
            uint other = (c.body_a == bi) ? c.body_b : c.body_a;
            if (other < body_count) {
                if (body_data[other].locked != 0) continue;
                uint other_color = coloring_scratch[other];
                if (other_color == 0xFFFFFFFF) {
                    uint other_priority = body_priority(other);
                    if (other_priority > my_priority ||
                        (other_priority == my_priority && other < bi)) {
                        blocked = true;
                    }
                } else if (other_color < max_colors) {
                    neighbor_colors |= (1u << other_color);
                }
            }
        }

        uint j_off = joint_offsets[bi];
        for (uint k = 0; k < jc; k++) {
            uint idx = j_off + k;
            if (idx >= max_joint_adjacency) break;
            uint ji = joint_adjacency[idx];
            if (ji >= num_joints) continue;
            joint_constraint j = joint_data[ji];
            uint other = (j.body_a == bi) ? j.body_b : j.body_a;
            if (other < body_count) {
                if (body_data[other].locked != 0) continue;
                uint other_color = coloring_scratch[other];
                if (other_color == 0xFFFFFFFF) {
                    uint other_priority = body_priority(other);
                    if (other_priority > my_priority ||
                        (other_priority == my_priority && other < bi)) {
                        blocked = true;
                    }
                } else if (other_color < max_colors) {
                    neighbor_colors |= (1u << other_color);
                }
            }
        }

        if (blocked) {
            uint dummy;
            InterlockedAdd(gs_pending, 1u, dummy);
            coloring_scratch[max_bodies + bi] = 0xFFFFFFFF;
            continue;
        }

        uint chosen = 0;
        while (chosen < max_colors && (neighbor_colors & (1u << chosen)) != 0) {
            chosen++;
        }

        coloring_scratch[max_bodies + bi] = (chosen < max_colors) ? chosen : 0xFFFFFFFF;
    }

    AllMemoryBarrierWithGroupSync();

    [unroll]
    for (uint kw = 0; kw < ELEMS_PER_THREAD; kw++) {
        uint bi = tid + kw * adjacency_workgroup_size;
        if (bi < max_bodies) {
            coloring_scratch[bi] = coloring_scratch[max_bodies + bi];
        }
    }

    AllMemoryBarrierWithGroupSync();

    if (gs_pending == 0) break;
}

[unroll]
for (uint km = 0; km < ELEMS_PER_THREAD; km++) {
    uint bi = tid + km * adjacency_workgroup_size;
    if (bi >= body_count) continue;
    uint c = coloring_scratch[bi];
    if (c < max_colors) {
        InterlockedMax(gs_max_contact_color, c + 1);
    }
}
AllMemoryBarrierWithGroupSync();

[unroll]
for (uint kp1 = 0; kp1 < ELEMS_PER_THREAD; kp1++) {
    uint bi = tid + kp1 * adjacency_workgroup_size;
    if (bi >= body_count) continue;
    if (body_data[bi].locked != 0) continue;
    uint cc = contact_counts[bi];
    uint jc = joint_counts[bi];
    bool has_motor = motor_map[bi] != 0xFFFFFFFF;
    if (cc == 0 && !has_motor && jc == 0) continue;
    if (coloring_scratch[bi] == 0xFFFFFFFF) {
        uint motor_only = (gs_max_contact_color < max_colors) ? gs_max_contact_color : 0u;
        coloring_scratch[bi] = motor_only;
        uint dummy;
        InterlockedOr(gs_used_motor_only, 1u, dummy);
        if (cc > 0) {
            InterlockedAdd(gs_coloring_fallback, 1u, dummy);
        }
    }
}
AllMemoryBarrierWithGroupSync();

uint motor_only_color = (gs_max_contact_color < max_colors) ? gs_max_contact_color : 0u;
uint free_body_color =
    (gs_used_motor_only != 0 && motor_only_color + 1 < max_colors)
        ? (motor_only_color + 1)
        : motor_only_color;

[unroll]
for (uint kp2 = 0; kp2 < ELEMS_PER_THREAD; kp2++) {
    uint bi = tid + kp2 * adjacency_workgroup_size;
    if (bi >= body_count) continue;
    if (body_data[bi].locked != 0) continue;
    uint cc = contact_counts[bi];
    uint jc = joint_counts[bi];
    bool has_motor = motor_map[bi] != 0xFFFFFFFF;
    if (cc != 0 || has_motor || jc != 0) continue;
    if (coloring_scratch[bi] == 0xFFFFFFFF) {
        coloring_scratch[bi] = free_body_color;
    }
}
AllMemoryBarrierWithGroupSync();

for (uint cv = tid; cv < num_contacts; cv += adjacency_workgroup_size) {
    contact_constraint c = contact_data[cv];
    if (c.body_a >= body_count || c.body_b >= body_count) continue;
    if (body_data[c.body_a].locked != 0 || body_data[c.body_b].locked != 0) continue;
    uint color_a = coloring_scratch[c.body_a];
    uint color_b = coloring_scratch[c.body_b];
    if (color_a == color_b && color_a != 0xFFFFFFFF) {
        uint dummy;
        InterlockedAdd(gs_coloring_conflicts, 1u, dummy);
    }
}
AllMemoryBarrierWithGroupSync();

[unroll]
for (uint kc = 0; kc < ELEMS_PER_THREAD; kc++) {
    uint bi = tid + kc * adjacency_workgroup_size;
    if (bi >= body_count) continue;
    uint c = coloring_scratch[bi];
    if (c < max_colors) {
        uint dummy;
        InterlockedAdd(gs_color_count[c], 1u, dummy);
        InterlockedMax(gs_max_used_color, c + 1);
    }
}
AllMemoryBarrierWithGroupSync();

if (tid == 0) {
    uint running = 0;
    for (uint c = 0; c < max_colors; c++) {
        gs_color_offset[c] = running;
        running += gs_color_count[c];
    }
    collision_state[state_max_used_color_index] = gs_max_used_color;
    collision_state[state_coloring_fallback_index] = gs_coloring_fallback;
    collision_state[state_coloring_conflict_index] = gs_coloring_conflicts;
}
AllMemoryBarrierWithGroupSync();

[unroll]
for (uint ks = 0; ks < ELEMS_PER_THREAD; ks++) {
    uint bi = tid + ks * adjacency_workgroup_size;
    if (bi >= body_count) continue;
    uint c = coloring_scratch[bi];
    if (c < max_colors) {
        uint slot;
        InterlockedAdd(gs_color_fill[c], 1u, slot);
        uint pos = gs_color_offset[c] + slot;
        if (pos < max_bodies) {
            color_data[max_colors * 2 + pos] = bi;
        }
    }
}
AllMemoryBarrierWithGroupSync();

if (tid < max_colors) {
    color_data[tid] = gs_color_offset[tid];
    color_data[max_colors + tid] = gs_color_count[tid];
    indirect_args[2 + tid].x = (gs_color_count[tid] + workgroup_size - 1) / workgroup_size;
    indirect_args[2 + tid].y = 1;
    indirect_args[2 + tid].z = 1;
}
AllMemoryBarrierWithGroupSync();

if (tid == 0) {
    uint running = 0;
    for (uint c = 0; c < max_colors; c++) {
        uint count = 0;
        uint full_offset = gs_color_offset[c];
        uint full_count = gs_color_count[c];
        jointless_color_data[c] = running;
        for (uint k = 0; k < full_count; k++) {
            uint bi = color_data[max_colors * 2 + full_offset + k];
            if (bi < body_count && joint_counts[bi] == 0) {
                jointless_color_data[max_colors * 2 + running + count] = bi;
                count++;
            }
        }
        jointless_color_data[max_colors + c] = count;
        jointless_indirect_args[2 + c].x = (count + workgroup_size - 1) / workgroup_size;
        jointless_indirect_args[2 + c].y = 1;
        jointless_indirect_args[2 + c].z = 1;
        running += count;
    }
}
}
