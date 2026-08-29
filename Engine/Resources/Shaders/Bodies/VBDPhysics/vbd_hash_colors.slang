
groupshared uint colors_hash_accum;

void vbd_hash_colors(uint3 dispatch_id, uint3 group_thread_id) {
    uint tid = group_thread_id.x;

    if (tid == 0) {
        colors_hash_accum = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint local_count = 0;

    for (uint bi = tid; bi < pc.body_count; bi += adjacency_workgroup_size) {
        if (body_data[bi].locked != 0) {
            continue;
        }
        uint seen = 0;
        for (uint color = 0; color < max_colors; color++) {
            uint count = min(color_data[max_colors + color], pc.body_count);
            uint offset = color_data[color];
            for (uint k = 0; k < count; k++) {
                if (color_data[max_colors * 2 + offset + k] == bi) {
                    seen++;
                }
            }
        }
        if (pc.iteration == 7) {
            if (seen > 1) {
                local_count++;
            }
        }
        else {
            if (seen == 0) {
                local_count++;
            }
        }
    }

    uint dummy;
    InterlockedAdd(colors_hash_accum, local_count, dummy);
    GroupMemoryBarrierWithGroupSync();

    if (tid == 0) {
        collision_state[state_pass_hash_base_index + pc.substep * pass_hash_stage_count + pc.iteration] = colors_hash_accum;
    }
}
