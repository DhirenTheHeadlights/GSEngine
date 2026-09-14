groupshared float gs_loss[nn_reduce_width];
groupshared float gs_out[nn_reduce_width];

void nn_reduce_loss(uint3 dispatch_id) {
    uint j = dispatch_id.x;
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
    if (pc.mode == 2) {
        uint n_fake = n - pc.n_real;
        nn_scratch[nn_scalars_off + nn_scalar_mean_real] = pc.n_real > 0 ? sum_real / float(pc.n_real) : 0.0;
        nn_scratch[nn_scalars_off + nn_scalar_mean_fake] = n_fake > 0 ? sum_fake / float(n_fake) : 0.0;
    }
    if (pc.mode > 2) {
        return;
    }

    bool apply = pc.mode != 2 || mean >= pc.gate_loss;
    nn_scratch[nn_scalars_off + nn_scalar_apply] = apply ? 1.0 : 0.0;
    if (!apply) {
        nn_scratch[nn_scalars_off + nn_scalar_r1] = 0.0;
        return;
    }
    uint step_index = nn_step_off(pc.param_count);
    float step = nn_state[step_index] + 1.0;
    nn_state[step_index] = step;
    nn_scratch[nn_scalars_off + nn_scalar_bc1] = 1.0 - pow(nn_adam_beta1, step);
    nn_scratch[nn_scalars_off + nn_scalar_bc2] = 1.0 - pow(nn_adam_beta2, step);
}
