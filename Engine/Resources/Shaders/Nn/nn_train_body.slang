static const uint nn_train_tile = 4;
static const uint nn_grad_tile = 16;
static const uint nn_grad_tile_area = nn_grad_tile * nn_grad_tile;

groupshared float gs_x[nn_train_tile * nn_max_in];
groupshared float gs_a1[nn_train_tile * nn_hidden];
groupshared float gs_a2[nn_train_tile * nn_hidden];
groupshared float gs_out[nn_train_tile * nn_max_out];
groupshared float gs_od[nn_train_tile * nn_max_out];
groupshared float gs_h2p[nn_train_tile * nn_hidden];
groupshared float gs_loss[nn_reduce_width];
groupshared float gs_d[nn_grad_tile_area];
groupshared float gs_a[nn_grad_tile_area];

uint nn_train_tiles(uint sample_count) {
    return (sample_count + nn_train_tile - 1) / nn_train_tile;
}

uint nn_grad_tiles(uint extent) {
    return (extent + nn_grad_tile - 1) / nn_grad_tile;
}

uint nn_grad_vector_count(uint out_dim) {
    return 2 * nn_hidden + 2 * out_dim;
}

uint nn_grad_groups(uint in_dim, uint out_dim) {
    uint hidden_tiles = nn_grad_tiles(nn_hidden);
    uint w1 = hidden_tiles * nn_grad_tiles(in_dim);
    uint w2 = hidden_tiles * hidden_tiles;
    uint w3 = nn_grad_tiles(out_dim) * hidden_tiles;
    uint vectors = (nn_grad_vector_count(out_dim) + nn_reduce_width - 1) / nn_reduce_width;
    return w1 + w2 + w3 + vectors + 1;
}

