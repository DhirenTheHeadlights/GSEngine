static const int sky_view_steps = 30;

float3 sample_transmittance_at(float radius, float view_zenith_cos) {
    float2 uv = transmittance_r_mu_to_uv(radius, view_zenith_cos);
    return transmittance_in.SampleLevel(lut_sampler, uv, 0).rgb;
}

float3 sample_multiscatter_at(float radius, float mu_sun) {
    float2 uv = multiscatter_r_mu_sun_to_uv(radius, mu_sun);
    return multiscatter_in.SampleLevel(lut_sampler, uv, 0).rgb;
}

float3 integrate_sky_radiance(float3 origin, float3 view_dir, float3 sun_dir) {
    float radius_origin = length(origin);
    float view_zenith_origin = dot(origin / radius_origin, view_dir);

    float t_top = ray_to_top_distance(radius_origin, view_zenith_origin);
    float t_ground = ray_to_ground_distance(radius_origin, view_zenith_origin);
    float t_max = (t_ground >= 0.0) ? t_ground : t_top;
    if (t_max <= 0.0) {
        return float3(0.0, 0.0, 0.0);
    }

    float mu_view_sun = dot(view_dir, sun_dir);
    float ph_r = rayleigh_phase(mu_view_sun);
    float ph_m = mie_phase(mu_view_sun, atmosphere_ubo.mie_phase_g);

    float dt = t_max / float(sky_view_steps);
    float3 luminance = float3(0.0, 0.0, 0.0);
    float3 view_transmittance = float3(1.0, 1.0, 1.0);

    for (int i = 0; i < sky_view_steps; ++i) {
        float t = dt * (float(i) + 0.5);
        float3 p = origin + view_dir * t;
        float r_p = length(p);
        float3 up = p / r_p;
        float mu_sun = dot(up, sun_dir);

        medium_split m = sample_medium_split(r_p);
        float3 step_transmittance = exp(-m.extinction * dt);

        float3 sun_transmittance = sample_transmittance_at(r_p, mu_sun);
        float3 multi_scatter = sample_multiscatter_at(r_p, mu_sun);

        float3 sun_in_scatter = (m.rayleigh_scattering * ph_r + m.mie_scattering * ph_m) * sun_transmittance;
        float3 ms_in_scatter = (m.rayleigh_scattering + m.mie_scattering) * multi_scatter;
        float3 in_scatter = sun_in_scatter + ms_in_scatter;

        luminance += view_transmittance * in_scatter * dt;
        view_transmittance *= step_transmittance;
    }

    if (t_ground >= 0.0) {
        float3 ground_point = origin + view_dir * t_max;
        float3 ground_normal = ground_point / length(ground_point);
        float mu_sun_ground = dot(ground_normal, sun_dir);

        float3 sun_at_ground = sample_transmittance_at(atmosphere_ubo.bottom_radius, mu_sun_ground);
        float3 sky_at_ground = sample_multiscatter_at(atmosphere_ubo.bottom_radius, mu_sun_ground);
        float3 ground_irradiance = sun_at_ground * max(mu_sun_ground, 0.0) + sky_at_ground;

        luminance += view_transmittance * atmosphere_ubo.ground_albedo * ground_irradiance * (1.0 / pi);
    }

    return luminance;
}

void atmosphere_sky_view(uint3 dispatch_id) {
    uint2 size;
    sky_view_out.GetDimensions(size.x, size.y);
    if (dispatch_id.x >= size.x || dispatch_id.y >= size.y) {
        return;
    }

    float2 uv = (float2(dispatch_id.xy) + 0.5) / float2(size);
    float3 view_dir = sky_view_uv_to_dir(uv);

    float3 origin = float3(0.0, atmosphere_ubo.bottom_radius + pc.camera_altitude, 0.0);
    float3 radiance = integrate_sky_radiance(origin, view_dir, pc.sun_direction);

    sky_view_out[dispatch_id.xy] = float4(radiance, 1.0);
}
