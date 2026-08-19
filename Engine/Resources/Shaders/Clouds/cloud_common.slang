public static const float cloud_pi = 3.14159265358979;

public float remap(float value, float old_lo, float old_hi, float new_lo, float new_hi) {
    return new_lo + (value - old_lo) * (new_hi - new_lo) / max(old_hi - old_lo, 1e-6);
}

public float hash13(float3 p) {
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

public float3 hash33(float3 p) {
    p = float3(
        dot(p, float3(127.1, 311.7, 74.7)),
        dot(p, float3(269.5, 183.3, 246.1)),
        dot(p, float3(113.5, 271.9, 124.6))
    );
    return frac(sin(p) * 43758.5453);
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

public float cloud_altitude_gradient(float height_fraction, float cloud_type) {
    float cumulus_lo = 0.05;
    float cumulus_hi = 0.45;
    float stratus_lo = 0.0;
    float stratus_hi = 0.15;

    float type_lo = lerp(stratus_lo, cumulus_lo, cloud_type);
    float type_hi = lerp(stratus_hi, cumulus_hi, cloud_type);

    float bottom = smoothstep(0.0, type_lo, height_fraction);
    float top = 1.0 - smoothstep(type_hi, 1.0, height_fraction);
    return bottom * top;
}

public float cloud_weather_coverage(float weather, float global_coverage, float contrast) {
    float gate = saturate((weather - 0.5) * max(contrast, 1e-3) + 0.5);
    return saturate(global_coverage * 2.0 * gate);
}

public float cloud_coverage_from_shape(float shape, float height_fraction, float cloud_type, float coverage) {
    float gradient = cloud_altitude_gradient(height_fraction, cloud_type);
    return saturate(remap(shape * gradient, 1.0 - coverage, 1.0, 0.0, 1.0));
}

public float cloud_carve_density(float base, float erosion, float detail_strength, float density_multiplier) {
    float carved = base - erosion * detail_strength * (1.0 - base);
    return saturate(carved) * density_multiplier;
}

public float henyey_greenstein(float cos_theta, float g) {
    float g2 = g * g;
    float denom = pow(max(1.0 + g2 - 2.0 * g * cos_theta, 1e-6), 1.5);
    return (1.0 - g2) / (4.0 * cloud_pi * denom);
}

public float dual_henyey_greenstein(float cos_theta, float g_forward, float g_back, float blend) {
    return lerp(
        henyey_greenstein(cos_theta, g_forward),
        henyey_greenstein(cos_theta, g_back),
        blend
    );
}

public float beer_powder(float density, float step_length) {
    float beer = exp(-density * step_length);
    float powder = 1.0 - exp(-density * step_length * 2.0);
    return beer * powder * 2.0;
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
