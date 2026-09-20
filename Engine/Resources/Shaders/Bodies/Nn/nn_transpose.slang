void nn_transpose(uint3 dispatch_id) {
    uint p = dispatch_id.x;
    if (p >= pc.param_count) {
        return;
    }
    nn_store_transposed(p, nn_state[p], pc.in_dim, pc.out_dim, pc.param_count);
}
