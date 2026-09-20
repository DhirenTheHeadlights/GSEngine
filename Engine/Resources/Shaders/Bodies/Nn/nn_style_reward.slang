void nn_style_reward(uint3 dispatch_id) {
    uint t = dispatch_id.x;
    if (t >= pc.sample_count) {
        return;
    }
    float logit = nn_scratch[nn_out_off + (pc.sample_base + t) * pc.out_dim];
    float shaped = 1.0 - 0.25 * (logit - 1.0) * (logit - 1.0);
    float r = max(0.0, shaped);
    nn_scratch[nn_sample_loss_off + t] = r;
    uint index = pc.target_offset + t;
    nn_inputs[index] = pc.w_task * nn_inputs[index] + pc.w_style * r;
}
