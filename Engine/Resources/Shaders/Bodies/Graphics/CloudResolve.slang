[shader("vertex")]
screen_vs_out vs_main(uint vid : SV_VertexID) {
    return screen_pass_vertex(vid, screen_depth_near);
}

static const float traced_blend = 0.3;
static const float upscaled_blend = 0.05;

float3 world_ray_from_ndc(float2 ndc) {
    float4 near_point = mul(pc.inv_view_proj, float4(ndc, 1.0, 1.0));
    float4 far_point = mul(pc.inv_view_proj, float4(ndc, 0.0, 1.0));
    return normalize(far_point.xyz / far_point.w - near_point.xyz / near_point.w);
}

[shader("fragment")]
float4 fs_main(screen_vs_out input) : SV_Target {
    int2 pixel = int2(input.uv * float2(pc.full_extent));
    int2 offset = int2(pc.block_offset);
    int2 low_extent = max(int2(pc.full_extent) / pc.divisor, int2(1, 1));
    int2 low_pixel = min(pixel / pc.divisor, low_extent - 1);

    bool fresh = all(uint2(low_pixel) % pc.interleave_stride == pc.interleave_offset);
    bool traced = fresh && pixel.x % pc.divisor == offset.x && pixel.y % pc.divisor == offset.y;
    float4 current = traced
        ? resolve_low_in.Load(int3(low_pixel, 0))
        : resolve_low_in.SampleLevel(resolve_sampler_binding, screen_target_uv(input.uv, pc.low_uv_scale, pc.low_uv_max), 0);

    if (pc.history_valid == 0) {
        return current;
    }

    float4 neighbourhood_min = float4(65504.0, 65504.0, 65504.0, 65504.0);
    float4 neighbourhood_max = -neighbourhood_min;
    if (pc.interleave_stride == 1) {
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                float4 tap = resolve_low_in.Load(int3(clamp(low_pixel + int2(x, y), int2(0, 0), low_extent - 1), 0));
                neighbourhood_min = min(neighbourhood_min, tap);
                neighbourhood_max = max(neighbourhood_max, tap);
            }
        }
    } else {
        int2 fresh_origin = low_pixel - ((low_pixel - int2(pc.interleave_offset)) & 1);
        for (int y = 0; y <= 2; y += 2) {
            for (int x = 0; x <= 2; x += 2) {
                float4 tap = resolve_low_in.Load(int3(clamp(fresh_origin + int2(x, y), int2(0, 0), low_extent - 1), 0));
                neighbourhood_min = min(neighbourhood_min, tap);
                neighbourhood_max = max(neighbourhood_max, tap);
            }
        }
    }

    float3 view_dir = world_ray_from_ndc(screen_ndc(input.uv));
    float4 previous_clip = mul(pc.prev_view_proj, float4(view_dir, 0.0));

    if (previous_clip.w <= 1e-6) {
        return current;
    }

    float2 previous_uv = (previous_clip.xy / previous_clip.w) * 0.5 + 0.5;

    if (!screen_uv_inside(previous_uv)) {
        return current;
    }

    float4 history = resolve_history_in.SampleLevel(resolve_sampler_binding, screen_target_uv(previous_uv, pc.history_uv_scale, pc.history_uv_max), 0);

    if (current.a >= 1.0 && neighbourhood_min.a >= 1.0) {
        return current;
    }

    history = clamp(history, neighbourhood_min, neighbourhood_max);
    return lerp(history, current, traced ? traced_blend : upscaled_blend);
}
