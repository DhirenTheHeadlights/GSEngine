
void vbd_update_lambda(uint3 dispatch_id) {
    if (collision_state[state_converged_flag_index] != 0) {
        return;
    }

    uint ci = dispatch_id.x;

    uint actual_contact_count = min(collision_state[state_contact_count_index], max_contacts);
    if (ci >= actual_contact_count) {
        return;
    }

    contact_constraint c = contact_data[ci];
    body_state body_a = body_data[c.body_a];
    body_state body_b = body_data[c.body_b];

    float3 world_r_a = rotate_vector(body_a.predicted_orientation, c.local_anchor_a.xyz);
    float3 world_r_b = rotate_vector(body_b.predicted_orientation, c.local_anchor_b.xyz);

    float3 p_a = body_a.predicted_position.xyz + world_r_a;
    float3 p_b = body_b.predicted_position.xyz + world_r_b;
    float3 d = p_a - p_b;

    float3 Cn = float3(
        dot(c.normal.xyz, d) + pc.cfg.collision_margin,
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
        penalty.x = min(penalty.x + pc.cfg.beta * abs(C.x), pc.cfg.penalty_max);
        InterlockedMax(collision_state[state_convergence_max_delta_index], asuint(abs(C.x)));
    }

    if (friction_bound > 1e-10) {
        if (abs(lambda.y) < friction_bound) {
            penalty.y = min(penalty.y + pc.cfg.beta * abs(C.y), pc.cfg.penalty_max);
        }
        if (abs(lambda.z) < friction_bound) {
            penalty.z = min(penalty.z + pc.cfg.beta * abs(C.z), pc.cfg.penalty_max);
        }
    }

    contact_data[ci].lambda = lambda;
    contact_data[ci].penalty = penalty;
}
