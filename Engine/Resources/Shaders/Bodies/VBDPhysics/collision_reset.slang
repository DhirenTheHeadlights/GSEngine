
void collision_reset(uint3 dispatch_id) {
    uint bi = dispatch_id.x;

    if (bi == 0) {
        collision_pairs[0] = 0;
        for (uint i = 0; i < collision_state_header_uints; i++) {
            bool coloring_field = i == state_max_used_color_index
                || i == state_coloring_fallback_index
                || i == state_coloring_conflict_index;
            bool hash_field = i >= state_pass_hash_base_index;
            bool tick_scope_field = i >= state_tick_hash_base_index;
            if (tick_scope_field || (pc.substep != 0 && (coloring_field || hash_field))) {
                continue;
            }
            collision_state[i] = 0;
        }
        collision_state[state_max_speed_index] = 0;
        collision_state[state_max_angular_speed_index] = 0;

        if (pc.substep == 0) {
            collision_state[state_tick_hash_base_index] = 0;
            uint grad_check_base = state_tick_hash_base_index + 3;
            for (uint g = 0; g < 5; g++) {
                collision_state[grad_check_base + g] = 0;
            }
        }
    }

    if (bi < max_contacts) {
        contact_data[bi].body_a = 0;
        contact_data[bi].body_b = 0;
    }

    if (bi < max_grounded_uints) {
        grounded_bits[bi] = 0;
    }

    if (bi < max_bodies) {
        coloring_scratch[bi] = 0xFFFFFFFFu;
    }

    if (bi == 0) {
        grid_data[0] = 0;
    }
    uint grid_idx = bi + 1;
    if (grid_idx <= grid_table_size) {
        grid_data[grid_idx] = 0;
    }
}
