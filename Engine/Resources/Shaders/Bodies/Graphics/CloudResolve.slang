[shader("vertex")]
screen_vs_out vs_main(uint vid : SV_VertexID) {
    return screen_pass_vertex(vid, screen_depth_near);
}

static const float traced_blend = 0.3;
static const float shared_blend = 0.2;

float4 moved_with_neighbours(float4 history, uint2 pixel) {
    float2 position = (float2(pixel) - float2(pc.sample_offset)) / float(pc.block_size);
    float2 uv = (position + 0.5) / float2(pc.trace_extent);
    float4 delta = resolve_delta_in.SampleLevel(resolve_sampler_binding, screen_target_uv(uv, pc.delta_uv_scale, pc.delta_uv_max), 0);
    float4 moved = history + delta * shared_blend;
    return float4(max(moved.rgb, 0.0), saturate(moved.a));
}

float4 upsampled_trace(uint2 pixel) {
    float2 position = (float2(pixel) - float2(pc.sample_offset)) / float(pc.block_size);
    float2 base = floor(position);
    float2 weight = position - base;

    float4 sum = float4(0.0, 0.0, 0.0, 0.0);
    float total = 0.0;
    for (int y = 0; y <= 1; ++y) {
        for (int x = 0; x <= 1; ++x) {
            int2 cell = clamp(int2(base) + int2(x, y), int2(0, 0), int2(pc.trace_extent) - 1);
            float4 tap = resolve_trace_in.Load(int3(cell, 0));
            float tap_weight = (x == 0 ? 1.0 - weight.x : weight.x) * (y == 0 ? 1.0 - weight.y : weight.y);
            if (cloud_sample_usable(tap)) {
                sum += tap * tap_weight;
                total += tap_weight;
            }
        }
    }

    if (total > 0.0) {
        return sum / total;
    }
    return float4(0.0, 0.0, 0.0, cloud_skipped_transmittance);
}

[shader("fragment")]
float4 fs_main(screen_vs_out input) : SV_Target {
    uint2 pixel = uint2(input.uv * float2(pc.resolve_extent));
    uint2 cell = pixel / pc.block_size;

    float4 fresh = float4(0.0, 0.0, 0.0, cloud_skipped_transmittance);
    if (all(pixel == cloud_trace_pixel(cell, pc.block_size, pc.sample_offset, pc.resolve_extent))) {
        fresh = resolve_trace_in.Load(int3(cell, 0));
    }

    float4 history = sharp_reprojected_history(input.uv);

    if (!cloud_sample_usable(history)) {
        return cloud_sample_usable(fresh) ? fresh : upsampled_trace(pixel);
    }
    if (cloud_sample_usable(fresh)) {
        return lerp(history, fresh, traced_blend);
    }
    return moved_with_neighbours(history, pixel);
}
