float loco_split_word_float(uint index) {
    return asfloat(loco_split_table_data[index]);
}

float loco_split_moment_arm(uint curve, float value) {
    uint data = loco_split_curve_data_base + curve * 2 * loco_split_samples;
    uint upper = loco_split_samples;
    for (uint i = 0; i < loco_split_samples; ++i) {
        if (value < loco_split_word_float(data + i)) {
            upper = i;
            break;
        }
    }
    if (upper == 0) return loco_split_word_float(data + loco_split_samples);
    if (upper == loco_split_samples) return loco_split_word_float(data + 2 * loco_split_samples - 1);
    float v0 = loco_split_word_float(data + upper - 1);
    float v1 = loco_split_word_float(data + upper);
    float a0 = loco_split_word_float(data + loco_split_samples + upper - 1);
    float a1 = loco_split_word_float(data + loco_split_samples + upper);
    float t = (value - v0) / (v1 - v0);
    return a0 + (a1 - a0) * t;
}

void loco_joint_split(uint3 dispatch_id) {
    uint id = dispatch_id.x;
    uint e = id / 2;
    uint side = id % 2;
    if (e >= pc.env_count) return;

    loco_muscle_row row = loco_muscle_table_data[e];
    if (row.driving == 0) return;

    uint slots = loco_split_leg_slots;
    uint muscles = loco_split_leg_muscles;
    uint state = id * loco_split_state_stride;
    uint warm = state + slots + 1;
    uint arms = warm + muscles;
    uint env = loco_split_env_base + e * loco_split_env_stride;
    bool primed = row.fresh == 0 && loco_split_state_data[state + slots] != 0.0;
    if (!primed) {
        for (uint mi = 0; mi < muscles; ++mi) {
            loco_split_state_data[warm + mi] = 0.0;
        }
    }

    float desired[8];
    float coordinate[8];
    for (uint c = 0; c < slots; ++c) {
        uint at = (side * slots + c) * loco_split_slot_words;
        uint parent = loco_split_table_data[env + loco_split_table_data[at + 0]];
        uint child = loco_split_table_data[env + loco_split_table_data[at + 1]];
        uint joint_words = loco_split_table_data[env + loco_split_bodies + loco_split_table_data[at + 2]];
        uint k = loco_split_table_data[at + 3];
        float3 q3 = coupled_coordinates_of(joint_words, loco_body_data[parent].orientation, loco_body_data[child].orientation);
        float q = k == 0 ? q3.x : (k == 1 ? q3.y : q3.z);
        float rate = primed ? (q - loco_split_state_data[state + c]) / pc.dt : 0.0;
        loco_split_state_data[state + c] = q;
        float action = loco_act_out_data[e * pc.act_dim + pc.target_dim + side * slots + c];
        float reference = clamp(loco_split_word_float(at + 4) + action * pc.action_scale, loco_split_word_float(at + 5), loco_split_word_float(at + 6));
        desired[c] = loco_split_stiffness * (reference - q - loco_split_damping * rate);
        coordinate[c] = q;
    }

    float plus[8];
    float minus[8];
    for (uint c = 0; c < slots; ++c) {
        plus[c] = 0.0;
        minus[c] = 0.0;
    }
    for (uint mi = 0; mi < muscles; ++mi) {
        uint global = loco_split_table_data[loco_split_muscle_base + side * muscles + mi];
        muscle_constraint m = loco_muscle_data[row.slots[global]];
        float path = muscle_fiber_path(m);
        muscle_fiber f = muscle_fiber_of(m, path, path, pc.h);
        float gain = f.valid ? m.max_force * (f.active * f.force_velocity * f.cos_pennation) : 0.0;
        for (uint c = 0; c < slots; ++c) {
            uint curve = loco_split_table_data[loco_split_curve_index_base + (side * slots + c) * muscles + mi];
            float torque = curve == loco_split_no_curve ? 0.0 : loco_split_moment_arm(curve, coordinate[c]) * gain;
            loco_split_state_data[arms + c * muscles + mi] = torque;
            plus[c] += max(torque, 0.0);
            minus[c] += min(torque, 0.0);
        }
    }
    for (uint c = 0; c < slots; ++c) {
        desired[c] = clamp(desired[c], minus[c], plus[c]);
    }

    float realized[8];
    for (uint sweep = 0; sweep < loco_split_sweeps; ++sweep) {
        for (uint c = 0; c < slots; ++c) {
            realized[c] = 0.0;
        }
        for (uint mi = 0; mi < muscles; ++mi) {
            float a = loco_split_state_data[warm + mi];
            for (uint c = 0; c < slots; ++c) {
                realized[c] += loco_split_state_data[arms + c * muscles + mi] * a;
            }
        }
        for (uint mi = 0; mi < muscles; ++mi) {
            float a = loco_split_state_data[warm + mi];
            float residual = 0.0;
            float diagonal = 0.0;
            for (uint c = 0; c < slots; ++c) {
                float torque = loco_split_state_data[arms + c * muscles + mi];
                residual += torque * (desired[c] - realized[c]);
                diagonal += torque * torque;
            }
            float next = clamp((residual + diagonal * a) / (diagonal + loco_split_weight), 0.0, 1.0);
            for (uint c = 0; c < slots; ++c) {
                realized[c] += loco_split_state_data[arms + c * muscles + mi] * (next - a);
            }
            loco_split_state_data[warm + mi] = next;
        }
    }

    loco_split_state_data[state + slots] = 1.0;
    for (uint mi = 0; mi < muscles; ++mi) {
        uint global = loco_split_table_data[loco_split_muscle_base + side * muscles + mi];
        loco_act_out_data[pc.env_count * pc.act_dim + e * loco_split_muscle_count + global] = clamp(loco_split_state_data[warm + mi], loco_split_floor, loco_split_ceiling);
    }
}
