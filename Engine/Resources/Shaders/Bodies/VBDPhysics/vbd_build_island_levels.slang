void vbd_build_island_levels(uint3 dispatch_id) {
    if (collision_state[state_converged_flag_index] != 0) return;
    build_island_levels(dispatch_id);
}
