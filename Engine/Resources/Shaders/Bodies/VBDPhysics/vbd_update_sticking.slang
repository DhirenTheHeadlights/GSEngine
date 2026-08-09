
void vbd_update_sticking(uint3 dispatch_id) {
    uint ci = dispatch_id.x;

    uint actual_contact_count = min(collision_state[state_contact_count_index], max_contacts);
    if (ci >= actual_contact_count) {
        return;
    }

    contact_constraint c = contact_data[ci];

    float friction_bound = abs(c.lambda.x) * c.friction_coeff;
    float tangential_lambda = length(float2(c.lambda.y, c.lambda.z));
    float tangential_gap = length(float2(c.c0.y, c.c0.z));

    bool sticking =
        c.lambda.x < -1e-3 &&
        tangential_gap < solver_config_data[0].stick_threshold &&
        tangential_lambda < friction_bound;

    contact_data[ci].sticking = sticking ? 1u : 0u;
}
