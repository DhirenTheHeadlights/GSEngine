[shader("vertex")]
screen_vs_out vs_main(uint vid : SV_VertexID) {
    return screen_pass_vertex(vid, screen_depth_far);
}

float3 rgb_to_ycocg(float3 c) {
    return float3(
        0.25 * c.r + 0.5 * c.g + 0.25 * c.b,
        0.5  * c.r - 0.5  * c.b,
        -0.25 * c.r + 0.5 * c.g - 0.25 * c.b
    );
}

float3 ycocg_to_rgb(float3 c) {
    float y  = c.x;
    float co = c.y;
    float cg = c.z;
    return float3(
        y + co - cg,
        y + cg,
        y - co - cg
    );
}

struct FS_OUT {
    float4 history_out : SV_Target0;
    float4 color_out   : SV_Target1;
};

[shader("fragment")]
FS_OUT fs_main(screen_vs_out input) {
    float2 target_uv = screen_target_uv(input.uv, pc.screen_uv_scale, pc.screen_uv_max);
    float3 current_rgb = hdr_color.Sample(color_sampler, target_uv).rgb;
    float3 current = rgb_to_ycocg(current_rgb);

    if (pc.taa_enabled == 0) {
        FS_OUT o;
        o.history_out = float4(current_rgb, 1.0);
        o.color_out   = float4(current_rgb, 1.0);
        return o;
    }

    float2 velocity = velocity_color.Sample(color_sampler, target_uv).rg;
    float2 prev_uv = input.uv - velocity;

    if (!screen_uv_inside(prev_uv)) {
        FS_OUT o;
        o.history_out = float4(current_rgb, 1.0);
        o.color_out   = float4(current_rgb, 1.0);
        return o;
    }

    float3 nbh_min = current;
    float3 nbh_max = current;
    [unroll]
    for (int dy = -1; dy <= 1; ++dy) {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            float2 offset_uv = screen_target_tap(target_uv + float2(dx, dy) * pc.inv_allocated_extent, pc.screen_uv_max);
            float3 s = rgb_to_ycocg(hdr_color.Sample(color_sampler, offset_uv).rgb);
            nbh_min = min(nbh_min, s);
            nbh_max = max(nbh_max, s);
        }
    }

    float3 history = rgb_to_ycocg(history_color.Sample(color_sampler, screen_target_uv(prev_uv, pc.screen_uv_scale, pc.screen_uv_max)).rgb);
    history = clamp(history, nbh_min, nbh_max);

    float3 blended = lerp(history, current, pc.blend_alpha);
    float4 result = float4(ycocg_to_rgb(blended), 1.0);

    FS_OUT o;
    o.history_out = result;
    o.color_out   = result;
    return o;
}
