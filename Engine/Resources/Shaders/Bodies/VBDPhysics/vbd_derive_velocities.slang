
void vbd_derive_velocities(uint3 dispatch_id) {
    uint bi = dispatch_id.x;

    if (bi == 0) {
        collision_state[state_converged_flag_index] = 0;
        collision_state[state_convergence_max_delta_index] = 0;
        collision_state[state_convergence_max_angular_delta_index] = 0;
    }

    if (bi >= pc.body_count) return;

    body_state body = body_data[bi];
    bool locked = body.locked != 0;
    bool sleeping = body.sleep_counter >= sleep_threshold;
    bool update_orient = body.update_orientation != 0;

    if (locked || sleeping) {
        return;
    }

    float3 new_vel = (body.predicted_position.xyz - body.old_position.xyz) / pc.dt;
    body.velocity = new_vel;

    if (update_orient) {
        float4 delta_q = quat_mul(body.predicted_orientation, quat_conjugate(body.old_orientation));
        float q_s = delta_q.x;
        float3 q_v = delta_q.yzw;
        if (q_s < 0.0) { q_s = -q_s; q_v = -q_v; }

        float3 ang_vel;
        if (q_s < 0.9999) {
            float angle = 2.0 * acos(clamp(q_s, 0.0, 1.0));
            float sin_half = sqrt(1.0 - q_s * q_s);
            if (sin_half > 1e-6) {
                ang_vel = (q_v / sin_half) * (angle / pc.dt);
            } else {
                ang_vel = q_v * (2.0 / pc.dt);
            }
        } else {
            ang_vel = q_v * (2.0 / pc.dt);
        }

        body.angular_velocity = ang_vel;
    } else {
        body.angular_velocity = float3(0, 0, 0);
    }

    float lin_speed = length(body.velocity.xyz);
    float ang_speed = length(body.angular_velocity.xyz);
    if (lin_speed < pc.cfg.velocity_sleep_threshold && ang_speed < pc.cfg.angular_sleep_threshold) {
        body.sleep_counter = body.sleep_counter + 1;
    } else {
        body.sleep_counter = body.sleep_counter / 2;
    }

    body_data[bi] = body;
}
