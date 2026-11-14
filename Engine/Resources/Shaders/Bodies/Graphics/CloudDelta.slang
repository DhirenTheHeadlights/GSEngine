[shader("vertex")]
screen_vs_out vs_main(uint vid : SV_VertexID) {
    return screen_pass_vertex(vid, screen_depth_near);
}

[shader("fragment")]
float4 fs_main(screen_vs_out input) : SV_Target {
    int2 centre = int2(input.uv * float2(pc.trace_extent));

    float4 sum = float4(0.0, 0.0, 0.0, 0.0);
    float total = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            int2 cell = centre + int2(x, y);
            if (any(cell < 0) || any(cell >= int2(pc.trace_extent))) {
                continue;
            }

            uint2 pixel = cloud_trace_pixel(uint2(cell), pc.block_size, pc.sample_offset, pc.resolve_extent);
            float4 fresh = resolve_trace_in.Load(int3(cell, 0));
            float4 history = reprojected_history((float2(pixel) + 0.5) / float2(pc.resolve_extent));

            if (cloud_sample_usable(fresh) && cloud_sample_usable(history)) {
                float weight = float((2 - abs(x)) * (2 - abs(y)));
                sum += (fresh - history) * weight;
                total += weight;
            }
        }
    }

    if (total > 0.0) {
        return sum / total;
    }
    return float4(0.0, 0.0, 0.0, 0.0);
}
