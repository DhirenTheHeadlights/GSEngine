void grass_tile_settle(uint3 dispatch_id) {
    grass_params p = params_data[0];
    uint s = dispatch_id.x;
    if (s >= grass_slot_count) {
        return;
    }

    grass_slot slot = slot_data[s];
    if (slot.tile_plus_one == 0u) {
        return;
    }

    uint t = slot.tile_plus_one - 1u;
    grass_tile tile = tile_data[t];
    bool calm = slot.unsettled == 0u && tile.wake == 0u;
    tile.calm_steps = calm ? tile.calm_steps + p.substeps : 0u;
    tile.sleep_ready = tile.calm_steps >= p.sleep_steps ? 1u : 0u;
    tile_data[t] = tile;
    slot_data[s].unsettled = 0u;
}
