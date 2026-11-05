groupshared Atomic<uint> s_histogram[histogram_bins];

uint luminance_bin(float3 color) {
    float luminance = dot(color, luminance_weights);
    if (!(luminance > histogram_min_luminance)) {
        return 0;
    }
    float log_min = log2(histogram_min_luminance);
    float log_max = log2(histogram_max_luminance);
    float t = saturate((log2(luminance) - log_min) / (log_max - log_min));
    return 1 + uint(t * float(histogram_bins - 2));
}

void exposure_histogram_atomic(uint3 dispatch_id, uint group_index) {
    s_histogram[group_index].store(0);
    GroupMemoryBarrierWithGroupSync();

    uint2 size = pc.active_extent;
    uint2 origin = dispatch_id.xy * histogram_tile;
    uint run_bin = 0;
    uint run_count = 0;
    for (uint y = 0; y < histogram_tile; y += histogram_sample_step) {
        for (uint x = 0; x < histogram_tile; x += histogram_sample_step) {
            uint2 pixel = origin + uint2(x, y);
            if (pixel.x >= size.x || pixel.y >= size.y) {
                continue;
            }
            uint bin = luminance_bin(histogram_source.Load(int3(pixel, 0)).rgb);
            if (run_count > 0 && bin != run_bin) {
                s_histogram[run_bin].add(run_count);
                run_count = 0;
            }
            run_bin = bin;
            ++run_count;
        }
    }
    if (run_count > 0) {
        s_histogram[run_bin].add(run_count);
    }
    GroupMemoryBarrierWithGroupSync();

    uint count = s_histogram[group_index].load();
    if (count > 0) {
        histogram_out[group_index].add(count);
    }
}
