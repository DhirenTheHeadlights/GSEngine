void nn_store_transposed(uint p, float value, uint in_dim, uint out_dim, uint param_count) {
    if (p < nn_b1_off(in_dim)) {
        uint o = p / in_dim;
        uint i = p - o * in_dim;
        nn_state[nn_w1t_off(param_count) + i * nn_hidden + o] = value;
    }
    else if (p >= nn_w2_off(in_dim) && p < nn_b2_off(in_dim)) {
        uint q = p - nn_w2_off(in_dim);
        uint o = q / nn_hidden;
        uint k = q - o * nn_hidden;
        nn_state[nn_w2t_off(param_count, in_dim) + k * nn_hidden + o] = value;
    }
    else if (p >= nn_w3_off(in_dim) && p < nn_b3_off(in_dim, out_dim)) {
        uint q = p - nn_w3_off(in_dim);
        uint o = q / nn_hidden;
        uint k = q - o * nn_hidden;
        nn_state[nn_w3t_off(param_count, in_dim) + k * out_dim + o] = value;
    }
}
