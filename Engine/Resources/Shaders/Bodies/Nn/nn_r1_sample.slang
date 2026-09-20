groupshared float gs_a1[nn_hidden];
groupshared float gs_a2[nn_hidden];
groupshared float gs_dz2[nn_hidden];
groupshared float gs_dz1[nn_hidden];
groupshared float gs_g[nn_max_in];
groupshared float gs_da1_bar[nn_hidden];
groupshared float gs_z2_bar[nn_hidden];

void nn_r1_sample(uint3 group_id, uint3 group_thread_id) {
    uint s = group_id.x;
    uint j = group_thread_id.x;
    if (s >= pc.n_real || !nn_update_applies()) {
        return;
    }
    uint sample = pc.sample_base + s;
    uint in_dim = pc.in_dim;
    uint w1 = nn_w1_off(in_dim);
    uint w2 = nn_w2_off(in_dim);
    uint w3 = nn_w3_off(in_dim);
    uint row = nn_r1_off + s * nn_r1_stride;

    float a1 = nn_scratch[nn_h1_off + sample * nn_hidden + j];
    float a2 = nn_scratch[nn_h2_off + sample * nn_hidden + j];
    float s1 = 1.0 - a1 * a1;
    float s2 = 1.0 - a2 * a2;
    float w3j = nn_state[w3 + j];
    float dz2 = w3j * s2;
    gs_a1[j] = a1;
    gs_a2[j] = a2;
    gs_dz2[j] = dz2;
    GroupMemoryBarrierWithGroupSync();

    float da1 = 0.0;
    for (uint k = 0; k < nn_hidden; ++k) {
        da1 += nn_state[w2 + k * nn_hidden + j] * gs_dz2[k];
    }
    float dz1 = da1 * s1;
    gs_dz1[j] = dz1;
    GroupMemoryBarrierWithGroupSync();

    if (j < in_dim) {
        float g = 0.0;
        for (uint h = 0; h < nn_hidden; ++h) {
            g += nn_state[w1 + h * in_dim + j] * gs_dz1[h];
        }
        gs_g[j] = g;
    }
    GroupMemoryBarrierWithGroupSync();

    if (j == 0) {
        float penalty = 0.0;
        for (uint d = 0; d < in_dim; ++d) {
            penalty += gs_g[d] * gs_g[d];
        }
        nn_scratch[nn_r1_penalty_off + s] = 0.5 * penalty;
    }

    float dz1_bar = 0.0;
    uint w1_row = w1 + j * in_dim;
    for (uint d = 0; d < in_dim; ++d) {
        dz1_bar += nn_state[w1_row + d] * gs_g[d];
    }
    float da1_bar = dz1_bar * s1;
    float s1_bar = dz1_bar * da1;
    float a1_bar = s1_bar * (-2.0 * a1);
    gs_da1_bar[j] = da1_bar;
    GroupMemoryBarrierWithGroupSync();

    float dz2_bar = 0.0;
    uint w2_row = w2 + j * nn_hidden;
    for (uint h = 0; h < nn_hidden; ++h) {
        dz2_bar += nn_state[w2_row + h] * gs_da1_bar[h];
    }
    float s2_bar = dz2_bar * w3j;
    float a2_bar = s2_bar * (-2.0 * a2);
    float z2_bar = a2_bar * s2;
    gs_z2_bar[j] = z2_bar;
    GroupMemoryBarrierWithGroupSync();

    float a1_bar_fwd = 0.0;
    for (uint k = 0; k < nn_hidden; ++k) {
        a1_bar_fwd += nn_state[w2 + k * nn_hidden + j] * gs_z2_bar[k];
    }
    float zb1 = (a1_bar + a1_bar_fwd) * s1;

    if (j < in_dim) {
        nn_scratch[row + nn_r1_g + j] = gs_g[j];
    }
    nn_scratch[row + nn_r1_dz1 + j] = dz1;
    nn_scratch[row + nn_r1_zb1 + j] = zb1;
    nn_scratch[row + nn_r1_dz2 + j] = dz2;
    nn_scratch[row + nn_r1_da1_bar + j] = da1_bar;
    nn_scratch[row + nn_r1_z2_bar + j] = z2_bar;
    nn_scratch[row + nn_r1_dz2_bar + j] = dz2_bar * s2;
}
