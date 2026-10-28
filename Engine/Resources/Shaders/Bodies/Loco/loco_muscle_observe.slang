float loco_maintenance_share(float ratio) {
    if (ratio <= loco_maintenance_lengths[0]) return loco_maintenance_shares[0];
    for (uint i = 1; i < 4; ++i) {
        if (ratio <= loco_maintenance_lengths[i]) {
            float t = (ratio - loco_maintenance_lengths[i - 1]) / (loco_maintenance_lengths[i] - loco_maintenance_lengths[i - 1]);
            return loco_maintenance_shares[i - 1] + (loco_maintenance_shares[i] - loco_maintenance_shares[i - 1]) * t;
        }
    }
    return loco_maintenance_shares[3];
}

float loco_metabolic_power(muscle_constraint m, muscle_fiber f, float excitation, loco_muscle_constants heat) {
    float u = saturate(excitation);
    float slow = loco_slow_twitch * sin(1.57079632679489661923 * u);
    float fast = (1.0 - loco_slow_twitch) * (1.0 - cos(1.57079632679489661923 * u));
    float activation_heat = heat.activation_slow * slow + heat.activation_fast * fast;
    float maintenance_heat = loco_maintenance_share(f.fiber_ratio) * (heat.maintenance_slow * slow + heat.maintenance_fast * fast);
    float isometric = m.max_force * m.activation * f.active;
    float active = isometric * f.force_velocity;
    float alpha = f.fiber_velocity <= 0.0 ? loco_shortening_isometric * isometric + loco_shortening_active * active : loco_lengthening_active * active;
    float heat_rate = max(activation_heat + maintenance_heat - alpha * f.fiber_velocity, heat.minimum_heat);
    return max(heat_rate - active * f.fiber_velocity, 0.0);
}

void loco_muscle_observe(uint3 dispatch_id) {
    uint e = dispatch_id.x;
    if (e >= pc.env_count) return;

    loco_muscle_row row = loco_muscle_table_data[e];
    uint prev = e * loco_muscle_prev_stride;
    uint base = e * pc.obs_stride + pc.obs_base;

    if (loco_env_data[e].valid == 0 || row.driving == 0) {
        for (uint k = 0; k < loco_muscle_count * loco_muscle_obs_per; ++k) {
            loco_obs_data[base + k] = 0.0;
        }
        for (uint k = 0; k < loco_muscle_count; ++k) {
            loco_obs_data[base + k * loco_muscle_obs_per + 2] = loco_muscle_constants_data[k].rest_activation;
        }
        loco_muscle_prev_data[prev + loco_muscle_count] = 0.0;
        return;
    }

    bool primed = loco_muscle_prev_data[prev + loco_muscle_count] != 0.0 && row.fresh == 0;
    float power = 0.0;
    float change = 0.0;
    for (uint k = 0; k < loco_muscle_count; ++k) {
        uint slot = row.slots[k];
        muscle_constraint m = loco_muscle_data[slot];
        loco_muscle_constants constants = loco_muscle_constants_data[k];
        m.activation = row.fresh != 0 ? constants.rest_activation : m.activation;
        float excitation = row.fresh != 0 ? constants.rest_activation : loco_excitation_data[slot];
        float path = row.fresh != 0 && m.tendon_shape > 0.0 ? compliant_fiber_of(m, m.path_length_start, 0.0, pc.dt).x + m.tendon_slack_length : muscle_fiber_path(m);
        float previous = primed ? loco_muscle_prev_data[prev + k] : path;
        muscle_fiber f = muscle_fiber_of(m, path, previous, pc.dt);
        power += loco_metabolic_power(m, f, excitation, constants);

        uint o = base + k * loco_muscle_obs_per;
        loco_obs_data[o + 0] = path / (m.optimal_fiber_length + m.tendon_slack_length);
        loco_obs_data[o + 1] = (path - previous) / pc.dt / m.max_contraction_velocity;
        loco_obs_data[o + 2] = m.activation;
        loco_muscle_prev_data[prev + k] = path;
        if (loco_r_excitation_smooth != 0.0) {
            uint last_slot = prev + loco_muscle_count + 1 + k;
            float step = excitation - (primed ? loco_muscle_prev_data[last_slot] : constants.rest_activation);
            change += step * step;
            loco_muscle_prev_data[last_slot] = excitation;
        }
    }
    loco_muscle_prev_data[prev + loco_muscle_count] = 1.0;

    if (pc.write_reward == 0) return;
    uint rbase = pc.env_count * pc.obs_stride + e * loco_reward_stride;
    if (pc.r_metabolic != 0.0) {
        loco_obs_data[rbase + loco_reward_metabolic] = -pc.r_metabolic * power / pc.reward_power;
    }
    if (loco_r_excitation_smooth != 0.0) {
        loco_obs_data[rbase + loco_reward_smooth] -= loco_r_excitation_smooth * change / float(loco_muscle_count);
    }
}
