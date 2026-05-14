
if (collision_state[CONVERGED_FLAG_INDEX] != 0) {
    return;
}

uint ci = dispatch_id.x;

uint actual_contact_count = min(collision_state[0], MAX_CONTACTS);
if (ci >= actual_contact_count) {
    return;
}

gpu_contact c = contact_data[ci];
gpu_body body_a = body_data[c.body_a];
gpu_body body_b = body_data[c.body_b];

float3 world_r_a = rotate_vector(body_a.predicted_orientation, c.local_anchor_a.xyz);
float3 world_r_b = rotate_vector(body_b.predicted_orientation, c.local_anchor_b.xyz);

float3 p_a = body_a.predicted_position.xyz + world_r_a;
float3 p_b = body_b.predicted_position.xyz + world_r_b;
float3 d = p_a - p_b;

float3 Cn = float3(
    dot(c.normal.xyz, d) + pc.collision_margin,
    dot(c.tangent_u.xyz, d),
    dot(c.tangent_v.xyz, d)
);
float3 C = Cn - c.c0.xyz * pc.current_alpha;

float4 lambda = c.lambda;
float4 penalty = c.penalty;

lambda.x = min(penalty.x * C.x + lambda.x, 0.0);

float friction_bound = abs(lambda.x) * c.friction_coeff;
lambda.y = clamp(penalty.y * C.y + lambda.y, -friction_bound, friction_bound);
lambda.z = clamp(penalty.z * C.z + lambda.z, -friction_bound, friction_bound);

if (lambda.x < 0.0) {
    penalty.x = min(penalty.x + pc.beta * abs(C.x), pc.penalty_max);
    InterlockedMax(collision_state[CONVERGENCE_MAX_DELTA_INDEX], asuint(abs(C.x)));
}

if (friction_bound > 1e-10) {
    if (abs(lambda.y) < friction_bound) {
        penalty.y = min(penalty.y + pc.beta * abs(C.y), pc.penalty_max);
    }
    if (abs(lambda.z) < friction_bound) {
        penalty.z = min(penalty.z + pc.beta * abs(C.z), pc.penalty_max);
    }
}

contact_data[ci].lambda = lambda;
contact_data[ci].penalty = penalty;
