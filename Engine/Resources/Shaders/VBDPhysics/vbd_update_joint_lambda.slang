import common;
import vbd_shared;
import vbd_physics;

[[vk::push_constant]]
ConstantBuffer<vbd_push_constants> pc;

[Layout(LayoutKind::VBDPhysics)]
[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    if (collision_state[CONVERGED_FLAG_INDEX] != 0) return;

    uint ji = dispatch_id.x;
    if (ji >= pc.joint_count) return;

    GpuJoint j = joint_data[ji];
    GpuBody body_a = body_data[j.body_a];
    GpuBody body_b = body_data[j.body_b];

    bool a_locked = (body_a.flags & FLAG_LOCKED) != 0;
    bool b_locked = (body_b.flags & FLAG_LOCKED) != 0;
    bool a_sleeping = body_a.sleep_counter >= SLEEP_THRESHOLD;
    bool b_sleeping = body_b.sleep_counter >= SLEEP_THRESHOLD;
    if ((a_locked || a_sleeping) && (b_locked || b_sleeping)) return;

    float3 r_aw = rotate_vector(body_a.predicted_orientation, j.local_anchor_a.xyz);
    float3 r_bw = rotate_vector(body_b.predicted_orientation, j.local_anchor_b.xyz);
    float3 p_a = body_a.predicted_position.xyz + r_aw;
    float3 p_b = body_b.predicted_position.xyz + r_bw;
    float3 d = p_a - p_b;

    if (j.type == JOINT_TYPE_DISTANCE) {
        float d_mag = length(d);
        float c = (d_mag - j.target_distance) - j.pos_c0.x;
        j.pos_lambda.x = j.pos_penalty.x * c + j.pos_lambda.x;
        float penalty_cap = pc.penalty_max;
        if (j.compliance > 0.0) {
            penalty_cap = min(1.0 / max(j.compliance * pc.h_squared, 1e-10), pc.penalty_max);
        }
        j.pos_penalty.x = min(j.pos_penalty.x + pc.beta * abs(c), penalty_cap);
        InterlockedMax(collision_state[CONVERGENCE_MAX_DELTA_INDEX], asuint(abs(c)));
    }
    else if (j.type == JOINT_TYPE_FIXED || j.type == JOINT_TYPE_HINGE) {
        float3 dirs[3] = { float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1) };
        for (int k = 0; k < 3; k++) {
            float c = dot(dirs[k], d) - j.pos_c0[k];
            j.pos_lambda[k] = j.pos_penalty[k] * c + j.pos_lambda[k];
            j.pos_penalty[k] = min(j.pos_penalty[k] + pc.beta * abs(c), pc.penalty_max);
            InterlockedMax(collision_state[CONVERGENCE_MAX_DELTA_INDEX], asuint(abs(c)));
        }

        float4 q_error = quat_mul(body_b.predicted_orientation, quat_mul(quat_conjugate(body_a.predicted_orientation), quat_conjugate(j.rest_orientation)));
        float3 theta = to_axis_angle(q_error);

        if (j.type == JOINT_TYPE_FIXED) {
            for (int k = 0; k < 3; k++) {
                float c_ang = theta[k] - j.ang_c0[k];
                j.ang_lambda[k] = j.ang_penalty[k] * c_ang + j.ang_lambda[k];
                j.ang_penalty[k] = min(j.ang_penalty[k] + pc.ang_beta * abs(c_ang), pc.penalty_max);
            }
        }
        else {
            float3 axis_a = rotate_vector(body_a.predicted_orientation, j.local_axis_a.xyz);
            float3 axis_b = rotate_vector(body_b.predicted_orientation, j.local_axis_b.xyz);
            float3 swing_error = cross(axis_a, axis_b);

            float3 perp_u = cross(axis_a, float3(0, 1, 0));
            if (length(perp_u) < 1e-6) perp_u = cross(axis_a, float3(1, 0, 0));
            perp_u = normalize(perp_u);
            float3 perp_v = normalize(cross(axis_a, perp_u));

            float c_u = dot(perp_u, swing_error) - j.ang_c0.x;
            float c_v = dot(perp_v, swing_error) - j.ang_c0.y;

            j.ang_lambda.x = j.ang_penalty.x * c_u + j.ang_lambda.x;
            j.ang_penalty.x = min(j.ang_penalty.x + pc.ang_beta * abs(c_u), pc.penalty_max);

            j.ang_lambda.y = j.ang_penalty.y * c_v + j.ang_lambda.y;
            j.ang_penalty.y = min(j.ang_penalty.y + pc.ang_beta * abs(c_v), pc.penalty_max);

            if (j.limits_enabled != 0) {
                float twist = dot(axis_a, theta);

                if (twist < j.limit_lower) {
                    float c_limit = (twist - j.limit_lower) - j.limit_c0;
                    j.limit_lambda = min(j.limit_penalty * c_limit + j.limit_lambda, 0.0);
                    j.limit_penalty = min(j.limit_penalty + pc.ang_beta * abs(c_limit), pc.penalty_max);
                }
                else if (twist > j.limit_upper) {
                    float c_limit = (twist - j.limit_upper) - j.limit_c0;
                    j.limit_lambda = max(j.limit_penalty * c_limit + j.limit_lambda, 0.0);
                    j.limit_penalty = min(j.limit_penalty + pc.ang_beta * abs(c_limit), pc.penalty_max);
                }
            }
        }
    }
    else if (j.type == JOINT_TYPE_SLIDER) {
        float3 axis_w = normalize(rotate_vector(body_a.predicted_orientation, j.local_axis_a.xyz));
        float3 perp0 = cross(axis_w, float3(0, 1, 0));
        if (length(perp0) < 1e-6) perp0 = cross(axis_w, float3(1, 0, 0));
        perp0 = normalize(perp0);
        float3 perp1 = normalize(cross(axis_w, perp0));

        float3 perps[2] = { perp0, perp1 };
        for (int k = 0; k < 2; k++) {
            float c = dot(perps[k], d) - j.pos_c0[k];
            j.pos_lambda[k] = j.pos_penalty[k] * c + j.pos_lambda[k];
            j.pos_penalty[k] = min(j.pos_penalty[k] + pc.beta * abs(c), pc.penalty_max);
            InterlockedMax(collision_state[CONVERGENCE_MAX_DELTA_INDEX], asuint(abs(c)));
        }

        float4 q_error = quat_mul(body_b.predicted_orientation, quat_mul(quat_conjugate(body_a.predicted_orientation), quat_conjugate(j.rest_orientation)));
        float3 theta = to_axis_angle(q_error);

        for (int k = 0; k < 3; k++) {
            float c_ang = theta[k] - j.ang_c0[k];
            j.ang_lambda[k] = j.ang_penalty[k] * c_ang + j.ang_lambda[k];
            j.ang_penalty[k] = min(j.ang_penalty[k] + pc.ang_beta * abs(c_ang), pc.penalty_max);
        }
    }

    joint_data[ji] = j;
}
