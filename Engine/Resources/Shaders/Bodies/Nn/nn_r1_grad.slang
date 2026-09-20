void nn_r1_grad(uint3 dispatch_id) {
    uint p = dispatch_id.x;
    if (p >= pc.param_count || !nn_update_applies()) {
        return;
    }
    uint in_dim = pc.in_dim;
    uint out_dim = pc.out_dim;
    uint n = pc.n_real;
    uint base = pc.sample_base;

    if (p == 0) {
        float penalty = 0.0;
        for (uint s = 0; s < n; ++s) {
            penalty += nn_scratch[nn_r1_penalty_off + s];
        }
        nn_scratch[nn_scalars_off + nn_scalar_r1] = n > 0 ? penalty / float(n) : 0.0;
    }

    if (p >= nn_b3_off(in_dim, out_dim)) {
        return;
    }

    float sum = 0.0;
    if (p < nn_b1_off(in_dim)) {
        uint h = p / in_dim;
        uint d = p - h * in_dim;
        for (uint s = 0; s < n; ++s) {
            uint row = nn_r1_off + s * nn_r1_stride;
            float x = nn_inputs[pc.input_offset + (base + s) * in_dim + d];
            sum += nn_scratch[row + nn_r1_dz1 + h] * nn_scratch[row + nn_r1_g + d] + nn_scratch[row + nn_r1_zb1 + h] * x;
        }
    }
    else if (p < nn_w2_off(in_dim)) {
        uint h = p - nn_b1_off(in_dim);
        for (uint s = 0; s < n; ++s) {
            sum += nn_scratch[nn_r1_off + s * nn_r1_stride + nn_r1_zb1 + h];
        }
    }
    else if (p < nn_b2_off(in_dim)) {
        uint q = p - nn_w2_off(in_dim);
        uint j = q / nn_hidden;
        uint h = q - j * nn_hidden;
        for (uint s = 0; s < n; ++s) {
            uint row = nn_r1_off + s * nn_r1_stride;
            float a1 = nn_scratch[nn_h1_off + (base + s) * nn_hidden + h];
            sum += nn_scratch[row + nn_r1_dz2 + j] * nn_scratch[row + nn_r1_da1_bar + h] + nn_scratch[row + nn_r1_z2_bar + j] * a1;
        }
    }
    else if (p < nn_w3_off(in_dim)) {
        uint j = p - nn_b2_off(in_dim);
        for (uint s = 0; s < n; ++s) {
            sum += nn_scratch[nn_r1_off + s * nn_r1_stride + nn_r1_z2_bar + j];
        }
    }
    else {
        uint j = p - nn_w3_off(in_dim);
        for (uint s = 0; s < n; ++s) {
            sum += nn_scratch[nn_r1_off + s * nn_r1_stride + nn_r1_dz2_bar + j];
        }
    }

    nn_scratch[nn_grads_off + p] += pc.r1_scale * sum;
}
