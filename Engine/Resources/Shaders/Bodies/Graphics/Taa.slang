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

float3 sample_history(float2 uv) {
    float2 texel = pc.inv_allocated_extent;
    float2 position = uv / texel;
    float2 center = floor(position - 0.5) + 0.5;
    float2 f = position - center;
    float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    float2 w3 = f * f * (-0.5 + 0.5 * f);
    float2 w12 = w1 + w2;
    float2 uv0 = (center - 1.0) * texel;
    float2 uv3 = (center + 2.0) * texel;
    float2 uv12 = (center + w2 / w12) * texel;

    float3 sum = history_color.Sample(color_sampler, screen_target_tap(float2(uv12.x, uv0.y), pc.screen_uv_max)).rgb * (w12.x * w0.y);
    sum += history_color.Sample(color_sampler, screen_target_tap(float2(uv0.x, uv12.y), pc.screen_uv_max)).rgb * (w0.x * w12.y);
    sum += history_color.Sample(color_sampler, screen_target_tap(uv12, pc.screen_uv_max)).rgb * (w12.x * w12.y);
    sum += history_color.Sample(color_sampler, screen_target_tap(float2(uv3.x, uv12.y), pc.screen_uv_max)).rgb * (w3.x * w12.y);
    sum += history_color.Sample(color_sampler, screen_target_tap(float2(uv12.x, uv3.y), pc.screen_uv_max)).rgb * (w12.x * w3.y);
    float weight = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
    return max(sum / weight, 0.0);
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
    float velocity_length = dot(velocity, velocity);
    float3 nbh_min = current;
    float3 nbh_max = current;
    float3 moment1 = current;
    float3 moment2 = current * current;
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
            moment1 += s;
            moment2 += s * s;
            float2 v = velocity_color.Sample(color_sampler, offset_uv).rg;
            float l = dot(v, v);
            if (l > velocity_length) {
                velocity = v;
                velocity_length = l;
            }
        }
    }

    float2 prev_uv = input.uv - velocity;

    if (!screen_uv_inside(prev_uv)) {
        FS_OUT o;
        o.history_out = float4(current_rgb, 1.0);
        o.color_out   = float4(current_rgb, 1.0);
        return o;
    }

    float3 mean = moment1 / 9.0;
    float3 sigma = sqrt(max(moment2 / 9.0 - mean * mean, 0.0));
    float3 clip_min = max(nbh_min, mean - sigma);
    float3 clip_max = min(nbh_max, mean + sigma);

    float3 history = rgb_to_ycocg(sample_history(screen_target_uv(prev_uv, pc.screen_uv_scale, pc.screen_uv_max)));
    float3 clip_center = 0.5 * (clip_max + clip_min);
    float3 clip_extent = max(0.5 * (clip_max - clip_min), 1e-5);
    float3 offset = history - clip_center;
    float3 ratio = abs(offset / clip_extent);
    float scale = max(ratio.x, max(ratio.y, ratio.z));
    if (scale > 1.0) {
        history = clip_center + offset / scale;
    }

    float motion = saturate(length(velocity * pc.active_extent) / pc.motion_full_pixels);
    float3 blended = lerp(history, current, lerp(pc.blend_alpha, pc.motion_blend_alpha, motion));
    float4 result = float4(ycocg_to_rgb(blended), 1.0);

    FS_OUT o;
    o.history_out = result;
    o.color_out   = result;
    return o;
}
