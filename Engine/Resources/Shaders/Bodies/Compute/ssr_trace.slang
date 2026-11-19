bool project_to_screen(float3 view_pos, out float2 uv) {
    float4 clip = mul(camera_ubo.proj, float4(view_pos, 1.0));
    uv = float2(0.0, 0.0);
    if (clip.w <= 1e-4) {
        return false;
    }
    uv = clip.xy / clip.w * 0.5 + 0.5;
    return all(uv >= 0.0) && all(uv <= 1.0);
}

float depth_gap(float3 sample_pos, float2 uv) {
    int2 pixel = int2(uv * float2(pc.extent));
    float depth = scene_depth(pixel);
    if (depth <= 0.0) {
        return -1e30;
    }
    return view_position_at(uv, depth).z - sample_pos.z;
}

void ssr_trace(uint3 dispatch_id) {
    int2 pixel = int2(dispatch_id.xy);
    if (any(dispatch_id.xy >= pc.extent)) {
        return;
    }

    if (scene_depth(pixel) <= 0.0) {
        radiance_out[pixel] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    int quality = clamp(pc.reflection_quality, 1, 3);
    int steps = quality == 1 ? 16 : (quality == 2 ? 24 : 32);

    float3 origin = view_position(pixel);
    float3 normal = view_normal(pixel, origin);
    float3 dir = reflect(normalize(origin), normal);
    float jitter = interleaved_gradient_noise(float2(pixel));

    float previous_t = 0.0;
    float hit_t = -1.0;
    for (int s = 1; s <= steps; ++s) {
        float f = (float(s) - 1.0 + jitter) / float(steps);
        float t = pc.max_distance * max(f * f, 1e-3);
        float2 uv;
        if (!project_to_screen(origin + dir * t, uv)) {
            break;
        }
        float gap = depth_gap(origin + dir * t, uv);
        if (gap > 0.0 && gap < pc.thickness + t * 0.02) {
            float lo = previous_t;
            float hi = t;
            for (int r = 0; r < 4; ++r) {
                float mid = 0.5 * (lo + hi);
                float2 mid_uv;
                project_to_screen(origin + dir * mid, mid_uv);
                if (depth_gap(origin + dir * mid, mid_uv) > 0.0) {
                    hi = mid;
                }
                else {
                    lo = mid;
                }
            }
            hit_t = hi;
            break;
        }
        previous_t = t;
    }

    if (hit_t < 0.0) {
        radiance_out[pixel] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    float3 hit_view = origin + dir * hit_t;
    float3 hit_world = mul(camera_ubo.inv_view, float4(hit_view, 1.0)).xyz;
    float4 prev_clip = mul(camera_ubo.prev_proj, mul(camera_ubo.prev_view, float4(hit_world, 1.0)));
    if (prev_clip.w <= 1e-4) {
        radiance_out[pixel] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }
    float2 prev_uv = prev_clip.xy / prev_clip.w * 0.5 + 0.5;
    if (!screen_uv_inside(prev_uv)) {
        radiance_out[pixel] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    float3 color = history_in.SampleLevel(history_sampler, screen_target_uv(prev_uv, pc.screen_uv_scale, pc.screen_uv_max), 0).rgb;
    float2 edge = min(prev_uv, 1.0 - prev_uv);
    float edge_fade = saturate(min(edge.x, edge.y) * 10.0);
    float distance_fade = 1.0 - saturate(hit_t / pc.max_distance);
    radiance_out[pixel] = float4(color, edge_fade * distance_fade);
}
