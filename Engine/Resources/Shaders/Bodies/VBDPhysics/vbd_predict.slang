
uint bi = dispatch_id.x;
if (bi >= pc.body_count) return;

body_state body = body_data[bi];
bool locked = body.locked != 0;
bool sleeping = body.sleep_counter >= sleep_threshold;
bool update_orient = body.update_orientation != 0;
bool gravity = body.affected_by_gravity != 0;

if (pc.substep == 0) {
    for (uint mi = 0; mi < pc.motor_count; mi++) {
        velocity_motor_constraint m = motor_data[mi];
        if (m.body_index == bi && sleeping && length(m.target_velocity.xyz) > 0.01) {
            body.sleep_counter = 0;
            sleeping = false;
        }
    }

    if (sleeping) {
        float wake_thresh = pc.cfg.velocity_sleep_threshold * 10.0;
        uint co = contact_offsets[bi];
        uint cc = contact_counts[bi];
        for (uint k = 0; k < cc; k++) {
            uint ci = safe_load_uint(contact_adjacency, co + k, max_contact_adjacency);
            if (ci >= max_contacts) continue;
            contact_constraint c = contact_data[ci];
            uint other = (c.body_a == bi) ? c.body_b : c.body_a;
            if (other >= pc.body_count) continue;
            body_state other_body = body_data[other];
            bool other_locked = other_body.locked != 0;
            bool other_sleeping = other_body.sleep_counter >= sleep_threshold;
            float other_speed = length(other_body.velocity.xyz);

            if (!other_locked && !other_sleeping && other_speed > wake_thresh) {
                body.sleep_counter = 0;
                sleeping = false;
                break;
            }
        }

        if (sleeping) {
            uint j_off = joint_offsets[bi];
            uint jc = joint_counts[bi];
            for (uint k = 0; k < jc; k++) {
                uint ji = safe_load_uint(joint_adjacency, j_off + k, max_joint_adjacency);
                if (ji >= max_joints) continue;
                joint_constraint jnt = joint_data[ji];
                uint other = (jnt.body_a == bi) ? jnt.body_b : jnt.body_a;
                if (other >= pc.body_count) continue;
                body_state other_body = body_data[other];
                bool other_locked = other_body.locked != 0;
                bool other_sleeping = other_body.sleep_counter >= sleep_threshold;
                float other_speed = length(other_body.velocity.xyz);

                if (!other_locked && !other_sleeping && other_speed > wake_thresh) {
                    body.sleep_counter = 0;
                    sleeping = false;
                    break;
                }
            }
        }
    }
}

body.old_position = body.position;
body.old_orientation = body.orientation;

if (locked || sleeping) {
    body.predicted_position = body.position;
    body.inertia_target = body.position;
    body.predicted_orientation = body.orientation;
    body.angular_inertia_target = body.orientation;
    body.motor_target = body.position;
    body_data[bi] = body;
    return;
}

body.velocity *= (1.0 - pc.cfg.linear_damping);
body.angular_velocity *= (1.0 - pc.cfg.angular_damping);

float3 linear_velocity = body.velocity.xyz;
float lin_speed = length(linear_velocity);
if (lin_speed > pc.cfg.max_linear_speed) {
    linear_velocity *= (pc.cfg.max_linear_speed / lin_speed);
    body.velocity = linear_velocity;
}

float3 angular_velocity = body.angular_velocity.xyz;
float ang_speed = length(angular_velocity);
if (ang_speed > pc.cfg.max_angular_speed) {
    angular_velocity *= (pc.cfg.max_angular_speed / ang_speed);
    body.angular_velocity = angular_velocity;
}

float3 gravity_step = gravity ? float3(0, -9.8 * pc.dt * pc.dt, 0) : float3(0, 0, 0);
body.inertia_target = body.position + body.velocity * pc.dt + gravity_step;
body.predicted_position = body.position + body.velocity * pc.dt + gravity_step * body.accel_weight;
body.motor_target = body.inertia_target;

if (update_orient) {
    float3 w = angular_velocity;
    float omega_mag = length(w);
    if (omega_mag > 1e-7) {
        float angle = omega_mag * pc.dt;
        float3 axis = w / omega_mag;
        float half_a = angle * 0.5;
        float sh = sin(half_a);
        float ch = cos(half_a);
        float4 dq = float4(ch, sh * axis.x, sh * axis.y, sh * axis.z);
        body.predicted_orientation = quat_normalize(quat_mul(dq, body.orientation));
    } else {
        body.predicted_orientation = body.orientation;
    }
} else {
    body.predicted_orientation = body.orientation;
}

body.angular_inertia_target = body.predicted_orientation;

for (uint mi = 0; mi < pc.motor_count; mi++) {
    velocity_motor_constraint m = motor_data[mi];
    if (m.body_index != bi) continue;

    float3 target = body.old_position + m.target_velocity * pc.dt;
    if (m.horizontal_only != 0) {
        body.motor_target.x = target.x;
        body.motor_target.z = target.z;
    } else {
        body.motor_target = target;
    }
}

body_data[bi] = body;
