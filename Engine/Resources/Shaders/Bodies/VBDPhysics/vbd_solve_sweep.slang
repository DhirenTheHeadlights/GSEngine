groupshared uint gs_sweep_bail;

bool sweep_bailed(uint local_index) {
    if (local_index == 0) {
        uint bail_count;
        InterlockedAdd(collision_state[state_sweep_bail_index], 0, bail_count);
        gs_sweep_bail = bail_count;
    }
    AllMemoryBarrierWithGroupSync();
    return gs_sweep_bail != 0;
}

bool sweep_grid_barrier(uint local_index) {
    AllMemoryBarrierWithGroupSync();
    if (local_index == 0) {
        uint bail_count;
        InterlockedAdd(collision_state[state_sweep_bail_index], 0, bail_count);
        if (bail_count == 0) {
            uint phase_before;
            InterlockedAdd(collision_state[state_sweep_phase_index], 0, phase_before);
            uint arrived;
            InterlockedAdd(collision_state[state_sweep_arrive_index], 1, arrived);
            if (arrived + 1 == pc.sweep_workgroups) {
                uint previous;
                InterlockedExchange(collision_state[state_sweep_arrive_index], 0, previous);
                InterlockedAdd(collision_state[state_sweep_phase_index], 1);
            }
            else {
                uint checks = 0;
                uint delay = sweep_spin_delay_start;
                uint pause_acc = phase_before | 1u;
                bool advanced = false;
                for (; checks < sweep_spin_checks; checks++) {
                    uint phase_now;
                    InterlockedAdd(collision_state[state_sweep_phase_index], 0, phase_now);
                    if (phase_now - phase_before != 0) {
                        advanced = true;
                        break;
                    }
                    InterlockedAdd(collision_state[state_sweep_bail_index], 0, bail_count);
                    if (bail_count != 0) {
                        break;
                    }
                    for (uint p = 0; p < delay; p++) {
                        pause_acc = pause_acc * 1664525u + 1013904223u;
                    }
                    delay = min(delay * 2u, sweep_spin_delay_max);
                }
                if (!advanced && bail_count == 0) {
                    uint bailed_before;
                    InterlockedAdd(collision_state[state_sweep_bail_index], 1, bailed_before);
                    bail_count = bailed_before + 1;
                }
                if (pause_acc == 0xFFFFFFFFu) {
                    InterlockedAdd(collision_state[state_sweep_arrive_index], 0, pause_acc);
                }
            }
        }
        gs_sweep_bail = bail_count;
    }
    AllMemoryBarrierWithGroupSync();
    return gs_sweep_bail != 0;
}

void vbd_solve_sweep(uint3 dispatch_id) {
    if (collision_state[state_converged_flag_index] != 0) {
        return;
    }

    uint linear = dispatch_id.x;
    uint local_index = linear % workgroup_size;

    if (sweep_bailed(local_index)) {
        return;
    }

    uint total_threads = pc.sweep_workgroups * workgroup_size;
    uint num_colors = min(collision_state[state_max_used_color_index], pc.color_count);

    for (uint color = 0; color < num_colors; color++) {
        uint count = color_data[max_colors + color];
        uint offset = color_data[color];
        for (uint k = linear; k < count; k += total_threads) {
            uint bi = color_data[max_colors * 2 + offset + k];
            if (bi < pc.body_count) {
                solve_body(bi, false);
            }
        }
        if (color + 1 == num_colors && linear == 0) {
            collision_state[state_convergence_max_delta_index] = 0;
            collision_state[state_convergence_max_angular_delta_index] = 0;
        }
        if (sweep_grid_barrier(local_index)) {
            return;
        }
    }
}
