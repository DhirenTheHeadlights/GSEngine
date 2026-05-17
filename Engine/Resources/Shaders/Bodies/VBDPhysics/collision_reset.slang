
uint bi = dispatch_id.x;

if (bi == 0) {
    collision_pairs[0] = 0;
    collision_state[0] = 0;
    collision_state[1] = 0;
    collision_state[2] = 0;
    collision_state[3] = 0;
    collision_state[4] = 0;
    collision_state[5] = 0;
    collision_state[6] = 0;
}

if (bi < pc.body_count) {
    contact_offsets[bi] = 0;
    contact_counts[bi] = 0;
}

if (bi < MAX_CONTACTS) {
    contact_data[bi].body_a = 0;
    contact_data[bi].body_b = 0;
}

if (bi < MAX_GROUNDED_UINTS) {
    grounded_bits[bi] = 0;
}

if (bi == 0) {
    grid_data[0] = 0;
}
uint grid_idx = bi + 1;
if (grid_idx <= GRID_TABLE_SIZE) {
    grid_data[grid_idx] = 0;
}
