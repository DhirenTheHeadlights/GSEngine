groupshared uint gs_scan[grass_compact_threads];
groupshared uint gs_free[grass_slot_count];
groupshared uint gs_occupied[grass_slot_count];

uint compact_exclusive_scan(uint tid, uint value, out uint total) {
    GroupMemoryBarrierWithGroupSync();
    gs_scan[tid] = value;
    GroupMemoryBarrierWithGroupSync();
    for (uint offset = 1u; offset < grass_compact_threads; offset <<= 1u) {
        uint add = tid >= offset ? gs_scan[tid - offset] : 0u;
        GroupMemoryBarrierWithGroupSync();
        gs_scan[tid] += add;
        GroupMemoryBarrierWithGroupSync();
    }
    total = gs_scan[grass_compact_threads - 1u];
    return gs_scan[tid] - value;
}

void grass_tile_compact(uint3 group_thread_id) {
    grass_params p = params_data[0];
    uint tid = group_thread_id.x;

    bool free_slot = false;
    if (tid < grass_slot_count) {
        grass_slot slot = slot_data[tid];
        slot.fresh = 0u;
        if (slot.tile_plus_one != 0u) {
            uint t = slot.tile_plus_one - 1u;
            grass_tile tile = tile_data[t];
            if (tile.sleep_ready != 0u && tile.wake == 0u) {
                tile.slot_plus_one = 0u;
                tile.calm_steps = 0u;
                tile.sleep_ready = 0u;
                tile_data[t] = tile;
                slot.tile_plus_one = 0u;
            }
        }
        slot_data[tid] = slot;
        free_slot = slot.tile_plus_one == 0u;
        gs_occupied[tid] = free_slot ? 0u : 1u;
    }
    AllMemoryBarrierWithGroupSync();

    uint free_count;
    uint free_rank = compact_exclusive_scan(tid, free_slot ? 1u : 0u, free_count);
    if (free_slot) {
        gs_free[free_rank] = tid;
    }
    GroupMemoryBarrierWithGroupSync();

    uint requested = 0u;
    for (uint chunk = 0u; chunk < p.tile_count; chunk += grass_compact_threads) {
        uint t = chunk + tid;
        bool request = false;
        if (t < p.tile_count) {
            grass_tile tile = tile_data[t];
            request = tile.wake != 0u && tile.slot_plus_one == 0u;
        }
        uint chunk_total;
        uint rank = requested + compact_exclusive_scan(tid, request ? 1u : 0u, chunk_total);
        if (request && rank < free_count) {
            uint s = gs_free[rank];
            tile_data[t].slot_plus_one = s + 1u;
            tile_data[t].calm_steps = 0u;
            tile_data[t].sleep_ready = 0u;
            slot_data[s].tile_plus_one = t + 1u;
            slot_data[s].fresh = 1u;
            gs_occupied[s] = 1u;
        }
        requested += chunk_total;
    }
    GroupMemoryBarrierWithGroupSync();

    bool occupied = tid < grass_slot_count && gs_occupied[tid] != 0u;
    uint awake_count;
    uint awake_rank = compact_exclusive_scan(tid, occupied ? 1u : 0u, awake_count);
    if (occupied) {
        awake_data[awake_rank] = tid;
    }

    if (tid == 0u) {
        status_data[0].simulate_groups_x = (grass_tile_blade_count + grass_sim_threads - 1u) / grass_sim_threads;
        status_data[0].simulate_groups_y = awake_count;
        status_data[0].simulate_groups_z = 1u;
        status_data[0].awake_tiles = awake_count;
        status_data[0].overflow_total += requested > free_count ? requested - free_count : 0u;
    }
}
