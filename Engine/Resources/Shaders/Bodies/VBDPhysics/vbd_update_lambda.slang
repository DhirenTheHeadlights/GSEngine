
void update_contact_lambda(uint ci) {
    contact_constraint c = contact_data[ci];
    body_state body_a = body_data[c.body_a];
    body_state body_b = body_data[c.body_b];

    if (pc.fold_lambda != 0) {
        uint island_a = body_env_data[c.body_a];
        uint island_b = body_env_data[c.body_b];
        bool a_owner = island_a != 0xFFFFFFFFu && body_a.locked == 0 && (island_a == island_b || body_b.locked != 0);
        bool b_owner = island_b != 0xFFFFFFFFu && body_b.locked == 0 && (island_a == island_b || body_a.locked != 0);
        if (a_owner || b_owner) return;
    }

    float3 world_r_a = rotate_vector(body_a.predicted_orientation, c.local_anchor_a.xyz);
    float3 world_r_b = rotate_vector(body_b.predicted_orientation, c.local_anchor_b.xyz);

    float3 p_a = body_a.predicted_position.xyz + world_r_a;
    float3 p_b = body_b.predicted_position.xyz + world_r_b;
    float3 d = p_a - p_b;

    float3 Cn = float3(
        dot(c.normal.xyz, d) + solver_config_data[0].collision_margin,
        dot(c.tangent_u.xyz, d),
        dot(c.tangent_v.xyz, d)
    );
    float3 C = Cn - c.c0.xyz * pc.current_alpha;

    float3 lambda = c.lambda;
    float3 penalty = c.penalty;

    lambda.x = min(penalty.x * C.x + lambda.x, 0.0);

    float friction_bound = abs(lambda.x) * c.friction_coeff;
    lambda.y = clamp(penalty.y * C.y + lambda.y, -friction_bound, friction_bound);
    lambda.z = clamp(penalty.z * C.z + lambda.z, -friction_bound, friction_bound);

    if (lambda.x < 0.0) {
        penalty.x = min(penalty.x + solver_config_data[0].beta * abs(C.x), solver_config_data[0].penalty_max);
        InterlockedMax(collision_state[state_convergence_max_delta_index], asuint(abs(C.x)));
    }

    if (friction_bound > 1e-10) {
        if (abs(lambda.y) < friction_bound) {
            penalty.y = min(penalty.y + solver_config_data[0].beta * abs(C.y), solver_config_data[0].penalty_max);
        }
        if (abs(lambda.z) < friction_bound) {
            penalty.z = min(penalty.z + solver_config_data[0].beta * abs(C.z), solver_config_data[0].penalty_max);
        }
    }

    contact_data[ci].lambda = lambda;
    contact_data[ci].penalty = penalty;

    uint trace_body = solver_config_data[0].trace_body;
    if (pc.substep == 0 && pc.iteration < iteration_trace_slots && (c.body_a == trace_body || c.body_b == trace_body)) {
        uint base = state_iteration_trace_base_index + pc.iteration * iteration_trace_uints;
        collision_state[base] = asuint(C.x);
        collision_state[base + 1] = asuint(lambda.x);
        collision_state[base + 2] = asuint(penalty.x);
    }
}

void fold_convergence_check(bool check) {
    uint linear_bits;
    uint angular_bits;
    uint ignored;
    InterlockedAdd(collision_state[state_convergence_max_delta_index], 0, linear_bits);
    InterlockedAdd(collision_state[state_convergence_max_angular_delta_index], 0, angular_bits);
    InterlockedExchange(collision_state[state_convergence_max_delta_index], 0, ignored);
    InterlockedExchange(collision_state[state_convergence_max_angular_delta_index], 0, ignored);
    InterlockedExchange(collision_state[state_lambda_arrive_index], 0, ignored);
    if (!check) return;

    float linear = asfloat(linear_bits);
    float angular = asfloat(angular_bits);

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

void vbd_update_lambda(uint3 dispatch_id) {
    uint ci = dispatch_id.x;
    bool converged = collision_state[state_converged_flag_index] != 0;

    if (!converged) {
        uint actual_contact_count = min(collision_state[state_contact_count_index], max_contacts);
        if (ci < actual_contact_count) {
            update_contact_lambda(ci);
        }
    }

    if (pc.fold_lambda == 0 || converged) return;

    AllMemoryBarrierWithGroupSync();
    if (ci % workgroup_size != 0) return;

    uint arrived;
    InterlockedAdd(collision_state[state_lambda_arrive_index], 1, arrived);
    if (arrived + 1 != indirect_args[1].x) return;

    fold_convergence_check(pc.color_offset + 1 >= pc.color_count);
}
