void vbd_convergence_check(uint3 dispatch_id) {
    if (dispatch_id.x != 0) return;
    if (collision_state[state_converged_flag_index] != 0) return;
    if (pc.color_offset + 1 < pc.color_count) return;

    float linear = asfloat(collision_state[state_convergence_max_delta_index]);
    float angular = asfloat(collision_state[state_convergence_max_angular_delta_index]);

    InterlockedAdd(collision_state[state_stale_check_count_index], 1);

    float linear_threshold = solver_config_data[0].convergence_threshold_linear;
    float angular_threshold = solver_config_data[0].convergence_threshold_angular;
    float speed_scale = solver_config_data[0].convergence_speed_scale;
    if (speed_scale > 0.0) {
        float max_speed = asfloat(collision_state[state_max_speed_index]);
        linear_threshold = max(linear_threshold, max_speed * pc.dt * speed_scale);
        float max_angular_speed = asfloat(collision_state[state_max_angular_speed_index]);
        angular_threshold = max(angular_threshold, max_angular_speed * pc.dt * speed_scale);
    }

    if (linear < linear_threshold && angular < angular_threshold) {
        InterlockedAdd(collision_state[state_stale_read_count_index], 1);
        collision_state[state_converged_flag_index] = 1;
        return;
    }

    float prev_linear = asfloat(collision_state[state_convergence_prev_linear_index]);
    float prev_angular = asfloat(collision_state[state_convergence_prev_angular_index]);

    if (prev_linear > 0.0 && prev_angular > 0.0 &&
        linear > prev_linear * 0.99 && angular > prev_angular * 0.99) {
        InterlockedAdd(collision_state[state_stale_read_count_index], 1);
        collision_state[state_converged_flag_index] = 1;
        return;
    }

    collision_state[state_convergence_prev_linear_index] = asuint(linear);
    collision_state[state_convergence_prev_angular_index] = asuint(angular);
}
