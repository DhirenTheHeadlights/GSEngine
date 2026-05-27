static const int sphere_samples = 64;
static const int ms_ray_steps = 20;
static const float two_pi = 6.28318530718;

float3 sample_transmittance(float radius, float view_zenith_cos) {
    float2 uv = transmittance_r_mu_to_uv(radius, view_zenith_cos);
    return transmittance_in.SampleLevel(uv, 0).rgb;
}

float3 fibonacci_sphere_direction(int index, int count) {
    float fi = float(index) + 0.5;
    float cos_theta = 1.0 - 2.0 * fi / float(count);
    float sin_theta = sqrt(saturate(1.0 - cos_theta * cos_theta));
    float phi = two_pi * fi * 0.61803398875;
    return float3(cos(phi) * sin_theta, cos_theta, sin(phi) * sin_theta);
}

struct ms_integrand {
    float3 luminance;
    float3 f_ms;
};

ms_integrand integrate_along_ray(float3 origin, float3 dir, float3 sun_dir) {
    float radius_origin = length(origin);
    float view_zenith_origin = dot(origin, dir) / radius_origin;

    float t_top = ray_to_top_distance(radius_origin, view_zenith_origin);
    float t_ground = ray_to_ground_distance(radius_origin, view_zenith_origin);
    float t_max = t_ground >= 0.0 ? t_ground : t_top;
    if (t_max <= 0.0) {
        ms_integrand zero;
        zero.luminance = float3(0.0, 0.0, 0.0);
        zero.f_ms = float3(0.0, 0.0, 0.0);
        return zero;
    }

    float dt = t_max / float(ms_ray_steps);
    float3 luminance = float3(0.0, 0.0, 0.0);
    float3 f_ms = float3(0.0, 0.0, 0.0);
    float3 view_transmittance = float3(1.0, 1.0, 1.0);

    for (int i = 0; i < ms_ray_steps; ++i) {
        float t = dt * (float(i) + 0.5);
        float3 p = origin + dir * t;
        float r_p = length(p);
        float3 up = p / r_p;
        float mu_sun = dot(up, sun_dir);

        medium_sample m = sample_medium(r_p);
        float3 step_transmittance = exp(-m.extinction * dt);

        float3 sun_transmittance = sample_transmittance(r_p, mu_sun);
        float3 in_scatter = m.scattering * sun_transmittance;

        luminance += view_transmittance * in_scatter * dt;
        f_ms += view_transmittance * m.scattering * dt;
        view_transmittance *= step_transmittance;
    }

    ms_integrand result;
    result.luminance = luminance;
    result.f_ms = f_ms;
    return result;
}

void atmosphere_multiscatter(uint3 dispatch_id) {
    uint2 size;
    multiscatter_out.GetDimensions(size.x, size.y);
    if (dispatch_id.x >= size.x || dispatch_id.y >= size.y) {
        return;
    }

    float2 uv = (float2(dispatch_id.xy) + 0.5) / float2(size);
    float mu_sun = uv.x * 2.0 - 1.0;
    float altitude = uv.y * (top_radius - bottom_radius);
    float radius = bottom_radius + altitude;

    float3 sun_dir = float3(sqrt(saturate(1.0 - mu_sun * mu_sun)), mu_sun, 0.0);
    float3 origin = float3(0.0, radius, 0.0);

    float3 luminance_sum = float3(0.0, 0.0, 0.0);
    float3 f_ms_sum = float3(0.0, 0.0, 0.0);

    for (int i = 0; i < sphere_samples; ++i) {
        float3 dir = fibonacci_sphere_direction(i, sphere_samples);
        ms_integrand m = integrate_along_ray(origin, dir, sun_dir);
        luminance_sum += m.luminance;
        f_ms_sum += m.f_ms;
    }

    float inv_samples = 1.0 / float(sphere_samples);
    float3 l_2 = luminance_sum * inv_samples;
    float3 f_ms = f_ms_sum * inv_samples;

    float3 multi_scattering = l_2 / max(float3(1.0, 1.0, 1.0) - f_ms, float3(1e-6, 1e-6, 1e-6));
    multiscatter_out[dispatch_id.xy] = float4(multi_scattering, 1.0);
}
