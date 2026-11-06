public static const float cloud_pi = 3.14159265358979;

public float remap(float value, float old_lo, float old_hi, float new_lo, float new_hi) {
    return new_lo + (value - old_lo) * (new_hi - new_lo) / max(old_hi - old_lo, 1e-6);
}

public uint3 pcg3d(uint3 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v ^= v >> 16u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    return v;
}

public float3 hash33(float3 p) {
    return float3(pcg3d(uint3(int3(floor(p))))) * (1.0 / 4294967296.0);
}

public float worley_3d(float3 p, float tiling) {
    float3 id = floor(p);
    float3 f = frac(p);

    float min_dist_sq = 1.0;
    for (int z = -1; z <= 1; ++z) {
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                float3 offset = float3(x, y, z);
                float3 cell = id + offset;
                cell = cell - floor(cell / tiling) * tiling;
                float3 jitter = hash33(cell);
                float3 r = offset + jitter - f;
                min_dist_sq = min(min_dist_sq, dot(r, r));
            }
        }
    }
    return 1.0 - sqrt(min_dist_sq);
}

public float perlin_grad_3d(float3 p, float tiling) {
    float3 id = floor(p);
    float3 f = frac(p);
    float3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    float result = 0.0;
    for (int z = 0; z <= 1; ++z) {
        for (int y = 0; y <= 1; ++y) {
            for (int x = 0; x <= 1; ++x) {
                float3 corner = float3(x, y, z);
                float3 cell = id + corner;
                cell = cell - floor(cell / tiling) * tiling;
                float3 grad = hash33(cell) * 2.0 - 1.0;
                float3 d = f - corner;
                float dot_val = dot(grad, d);
                float3 weights = lerp(1.0 - u, u, corner);
                result += dot_val * weights.x * weights.y * weights.z;
            }
        }
    }
    return result * 0.5 + 0.5;
}

public float worley_fbm(float3 p, float tiling) {
    return worley_3d(p, tiling) * 0.625
         + worley_3d(p * 2.0, tiling * 2.0) * 0.25
         + worley_3d(p * 4.0, tiling * 4.0) * 0.125;
}

public float perlin_worley(float3 p, float tiling) {
    float perlin = perlin_grad_3d(p, tiling);
    float worley = worley_fbm(p, tiling);
    return saturate(remap(perlin, worley - 1.0, 1.0, 0.0, 1.0));
}

public static const float3 cloud_octave_weights = float3(0.625, 0.25, 0.125);

public float cloud_height_profile(float height_fraction, float cloud_type) {
    float base_ramp = smoothstep(0.0, lerp(0.03, 0.12, cloud_type), height_fraction);
    float top_height = lerp(0.2, 1.0, cloud_type);
    float top_falloff = saturate(remap(height_fraction, top_height * lerp(0.5, 0.65, cloud_type), top_height, 1.0, 0.0));
    return base_ramp * top_falloff;
}

public float cloud_height_density(float height_fraction, float cloud_type) {
    return lerp(1.0, saturate(remap(height_fraction, 0.0, 0.5, 0.35, 1.0)), cloud_type);
}

public float cloud_weather_coverage_gate(float weather_coverage, float global_coverage, float contrast) {
    float gate = saturate((weather_coverage - 0.5) * max(contrast, 1e-3) + 0.5);
    return saturate(global_coverage * 2.0 * gate);
}

public float cloud_base_density(float4 shape, float coverage, float cloud_type, float core, float height_fraction) {
    float fbm = dot(shape.gba, cloud_octave_weights);
    float billows = saturate(remap(shape.r, fbm - 1.0, 1.0, 0.0, 1.0));
    float profiled = billows * cloud_height_profile(height_fraction, cloud_type);
    float base = saturate(remap(profiled, 1.0 - coverage, 1.0, 0.0, 1.0)) * coverage;
    return base * cloud_height_density(height_fraction, cloud_type) * (1.0 + core);
}

public static const float cloud_detail_tiling = 4.0;
public static const float3 cloud_detail_octave_frequencies = float3(1.0, 2.0, 4.0);

public float3 cloud_detail_resolvable(float3 detail, float detail_scale, float step_length) {
    float3 cell = 1.0 / (detail_scale * cloud_detail_tiling * cloud_detail_octave_frequencies);
    float3 weight = saturate(cell / max(step_length, 1e-6) - 1.0);
    return lerp(float3(0.5, 0.5, 0.5), detail, weight);
}

public float cloud_erode(float base, float3 detail, float height_fraction, float erosion) {
    float fbm = dot(detail, cloud_octave_weights);
    float shaped = lerp(fbm, 1.0 - fbm, saturate(height_fraction * 4.0));
    return max(remap(base, shaped * erosion, 1.0, 0.0, 1.0), 0.0);
}

public float henyey_greenstein(float cos_theta, float g) {
    float g2 = g * g;
    float denom = pow(max(1.0 + g2 - 2.0 * g * cos_theta, 1e-6), 1.5);
    return (1.0 - g2) / (4.0 * cloud_pi * denom);
}

public float dual_henyey_greenstein(float cos_theta, float g_forward, float g_back, float blend) {
    return lerp(
        henyey_greenstein(cos_theta, g_back),
        henyey_greenstein(cos_theta, g_forward),
        blend
    );
}

public float cloud_multi_scatter(float light_optical_depth, float cos_theta, float g_forward, float g_back, float blend, float strength, float extinction_falloff, int octaves) {
    float result = 0.0;
    float scatter = 1.0;
    float extinction = 1.0;
    float eccentricity = 1.0;
    for (int i = 0; i < octaves; ++i) {
        float phase = dual_henyey_greenstein(cos_theta, g_forward * eccentricity, g_back * eccentricity, blend);
        result += scatter * phase * exp(-light_optical_depth * extinction);
        scatter *= strength;
        extinction *= extinction_falloff;
        eccentricity *= 0.5;
    }
    return result;
}

public float cloud_powder(float optical_depth, float strength) {
    return lerp(1.0, 1.0 - exp(-2.0 * optical_depth), strength);
}

public float interleaved_gradient_noise(float2 pixel, float frame) {
    float2 p = pixel + 5.588238 * frac(frame * 0.0618034) * 64.0;
    return frac(52.9829189 * frac(dot(p, float2(0.06711056, 0.00583715))));
}

public float2 ray_layer_intersect(float3 origin, float3 dir, float layer_radius) {
    float b = dot(origin, dir);
    float c = dot(origin, origin) - layer_radius * layer_radius;
    float discriminant = b * b - c;
    if (discriminant < 0.0) {
        return float2(-1.0, -1.0);
    }
    float sd = sqrt(discriminant);
    return float2(-b - sd, -b + sd);
}
