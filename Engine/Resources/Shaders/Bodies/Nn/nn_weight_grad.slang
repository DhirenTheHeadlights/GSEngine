groupshared float gs_loss[nn_reduce_width];
groupshared float gs_out[nn_reduce_width];

void nn_reduce_group(uint j) {
    uint n = pc.sample_count;
    uint out_row = nn_out_off + pc.sample_base * pc.out_dim;
    float sum = 0.0;
    float sum_real = 0.0;
    float sum_fake = 0.0;
    for (uint s0 = 0; s0 < n; s0 += nn_reduce_width) {
        uint s = s0 + j;
        bool live = s < n;
        gs_loss[j] = live ? nn_scratch[nn_sample_loss_off + s] : 0.0;
        if (pc.mode == 2) {
            gs_out[j] = live ? nn_scratch[out_row + s * pc.out_dim] : 0.0;
        }
        GroupMemoryBarrierWithGroupSync();
        if (j == 0) {
            uint count = min(nn_reduce_width, n - s0);
            for (uint k = 0; k < count; ++k) {
                sum += gs_loss[k];
            }
            if (pc.mode == 2) {
                for (uint k = 0; k < count; ++k) {
                    if (s0 + k < pc.n_real) {
                        sum_real += gs_out[k];
                    }
                    else {
                        sum_fake += gs_out[k];
                    }
                }
            }
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (j != 0) {
        return;
    }
    float mean = sum * pc.inv_n;
    nn_scratch[nn_scalars_off + pc.mode] = mean;
    if (pc.mode < 2) {
        nn_scratch[nn_scalars_off + nn_scalar_loss_acc + pc.mode] += mean;
    }
    uint step_index = nn_step_off(pc.param_count);
    if (pc.mode != 2) {
        nn_state[step_index] = float(pc.adam_step);
        return;
    }
    uint n_fake = n - pc.n_real;
    nn_scratch[nn_scalars_off + nn_scalar_mean_real] = pc.n_real > 0 ? sum_real / float(pc.n_real) : 0.0;
    nn_scratch[nn_scalars_off + nn_scalar_mean_fake] = n_fake > 0 ? sum_fake / float(n_fake) : 0.0;

    bool apply = mean >= pc.gate_loss;
    nn_scratch[nn_scalars_off + nn_scalar_apply] = apply ? 1.0 : 0.0;
    if (!apply) {
        nn_scratch[nn_scalars_off + nn_scalar_r1] = 0.0;
        return;
    }
    float step = nn_state[step_index] + 1.0;
    nn_state[step_index] = step;
    nn_scratch[nn_scalars_off + nn_scalar_bc1] = 1.0 - pow(nn_adam_beta1, step);
    nn_scratch[nn_scalars_off + nn_scalar_bc2] = 1.0 - pow(nn_adam_beta2, step);
}

void nn_weight_grad(uint3 group_id, uint3 group_thread_id) {
    uint j = group_thread_id.x;
    uint reduce_group = (pc.param_count + nn_reduce_width - 1) / nn_reduce_width;
    if (group_id.x == reduce_group) {
        nn_reduce_group(j);
        return;
    }
    uint p = group_id.x * nn_reduce_width + j;
    if (p >= pc.param_count) {
        return;
    }
    uint in_dim = pc.in_dim;
    uint out_dim = pc.out_dim;
    uint n = pc.sample_count;
    uint base = pc.sample_base;
    float sum = 0.0;

    if (p < nn_b1_off(in_dim)) {
        uint o = p / in_dim;
        uint i = p - o * in_dim;
        for (uint s = 0; s < n; ++s) {
            sum += nn_scratch[nn_h1_pre_off + s * nn_hidden + o] * nn_inputs[pc.input_offset + (base + s) * in_dim + i];
        }
    }
    else if (p < nn_w2_off(in_dim)) {
        uint o = p - nn_b1_off(in_dim);
        for (uint s = 0; s < n; ++s) {
            sum += nn_scratch[nn_h1_pre_off + s * nn_hidden + o];
        }
    }
    else if (p < nn_b2_off(in_dim)) {
        uint q = p - nn_w2_off(in_dim);
        uint o = q / nn_hidden;
        uint i = q - o * nn_hidden;
        for (uint s = 0; s < n; ++s) {
            sum += nn_scratch[nn_h2_pre_off + s * nn_hidden + o] * nn_scratch[nn_h1_off + (base + s) * nn_hidden + i];
        }
    }
    else if (p < nn_w3_off(in_dim)) {
        uint o = p - nn_b2_off(in_dim);
        for (uint s = 0; s < n; ++s) {
            sum += nn_scratch[nn_h2_pre_off + s * nn_hidden + o];
        }
    }
    else if (p < nn_b3_off(in_dim, out_dim)) {
        uint q = p - nn_w3_off(in_dim);
        uint o = q / nn_hidden;
        uint i = q - o * nn_hidden;
        for (uint s = 0; s < n; ++s) {
            sum += nn_scratch[nn_out_delta_off + s * out_dim + o] * nn_scratch[nn_h2_off + (base + s) * nn_hidden + i];
        }
    }
    else if (p < nn_ls_off(in_dim, out_dim)) {
        uint o = p - nn_b3_off(in_dim, out_dim);
        for (uint s = 0; s < n; ++s) {
            sum += nn_scratch[nn_out_delta_off + s * out_dim + o];
        }
    }
    else {
        uint i = p - nn_ls_off(in_dim, out_dim);
        for (uint s = 0; s < n; ++s) {
            sum += nn_scratch[nn_ls_terms_off + s * out_dim + i];
        }
    }

    if (pc.mode == 2) {
        nn_scratch[nn_grads_off + p] = sum;
        return;
    }

    float step = float(pc.adam_step);
    float bc1 = 1.0 - pow(nn_adam_beta1, step);
    float bc2 = 1.0 - pow(nn_adam_beta2, step);
    uint m_index = pc.param_count + p;
    uint v_index = 2 * pc.param_count + p;
    float g = sum * pc.inv_n;
    float m = nn_adam_beta1 * nn_state[m_index] + (1.0 - nn_adam_beta1) * g;
    float v = nn_adam_beta2 * nn_state[v_index] + (1.0 - nn_adam_beta2) * g * g;
    nn_state[m_index] = m;
    nn_state[v_index] = v;
    float value = nn_state[p] - pc.lr * (m / bc1) / (sqrt(v / bc2) + nn_adam_eps);
    nn_state[p] = value;
    nn_store_transposed(p, value, in_dim, out_dim, pc.param_count);
}
