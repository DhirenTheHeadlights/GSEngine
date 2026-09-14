groupshared float gs_x[nn_max_in];
groupshared float gs_a1[nn_hidden];
groupshared float gs_a2[nn_hidden];

void nn_forward(uint3 group_id, uint3 group_thread_id) {
    uint s = group_id.x;
    uint j = group_thread_id.x;
    uint sample = pc.sample_base + s;
    uint in_dim = pc.in_dim;
    uint out_dim = pc.out_dim;
    bool active = s < pc.sample_count;
    uint w1t = nn_w1t_off(pc.param_count);
    uint w2t = nn_w2t_off(pc.param_count, in_dim);
    uint w3t = nn_w3t_off(pc.param_count, in_dim);

    if (active && j < in_dim) {
        gs_x[j] = nn_inputs[pc.input_offset + sample * in_dim + j];
    }
    GroupMemoryBarrierWithGroupSync();

    if (active) {
        float acc = nn_state[nn_b1_off(in_dim) + j];
        for (uint i = 0; i < in_dim; ++i) {
            acc += nn_state[w1t + i * nn_hidden + j] * gs_x[i];
        }
        float a1 = tanh(acc);
        gs_a1[j] = a1;
        nn_scratch[nn_h1_off + sample * nn_hidden + j] = a1;
    }
    GroupMemoryBarrierWithGroupSync();

    if (active) {
        float acc = nn_state[nn_b2_off(in_dim) + j];
        for (uint k = 0; k < nn_hidden; ++k) {
            acc += nn_state[w2t + k * nn_hidden + j] * gs_a1[k];
        }
        float a2 = tanh(acc);
        gs_a2[j] = a2;
        nn_scratch[nn_h2_off + sample * nn_hidden + j] = a2;
    }
    GroupMemoryBarrierWithGroupSync();

    if (active && j < out_dim) {
        float acc = nn_state[nn_b3_off(in_dim, out_dim) + j];
        for (uint k = 0; k < nn_hidden; ++k) {
            acc += nn_state[w3t + k * out_dim + j] * gs_a2[k];
        }
        nn_scratch[nn_out_off + sample * out_dim + j] = acc;
    }
}
