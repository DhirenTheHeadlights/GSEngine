groupshared float gs_x[nn_max_in];
groupshared float gs_a1[nn_hidden];
groupshared float gs_a2[nn_hidden];
groupshared float gs_out[nn_max_out];
groupshared float gs_od[nn_max_out];
groupshared float gs_h2p[nn_hidden];

void nn_train_sample(uint3 group_id, uint3 group_thread_id) {
    uint s = group_id.x;
    uint j = group_thread_id.x;
    if (s >= pc.sample_count) {
        return;
    }
    uint sample = pc.sample_base + s;
    uint in_dim = pc.in_dim;
    uint out_dim = pc.out_dim;
    uint w1t = nn_w1t_off(pc.param_count);
    uint w2t = nn_w2t_off(pc.param_count, in_dim);
    uint w3t = nn_w3t_off(pc.param_count, in_dim);

    if (j < in_dim) {
        gs_x[j] = nn_inputs[pc.input_offset + sample * in_dim + j];
    }
    GroupMemoryBarrierWithGroupSync();

    float acc = nn_state[nn_b1_off(in_dim) + j];
    for (uint i = 0; i < in_dim; ++i) {
        acc += nn_state[w1t + i * nn_hidden + j] * gs_x[i];
    }
    float a1 = tanh(acc);
    gs_a1[j] = a1;
    nn_scratch[nn_h1_off + sample * nn_hidden + j] = a1;
    GroupMemoryBarrierWithGroupSync();

    acc = nn_state[nn_b2_off(in_dim) + j];
    for (uint k = 0; k < nn_hidden; ++k) {
        acc += nn_state[w2t + k * nn_hidden + j] * gs_a1[k];
    }
    float a2 = tanh(acc);
    gs_a2[j] = a2;
    nn_scratch[nn_h2_off + sample * nn_hidden + j] = a2;
    GroupMemoryBarrierWithGroupSync();

    uint out_row = nn_out_off + sample * out_dim;
    if (j < out_dim) {
        acc = nn_state[nn_b3_off(in_dim, out_dim) + j];
        for (uint k = 0; k < nn_hidden; ++k) {
            acc += nn_state[w3t + k * out_dim + j] * gs_a2[k];
        }
        gs_out[j] = acc;
        nn_scratch[out_row + j] = acc;
    }
    GroupMemoryBarrierWithGroupSync();

    if (j == 0) {
        if (pc.mode == 0) {
            uint ls_off = nn_ls_off(in_dim, out_dim);
            uint act_row = pc.target_offset + sample * out_dim;
            float log_prob = 0.0;
            float entropy = 0.0;
            for (uint i = 0; i < out_dim; ++i) {
                float ls = nn_state[ls_off + i];
                float sigma = exp(ls);
                float diff = (nn_inputs[act_row + i] - gs_out[i]) / sigma;
                log_prob += -0.5 * diff * diff - ls - 0.5 * nn_log_2pi;
                entropy += ls + nn_gaussian_entropy_const;
            }
            float adv = nn_inputs[pc.adv_offset + sample];
            float ratio = exp(log_prob - nn_inputs[pc.aux_offset + sample]);
            float clipped = clamp(ratio, 1.0 - pc.clip_eps, 1.0 + pc.clip_eps);
            bool not_clipped = (adv >= 0.0) ? (ratio < 1.0 + pc.clip_eps) : (ratio > 1.0 - pc.clip_eps);
            float d_logp = not_clipped ? -ratio * adv : 0.0;
            nn_scratch[nn_sample_loss_off + s] = (not_clipped ? -ratio * adv : -clipped * adv) - pc.entropy_coeff * entropy;
            for (uint i = 0; i < out_dim; ++i) {
                float ls = nn_state[ls_off + i];
                float sigma = exp(ls);
                float centered = nn_inputs[act_row + i] - gs_out[i];
                float diff = centered / sigma;
                float od = d_logp * centered / (sigma * sigma);
                gs_od[i] = od;
                nn_scratch[nn_out_delta_off + s * out_dim + i] = od;
                nn_scratch[nn_ls_terms_off + s * out_dim + i] = d_logp * (diff * diff - 1.0) - pc.entropy_coeff;
            }
        }
        else if (pc.mode == 1) {
            float err = gs_out[0] - nn_inputs[pc.target_offset + sample];
            float od = pc.value_coeff * err;
            gs_od[0] = od;
            nn_scratch[nn_out_delta_off + s * out_dim] = od;
            nn_scratch[nn_sample_loss_off + s] = 0.5 * err * err;
        }
        else {
            float target = s < pc.n_real ? 1.0 : -1.0;
            float err = gs_out[0] - target;
            gs_od[0] = err;
            nn_scratch[nn_out_delta_off + s * out_dim] = err;
            nn_scratch[nn_sample_loss_off + s] = 0.5 * err * err;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    float delta = 0.0;
    uint w3 = nn_w3_off(in_dim);
    for (uint o = 0; o < out_dim; ++o) {
        delta += nn_state[w3 + o * nn_hidden + j] * gs_od[o];
    }
    float pre = delta * (1.0 - a2 * a2);
    gs_h2p[j] = pre;
    nn_scratch[nn_h2_pre_off + s * nn_hidden + j] = pre;
    GroupMemoryBarrierWithGroupSync();

    delta = 0.0;
    uint w2 = nn_w2_off(in_dim);
    for (uint k = 0; k < nn_hidden; ++k) {
        delta += nn_state[w2 + k * nn_hidden + j] * gs_h2p[k];
    }
    nn_scratch[nn_h1_pre_off + s * nn_hidden + j] = delta * (1.0 - a1 * a1);
}
