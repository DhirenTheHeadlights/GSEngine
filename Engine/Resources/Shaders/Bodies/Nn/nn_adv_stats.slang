groupshared float gs_adv[nn_reduce_width];

void nn_adv_stats(uint3 dispatch_id) {
    uint j = dispatch_id.x;
    uint n = pc.sample_count;
    float mean = 0.0;
    for (uint s0 = 0; s0 < n; s0 += nn_reduce_width) {
        uint s = s0 + j;
        gs_adv[j] = s < n ? nn_inputs[pc.adv_offset + s] : 0.0;
        GroupMemoryBarrierWithGroupSync();
        if (j == 0) {
            uint count = min(nn_reduce_width, n - s0);
            for (uint k = 0; k < count; ++k) {
                mean += gs_adv[k];
            }
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (j == 0) {
        mean /= float(n);
        gs_adv[0] = mean;
    }
    GroupMemoryBarrierWithGroupSync();
    mean = gs_adv[0];
    GroupMemoryBarrierWithGroupSync();
    float var = 0.0;
    for (uint s0 = 0; s0 < n; s0 += nn_reduce_width) {
        uint s = s0 + j;
        gs_adv[j] = s < n ? nn_inputs[pc.adv_offset + s] : 0.0;
        GroupMemoryBarrierWithGroupSync();
        if (j == 0) {
            uint count = min(nn_reduce_width, n - s0);
            for (uint k = 0; k < count; ++k) {
                float diff = gs_adv[k] - mean;
                var += diff * diff;
            }
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (j != 0) {
        return;
    }
    var /= float(n);
    nn_scratch[nn_scalars_off + nn_scalar_adv_mean] = mean;
    nn_scratch[nn_scalars_off + nn_scalar_adv_std] = sqrt(var + 1e-8);
}
