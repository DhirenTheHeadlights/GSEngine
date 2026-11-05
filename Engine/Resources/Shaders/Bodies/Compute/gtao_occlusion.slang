static const float gtao_pi = 3.14159265;
static const float gtao_half_pi = 1.57079633;

float horizon_cos(int2 pixel, float3 center, float3 view_dir, float2 direction, float pixel_radius, int steps, float jitter) {
    float best = -1.0;
    for (int j = 0; j < steps; ++j) {
        float t = (float(j) + jitter) / float(steps);
        float2 offset = direction * max(t * t * pixel_radius, float(j + 1));
        int2 sample_pixel = pixel + int2(round(offset));
        if (any(sample_pixel < int2(0, 0)) || any(sample_pixel >= int2(pc.extent))) {
            break;
        }
        if (scene_depth(sample_pixel) <= 0.0) {
            continue;
        }
        float3 delta = view_position(sample_pixel) - center;
        float dist_sq = dot(delta, delta);
        float weight = saturate(1.0 - dist_sq / (pc.radius * pc.radius));
        float c = dot(delta * rsqrt(max(dist_sq, 1e-8)), view_dir);
        best = max(best, lerp(-1.0, c, weight));
    }
    return best;
}

void gtao_occlusion(uint3 dispatch_id) {
    int2 pixel = int2(dispatch_id.xy);
    if (any(dispatch_id.xy >= pc.extent)) {
        return;
    }

    if (scene_depth(pixel) <= 0.0) {
        occlusion_out[pixel] = float4(1.0, 1.0, 1.0, 1.0);
        return;
    }

    int quality = clamp(pc.ao_quality, 1, 4);
    int slices = quality == 1 ? 2 : (quality == 2 ? 3 : (quality == 3 ? 4 : 6));
    int steps = quality == 1 ? 4 : (quality == 2 ? 6 : 8);

    float3 center = view_position(pixel);
    float3 normal = view_normal(pixel, center);
    float3 view_dir = normalize(-center);

    float4 unit_clip = mul(camera_ubo.proj, float4(1.0, 0.0, -1.0, 1.0));
    float focal = abs(unit_clip.x / unit_clip.w);
    float pixel_radius = pc.radius * focal / max(-center.z, 1e-3) * float(pc.extent.x) * 0.5;
    if (pixel_radius < 1.0) {
        occlusion_out[pixel] = float4(1.0, 1.0, 1.0, 1.0);
        return;
    }

    float noise = interleaved_gradient_noise(float2(pixel));
    float jitter = frac(noise * 7.0 + 0.5);
    float visibility = 0.0;

    for (int s = 0; s < slices; ++s) {
        float phi = (float(s) + noise) * gtao_pi / float(slices);
        float2 direction = float2(cos(phi), sin(phi));
        float3 slice_dir = float3(direction.x, -direction.y, 0.0);
        float3 ortho_dir = normalize(slice_dir - view_dir * dot(slice_dir, view_dir));
        float3 axis = normalize(cross(slice_dir, view_dir));
        float3 projected_normal = normal - axis * dot(normal, axis);
        float projected_length = length(projected_normal) + 1e-5;
        float cos_n = clamp(dot(projected_normal, view_dir) / projected_length, -1.0, 1.0);
        float n = (dot(projected_normal, ortho_dir) >= 0.0 ? 1.0 : -1.0) * acos(cos_n);

        float h1 = acos(horizon_cos(pixel, center, view_dir, direction, pixel_radius, steps, jitter));
        float h0 = -acos(horizon_cos(pixel, center, view_dir, -direction, pixel_radius, steps, jitter));
        h0 = n + max(h0 - n, -gtao_half_pi);
        h1 = n + min(h1 - n, gtao_half_pi);

        float sin_n = sin(n);
        float arc0 = -cos(2.0 * h0 - n) + cos_n + 2.0 * h0 * sin_n;
        float arc1 = -cos(2.0 * h1 - n) + cos_n + 2.0 * h1 * sin_n;
        visibility += projected_length * 0.25 * (arc0 + arc1);
    }

    float ao = saturate(visibility / float(slices));
    occlusion_out[pixel] = float4(ao, ao, ao, 1.0);
}
