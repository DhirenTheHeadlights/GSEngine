
static const uint ELEMS_PER_THREAD = (max_bodies + adjacency_workgroup_size - 1) / adjacency_workgroup_size;

groupshared uint gs_color_count[max_colors];
groupshared uint gs_color_offset[max_colors];
groupshared uint gs_color_fill[max_colors];
groupshared uint gs_max_contact_color;
groupshared uint gs_used_motor_only;
groupshared uint gs_max_used_color;
groupshared uint gs_coloring_fallback;
groupshared uint gs_coloring_conflicts;
static const uint max_waves_per_group = adjacency_workgroup_size / 32;
groupshared uint gs_wave_color_count[max_waves_per_group * max_colors];
groupshared uint gs_wave_color_base[max_waves_per_group * max_colors];

uint live_color_of(uint bi) {
    return coloring_scratch[bi] & 0xFFu;
}

void store_live_color(uint bi, uint c) {
    coloring_scratch[bi] = c & 0xFFu;
}

void collision_build_coloring(uint3 dispatch_id, uint3 group_thread_id) {
    uint tid = group_thread_id.x;
uint body_count = pc.body_count;
uint num_contacts = min(collision_state[state_contact_count_index], max_contacts);

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

[unroll]
for (uint km = 0; km < ELEMS_PER_THREAD; km++) {
    uint bi = tid + km * adjacency_workgroup_size;
    if (bi >= body_count) continue;
    uint c = live_color_of(bi);
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
    if (live_color_of(bi) == 0xFFu) {
        uint motor_only = (gs_max_contact_color < max_colors) ? gs_max_contact_color : 0u;
        store_live_color(bi, motor_only);
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
    if (live_color_of(bi) == 0xFFu) {
        store_live_color(bi, free_body_color);
    }
}
AllMemoryBarrierWithGroupSync();

for (uint cv = tid; cv < num_contacts; cv += adjacency_workgroup_size) {
    contact_constraint c = contact_data[cv];
    if (c.body_a >= body_count || c.body_b >= body_count) continue;
    if (body_data[c.body_a].locked != 0 || body_data[c.body_b].locked != 0) continue;
    uint color_a = live_color_of(c.body_a);
    uint color_b = live_color_of(c.body_b);
    if (color_a == color_b && color_a != 0xFFu) {
        uint dummy;
        InterlockedAdd(gs_coloring_conflicts, 1u, dummy);
    }
}
AllMemoryBarrierWithGroupSync();

[unroll]
for (uint kc = 0; kc < ELEMS_PER_THREAD; kc++) {
    uint bi = tid + kc * adjacency_workgroup_size;
    if (bi >= body_count) continue;
    uint c = live_color_of(bi);
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
        collision_state[state_color_population_base_index + c] = gs_color_count[c];
    }
    collision_state[state_max_used_color_index] = gs_max_used_color;
    collision_state[state_coloring_fallback_index] = gs_coloring_fallback;
    collision_state[state_coloring_conflict_index] = gs_coloring_conflicts;
}
AllMemoryBarrierWithGroupSync();

uint lane_count = WaveGetLaneCount();
uint wave_index = tid / lane_count;
uint wave_count = (adjacency_workgroup_size + lane_count - 1u) / lane_count;

for (uint ks = 0; ks < ELEMS_PER_THREAD; ks++) {
    uint chunk_base = ks * adjacency_workgroup_size;
    if (chunk_base >= body_count) {
        break;
    }

    uint bi = tid + chunk_base;
    bool active = bi < body_count;
    uint c = active ? live_color_of(bi) : 0xFFu;
    uint rank_in_wave = 0;

    for (uint cc = 0; cc < max_colors; cc++) {
        bool mine = active && c == cc;
        uint prefix = WavePrefixCountBits(mine);
        uint total = WaveActiveCountBits(mine);
        if (mine) {
            rank_in_wave = prefix;
        }
        if (WaveIsFirstLane()) {
            gs_wave_color_count[wave_index * max_colors + cc] = total;
        }
    }
    AllMemoryBarrierWithGroupSync();

    if (tid < max_colors) {
        uint running = gs_color_fill[tid];
        for (uint w = 0; w < wave_count; w++) {
            gs_wave_color_base[w * max_colors + tid] = running;
            running += gs_wave_color_count[w * max_colors + tid];
        }
        gs_color_fill[tid] = running;
    }
    AllMemoryBarrierWithGroupSync();

    if (active && c < max_colors) {
        uint pos = gs_color_offset[c] + gs_wave_color_base[wave_index * max_colors + c] + rank_in_wave;
        if (pos < max_bodies) {
            color_data[max_colors * 2 + pos] = bi;
        }
    }
    AllMemoryBarrierWithGroupSync();
}

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
