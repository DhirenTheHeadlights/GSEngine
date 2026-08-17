
int3 world_to_cell(float3 pos, float cell_size) {
    return int3(floor(pos / cell_size));
}

uint hash_cell(int3 cell) {
    uint h = uint(cell.x * 73856093 ^ cell.y * 19349663 ^ cell.z * 83492791);
    return (h % grid_table_size) + 1;
}

void collision_grid_build(uint3 dispatch_id) {
    uint bi = dispatch_id.x;
    if (bi >= pc.body_count) {
        return;
    }

    body_state body = body_data[bi];

    uint bh = chash_step(0x9e3779b9u, bi);
    bh = chash_vec3(bh, body.position.xyz);
    bh = chash_vec3(bh, body.velocity.xyz);
    bh = chash_vec3(bh, body.angular_velocity.xyz);
    bh = chash_step(bh, chash_bits(body.orientation.x));
    bh = chash_step(bh, chash_bits(body.orientation.y));
    bh = chash_step(bh, chash_bits(body.orientation.z));
    bh = chash_step(bh, chash_bits(body.orientation.w));
    bh = chash_step(bh, body.sleep_counter);
    bh = chash_step(bh, body.locked);
    uint bhash_ignored;
    InterlockedAdd(collision_state[state_tick_hash_base_index + 6], bh, bhash_ignored);

    if (body.locked != 0) {
        return;
    }

    float cell_size = pc.grid_cell_size;
    int3 min_cell = world_to_cell(body.aabb_min.xyz - solver_config_data[0].speculative_margin, cell_size);
    int3 max_cell = world_to_cell(body.aabb_max.xyz + solver_config_data[0].speculative_margin, cell_size);

    for (int cx = min_cell.x; cx <= max_cell.x; cx++) {
        for (int cy = min_cell.y; cy <= max_cell.y; cy++) {
            for (int cz = min_cell.z; cz <= max_cell.z; cz++) {
                uint bucket = hash_cell(int3(cx, cy, cz));

                uint slot;
                InterlockedAdd(grid_data[0], 1u, slot);
                if (slot >= grid_max_entries) {
                    InterlockedAdd(grid_data[0], -1);
                    return;
                }

                uint entry_base = grid_table_size + 1 + slot * 2;
                grid_data[entry_base] = bi;
                uint old_head = grid_data[bucket];
                uint observed;
                for (;;) {
                    grid_data[entry_base + 1] = old_head;
                    InterlockedCompareExchange(grid_data[bucket], old_head, entry_base, observed);
                    if (observed == old_head) {
                        break;
                    }
                    old_head = observed;
                }
            }
        }
    }
}