void nn_train_tile_net(RWStructuredBuffer<float> state, RWStructuredBuffer<float> scratch, uint tile, uint j, uint out_dim, uint param_count, uint mode, uint target_offset) {
    uint s0 = tile * nn_train_tile;
    uint live = min(nn_train_tile, pc.sample_count - s0);
    uint base = pc.sample_base + s0;
    uint in_dim = pc.in_dim;
    uint w1t = nn_w1t_off(param_count);
    uint w2t = nn_w2t_off(param_count, in_dim);
    uint w3t = nn_w3t_off(param_count, in_dim);

    if (j < in_dim) {
        for (uint t = 0; t < nn_train_tile; ++t) {
            gs_x[t * nn_max_in + j] = t < live ? nn_inputs[pc.input_offset + (base + t) * in_dim + j] : 0.0;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    float acc[nn_train_tile];
    float bias = state[nn_b1_off(in_dim) + j];
    for (uint t = 0; t < nn_train_tile; ++t) {
        acc[t] = bias;
    }
    for (uint i = 0; i < in_dim; ++i) {
        float w = state[w1t + i * nn_hidden + j];
        for (uint t = 0; t < nn_train_tile; ++t) {
            acc[t] += w * gs_x[t * nn_max_in + i];
        }
    }
    float a1[nn_train_tile];
    for (uint t = 0; t < nn_train_tile; ++t) {
        a1[t] = tanh(acc[t]);
        gs_a1[t * nn_hidden + j] = a1[t];
        if (t < live) {
            scratch[nn_h1_off + (base + t) * nn_hidden + j] = a1[t];
        }
    }
    GroupMemoryBarrierWithGroupSync();

    bias = state[nn_b2_off(in_dim) + j];
    for (uint t = 0; t < nn_train_tile; ++t) {
        acc[t] = bias;
    }
    for (uint k = 0; k < nn_hidden; ++k) {
        float w = state[w2t + k * nn_hidden + j];
        for (uint t = 0; t < nn_train_tile; ++t) {
            acc[t] += w * gs_a1[t * nn_hidden + k];
        }
    }
    float a2[nn_train_tile];
    for (uint t = 0; t < nn_train_tile; ++t) {
        a2[t] = tanh(acc[t]);
        gs_a2[t * nn_hidden + j] = a2[t];
        if (t < live) {
            scratch[nn_h2_off + (base + t) * nn_hidden + j] = a2[t];
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (j < out_dim) {
        bias = state[nn_b3_off(in_dim, out_dim) + j];
        for (uint t = 0; t < nn_train_tile; ++t) {
            acc[t] = bias;
        }
        for (uint k = 0; k < nn_hidden; ++k) {
            float w = state[w3t + k * out_dim + j];
            for (uint t = 0; t < nn_train_tile; ++t) {
                acc[t] += w * gs_a2[t * nn_hidden + k];
            }
        }
        for (uint t = 0; t < nn_train_tile; ++t) {
            gs_out[t * nn_max_out + j] = acc[t];
            if (t < live) {
                scratch[nn_out_off + (base + t) * out_dim + j] = acc[t];
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (j < live) {
        uint t = j;
        uint s = s0 + t;
        uint sample = base + t;
        uint row = t * nn_max_out;
        if (mode == 0) {
            uint ls_off = nn_ls_off(in_dim, out_dim);
            uint act_row = target_offset + sample * out_dim;
            float log_prob = 0.0;
            float entropy = 0.0;
            for (uint i = 0; i < out_dim; ++i) {
                float ls = state[ls_off + i];
                float sigma = exp(ls);
                float diff = (nn_inputs[act_row + i] - gs_out[row + i]) / sigma;
                log_prob += -0.5 * diff * diff - ls - 0.5 * nn_log_2pi;
                entropy += ls + nn_gaussian_entropy_const;
            }
            float adv = nn_inputs[pc.adv_offset + sample];
            float ratio = exp(log_prob - nn_inputs[pc.aux_offset + sample]);
            float clipped = clamp(ratio, 1.0 - pc.clip_eps, 1.0 + pc.clip_eps);
            bool not_clipped = (adv >= 0.0) ? (ratio < 1.0 + pc.clip_eps) : (ratio > 1.0 - pc.clip_eps);
            float d_logp = not_clipped ? -ratio * adv : 0.0;
            scratch[nn_sample_loss_off + s] = (not_clipped ? -ratio * adv : -clipped * adv) - pc.entropy_coeff * entropy;
            for (uint i = 0; i < out_dim; ++i) {
                float ls = state[ls_off + i];
                float sigma = exp(ls);
                float centered = nn_inputs[act_row + i] - gs_out[row + i];
                float diff = centered / sigma;
                float od = d_logp * centered / (sigma * sigma);
                gs_od[row + i] = od;
                scratch[nn_out_delta_off + s * out_dim + i] = od;
                scratch[nn_ls_terms_off + s * out_dim + i] = d_logp * (diff * diff - 1.0) - pc.entropy_coeff;
            }
        }
        else {
            float err = gs_out[row] - nn_inputs[target_offset + sample];
            float od = pc.value_coeff * err;
            gs_od[row] = od;
            scratch[nn_out_delta_off + s * out_dim] = od;
            scratch[nn_sample_loss_off + s] = 0.5 * err * err;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    float delta[nn_train_tile];
    for (uint t = 0; t < nn_train_tile; ++t) {
        delta[t] = 0.0;
    }
    uint w3 = nn_w3_off(in_dim);
    for (uint o = 0; o < out_dim; ++o) {
        float w = state[w3 + o * nn_hidden + j];
        for (uint t = 0; t < nn_train_tile; ++t) {
            delta[t] += w * gs_od[t * nn_max_out + o];
        }
    }
    for (uint t = 0; t < nn_train_tile; ++t) {
        float pre = delta[t] * (1.0 - a2[t] * a2[t]);
        gs_h2p[t * nn_hidden + j] = pre;
        if (t < live) {
            scratch[nn_h2_pre_off + (s0 + t) * nn_hidden + j] = pre;
        }
        delta[t] = 0.0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint w2 = nn_w2_off(in_dim);
    for (uint k = 0; k < nn_hidden; ++k) {
        float w = state[w2 + k * nn_hidden + j];
        for (uint t = 0; t < nn_train_tile; ++t) {
            delta[t] += w * gs_h2p[t * nn_hidden + k];
        }
    }
    for (uint t = 0; t < nn_train_tile; ++t) {
        if (t < live) {
            scratch[nn_h1_pre_off + (s0 + t) * nn_hidden + j] = delta[t] * (1.0 - a1[t] * a1[t]);
        }
    }
}

void nn_store_transposed_to(RWStructuredBuffer<float> state, uint p, float value, uint in_dim, uint out_dim, uint param_count) {
    if (p < nn_b1_off(in_dim)) {
        uint o = p / in_dim;
        uint i = p - o * in_dim;
        state[nn_w1t_off(param_count) + i * nn_hidden + o] = value;
    }
    else if (p >= nn_w2_off(in_dim) && p < nn_b2_off(in_dim)) {
        uint q = p - nn_w2_off(in_dim);
        uint o = q / nn_hidden;
        uint k = q - o * nn_hidden;
        state[nn_w2t_off(param_count, in_dim) + k * nn_hidden + o] = value;
    }
    else if (p >= nn_w3_off(in_dim) && p < nn_b3_off(in_dim, out_dim)) {
        uint q = p - nn_w3_off(in_dim);
        uint o = q / nn_hidden;
        uint k = q - o * nn_hidden;
        state[nn_w3t_off(param_count, in_dim) + k * out_dim + o] = value;
    }
}

void nn_adam_apply(RWStructuredBuffer<float> state, uint p, float sum, uint in_dim, uint out_dim, uint param_count, uint adam_step) {
    float step = float(adam_step);
    float bc1 = 1.0 - pow(nn_adam_beta1, step);
    float bc2 = 1.0 - pow(nn_adam_beta2, step);
    uint m_index = param_count + p;
    uint v_index = 2 * param_count + p;
    float g = sum * pc.inv_n;
    float m = nn_adam_beta1 * state[m_index] + (1.0 - nn_adam_beta1) * g;
    float v = nn_adam_beta2 * state[v_index] + (1.0 - nn_adam_beta2) * g * g;
    state[m_index] = m;
    state[v_index] = v;
    float value = state[p] - pc.lr * (m / bc1) / (sqrt(v / bc2) + nn_adam_eps);
    state[p] = value;
    nn_store_transposed_to(state, p, value, in_dim, out_dim, param_count);
}

void nn_reduce_group_net(RWStructuredBuffer<float> state, RWStructuredBuffer<float> scratch, uint j, uint param_count, uint mode, uint adam_step) {
    uint n = pc.sample_count;
    float sum = 0.0;
    for (uint s0 = 0; s0 < n; s0 += nn_reduce_width) {
        uint s = s0 + j;
        bool live = s < n;
        gs_loss[j] = live ? scratch[nn_sample_loss_off + s] : 0.0;
        GroupMemoryBarrierWithGroupSync();
        if (j == 0) {
            uint count = min(nn_reduce_width, n - s0);
            for (uint k = 0; k < count; ++k) {
                sum += gs_loss[k];
            }
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (j != 0) {
        return;
    }
    float mean = sum * pc.inv_n;
    scratch[nn_scalars_off + mode] = mean;
    scratch[nn_scalars_off + nn_scalar_loss_acc + mode] += mean;
    state[nn_step_off(param_count)] = float(adam_step);
}

float nn_grad_delta(RWStructuredBuffer<float> scratch, uint matrix, uint s, uint o, uint out_dim) {
    if (matrix == 0) {
        return scratch[nn_h1_pre_off + s * nn_hidden + o];
    }
    if (matrix == 1) {
        return scratch[nn_h2_pre_off + s * nn_hidden + o];
    }
    return scratch[nn_out_delta_off + s * out_dim + o];
}

float nn_grad_input(RWStructuredBuffer<float> scratch, uint matrix, uint s, uint i, uint in_dim) {
    uint sample = pc.sample_base + s;
    if (matrix == 0) {
        return nn_inputs[pc.input_offset + sample * in_dim + i];
    }
    if (matrix == 1) {
        return scratch[nn_h1_off + sample * nn_hidden + i];
    }
    return scratch[nn_h2_off + sample * nn_hidden + i];
}

void nn_grad_tile_net(RWStructuredBuffer<float> state, RWStructuredBuffer<float> scratch, uint matrix, uint tile_o, uint tile_i, uint j, uint in_dim, uint out_dim, uint param_count, uint adam_step) {
    uint rows = matrix == 2 ? out_dim : nn_hidden;
    uint cols = matrix == 0 ? in_dim : nn_hidden;
    uint p_off = matrix == 0 ? nn_w1_off(in_dim) : (matrix == 1 ? nn_w2_off(in_dim) : nn_w3_off(in_dim));
    uint to = j / nn_grad_tile;
    uint ti = j - to * nn_grad_tile;
    uint o = tile_o * nn_grad_tile + to;
    uint i = tile_i * nn_grad_tile + ti;
    uint n = pc.sample_count;
    uint load_o = tile_o * nn_grad_tile + ti;
    uint load_i = tile_i * nn_grad_tile + ti;
    float acc = 0.0;
    for (uint s0 = 0; s0 < n; s0 += nn_grad_tile) {
        uint s = s0 + to;
        bool live = s < n;
        gs_d[j] = live && load_o < rows ? nn_grad_delta(scratch, matrix, s, load_o, out_dim) : 0.0;
        gs_a[j] = live && load_i < cols ? nn_grad_input(scratch, matrix, s, load_i, in_dim) : 0.0;
        GroupMemoryBarrierWithGroupSync();
        uint count = min(nn_grad_tile, n - s0);
        for (uint k = 0; k < count; ++k) {
            acc += gs_d[k * nn_grad_tile + to] * gs_a[k * nn_grad_tile + ti];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (o < rows && i < cols) {
        nn_adam_apply(state, p_off + o * cols + i, acc, in_dim, out_dim, param_count, adam_step);
    }
}

void nn_grad_vector_net(RWStructuredBuffer<float> state, RWStructuredBuffer<float> scratch, uint v, uint in_dim, uint out_dim, uint param_count, uint adam_step) {
    uint n = pc.sample_count;
    uint p;
    float sum = 0.0;
    if (v < nn_hidden) {
        p = nn_b1_off(in_dim) + v;
        for (uint s = 0; s < n; ++s) {
            sum += scratch[nn_h1_pre_off + s * nn_hidden + v];
        }
    }
    else if (v < 2 * nn_hidden) {
        uint o = v - nn_hidden;
        p = nn_b2_off(in_dim) + o;
        for (uint s = 0; s < n; ++s) {
            sum += scratch[nn_h2_pre_off + s * nn_hidden + o];
        }
    }
    else if (v < 2 * nn_hidden + out_dim) {
        uint o = v - 2 * nn_hidden;
        p = nn_b3_off(in_dim, out_dim) + o;
        for (uint s = 0; s < n; ++s) {
            sum += scratch[nn_out_delta_off + s * out_dim + o];
        }
    }
    else {
        uint i = v - 2 * nn_hidden - out_dim;
        p = nn_ls_off(in_dim, out_dim) + i;
        for (uint s = 0; s < n; ++s) {
            sum += scratch[nn_ls_terms_off + s * out_dim + i];
        }
    }
    nn_adam_apply(state, p, sum, in_dim, out_dim, param_count, adam_step);
}

void nn_weight_grad_net(RWStructuredBuffer<float> state, RWStructuredBuffer<float> scratch, uint group, uint j, uint out_dim, uint param_count, uint mode, uint adam_step) {
    uint in_dim = pc.in_dim;
    uint hidden_tiles = nn_grad_tiles(nn_hidden);
    uint in_tiles = nn_grad_tiles(in_dim);
    uint out_tiles = nn_grad_tiles(out_dim);
    uint w1_groups = hidden_tiles * in_tiles;
    uint w2_groups = hidden_tiles * hidden_tiles;
    uint w3_groups = out_tiles * hidden_tiles;
    if (group < w1_groups) {
        nn_grad_tile_net(state, scratch, 0, group / in_tiles, group % in_tiles, j, in_dim, out_dim, param_count, adam_step);
        return;
    }
    group -= w1_groups;
    if (group < w2_groups) {
        nn_grad_tile_net(state, scratch, 1, group / hidden_tiles, group % hidden_tiles, j, in_dim, out_dim, param_count, adam_step);
        return;
    }
    group -= w2_groups;
    if (group < w3_groups) {
        nn_grad_tile_net(state, scratch, 2, group / hidden_tiles, group % hidden_tiles, j, in_dim, out_dim, param_count, adam_step);
        return;
    }
    group -= w3_groups;
    uint vector_groups = (nn_grad_vector_count(out_dim) + nn_reduce_width - 1) / nn_reduce_width;
    if (group < vector_groups) {
        uint v = group * nn_reduce_width + j;
        if (v < 2 * nn_hidden + param_count - nn_b3_off(in_dim, out_dim)) {
            nn_grad_vector_net(state, scratch, v, in_dim, out_dim, param_count, adam_step);
        }
        return;
    }
    nn_reduce_group_net(state, scratch, j, param_count, mode, adam_step);
}
