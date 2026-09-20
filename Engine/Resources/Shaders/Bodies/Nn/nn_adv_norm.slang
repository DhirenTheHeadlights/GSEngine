void nn_adv_norm(uint3 dispatch_id) {
    uint t = dispatch_id.x;
    if (t >= pc.sample_count) {
        return;
    }
    float mean = nn_scratch[nn_scalars_off + nn_scalar_adv_mean];
    float std = nn_scratch[nn_scalars_off + nn_scalar_adv_std];
    uint index = pc.adv_offset + t;
    nn_inputs[index] = (nn_inputs[index] - mean) / std;
}
