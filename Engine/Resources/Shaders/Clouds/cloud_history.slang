static const float history_subtexel_steps = 256.0;

float3 world_ray_from_ndc(float2 ndc) {
    float4 near_point = mul(camera_ubo.inv_view_proj, float4(ndc, 1.0, 1.0));
    float4 far_point = mul(camera_ubo.inv_view_proj, float4(ndc, 0.0, 1.0));
    return normalize(far_point.xyz / far_point.w - near_point.xyz / near_point.w);
}

bool reprojected_uv(float2 uv, out float2 previous_uv) {
    float3 view_dir = world_ray_from_ndc(screen_ndc(uv) - pc.jitter_ndc);
    float4 previous_clip = mul(camera_ubo.prev_proj, mul(camera_ubo.prev_view, float4(view_dir, 0.0)));
    previous_uv = (previous_clip.xy / previous_clip.w + pc.prev_jitter_ndc) * 0.5 + 0.5;
    return pc.history_valid != 0 && previous_clip.w > 1e-6 && screen_uv_inside(previous_uv);
}

float4 bilinear_history(float2 previous_uv) {
    return resolve_history_in.SampleLevel(resolve_sampler_binding, screen_target_uv(previous_uv, pc.history_uv_scale, pc.history_uv_max), 0);
}

float4 reprojected_history(float2 uv) {
    float2 previous_uv;
    if (reprojected_uv(uv, previous_uv)) {
        return bilinear_history(previous_uv);
    }
    return float4(0.0, 0.0, 0.0, cloud_skipped_transmittance);
}

float4 sharp_reprojected_history(float2 uv) {
    float2 previous_uv;
    if (!reprojected_uv(uv, previous_uv)) {
        return float4(0.0, 0.0, 0.0, cloud_skipped_transmittance);
    }

    float2 position = round(previous_uv * float2(pc.resolve_extent) * history_subtexel_steps) / history_subtexel_steps;
    float4 tap_min;
    float4 tap_max;
    float4 filtered = screen_target_catmull_rom(resolve_history_in, resolve_sampler_binding, position, pc.history_uv_scale / float2(pc.resolve_extent), pc.history_uv_max, tap_min, tap_max);

    if (!cloud_taps_traced(tap_max)) {
        return bilinear_history(previous_uv);
    }
    return clamp(filtered, tap_min, tap_max);
}
