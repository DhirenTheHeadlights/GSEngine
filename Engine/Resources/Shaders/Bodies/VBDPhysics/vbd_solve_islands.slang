void vbd_solve_islands(uint3 dispatch_id) {
    if (collision_state[state_converged_flag_index] != 0) return;
    solve_islands(dispatch_id);
}
