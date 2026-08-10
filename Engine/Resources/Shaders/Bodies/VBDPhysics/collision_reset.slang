
void collision_reset(uint3 dispatch_id) {
    uint bi = dispatch_id.x;

    if (bi == 0) {
        collision_pairs[0] = 0;
        for (uint i = 0; i < collision_state_header_uints; i++) {
            bool coloring_field = i == state_max_used_color_index
                || i == state_coloring_fallback_index
                || i == state_coloring_conflict_index;
            if (pc.substep != 0 && coloring_field) {
                continue;
            }
            collision_state[i] = 0;
        }
    }

    if (bi < pc.body_count) {
        contact_offsets[bi] = 0;
        contact_counts[bi] = 0;
    }

    if (bi < max_contacts) {
        contact_data[bi].body_a = 0;
        contact_data[bi].body_b = 0;
    }

    if (bi < max_grounded_uints) {
        grounded_bits[bi] = 0;
    }

    if (bi == 0) {
        grid_data[0] = 0;
    }
    uint grid_idx = bi + 1;
    if (grid_idx <= grid_table_size) {
        grid_data[grid_idx] = 0;
    }
}
