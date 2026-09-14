void nn_adam(uint3 dispatch_id) {
    uint p = dispatch_id.x;
    if (p >= pc.param_count || !nn_update_applies()) {
        return;
    }
    float bc1 = nn_scratch[nn_scalars_off + nn_scalar_bc1];
    float bc2 = nn_scratch[nn_scalars_off + nn_scalar_bc2];
    uint m_index = pc.param_count + p;
    uint v_index = 2 * pc.param_count + p;
    float g = nn_scratch[nn_grads_off + p] * pc.inv_n;
    float m = nn_adam_beta1 * nn_state[m_index] + (1.0 - nn_adam_beta1) * g;
    float v = nn_adam_beta2 * nn_state[v_index] + (1.0 - nn_adam_beta2) * g * g;
    nn_state[m_index] = m;
    nn_state[v_index] = v;
    float value = nn_state[p] - pc.lr * (m / bc1) / (sqrt(v / bc2) + nn_adam_eps);
    nn_state[p] = value;
    nn_store_transposed(p, value, pc.in_dim, pc.out_dim, pc.param_count);
}
