groupshared uint s_histogram[histogram_bins];

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

void exposure_histogram(uint3 dispatch_id, uint group_index) {
    s_histogram[group_index] = 0;
    GroupMemoryBarrierWithGroupSync();

    uint2 size;
    histogram_source.GetDimensions(size.x, size.y);
    if (dispatch_id.x < size.x && dispatch_id.y < size.y) {
        float3 color = histogram_source.Load(int3(dispatch_id.xy, 0)).rgb;
        InterlockedAdd(s_histogram[luminance_bin(color)], 1);
    }
    GroupMemoryBarrierWithGroupSync();

    uint count = s_histogram[group_index];
    if (count > 0) {
        InterlockedAdd(histogram_out[group_index], count);
    }
}
