static const int ap_steps = 30;

float3 sample_transmittance_at(float radius, float view_zenith_cos) {
    float2 uv = transmittance_r_mu_to_uv(radius, view_zenith_cos);
    return transmittance_in.SampleLevel(uv, 0).rgb;
}

float3 sample_multiscatter_at(float radius, float mu_sun) {
    float2 uv = multiscatter_r_mu_sun_to_uv(radius, mu_sun);
    return multiscatter_in.SampleLevel(uv, 0).rgb;
}

float3 reconstruct_view_dir(float2 screen_uv) {
    float2 ndc = screen_uv * 2.0 - 1.0;
    float4 wn = mul(pc.inv_view_proj, float4(ndc, 0.0, 1.0));
    float4 wf = mul(pc.inv_view_proj, float4(ndc, 1.0, 1.0));
    float3 ro = wn.xyz / wn.w;
    float3 rt = wf.xyz / wf.w;
    return normalize(rt - ro);
}

void atmosphere_aerial_perspective(uint3 dispatch_id) {
    uint3 size;
    ap_volume_out.GetDimensions(size.x, size.y, size.z);
    if (dispatch_id.x >= size.x || dispatch_id.y >= size.y || dispatch_id.z >= size.z) {
        return;
    }

    float2 screen_uv = (float2(dispatch_id.xy) + 0.5) / float2(size.xy);
    float slice = (float(dispatch_id.z) + 0.5) / float(size.z);
    float slice_dist_km = slice * max_view_distance;

    float3 view_dir = reconstruct_view_dir(screen_uv);
    float3 atm_camera = float3(0.0, bottom_radius + pc.camera_altitude, 0.0);

    float mu_view_sun = dot(view_dir, pc.sun_direction);
    float ph_r = rayleigh_phase(mu_view_sun);
    float ph_m = mie_phase(mu_view_sun, mie_phase_g);

    float dt = slice_dist_km / float(ap_steps);
    float3 luminance = float3(0.0, 0.0, 0.0);
    float3 view_transmittance = float3(1.0, 1.0, 1.0);

    for (int i = 0; i < ap_steps; ++i) {
        float t = dt * (float(i) + 0.5);
        float3 p = atm_camera + view_dir * t;
        float r_p = length(p);
        float3 up = p / r_p;
        float mu_sun = dot(up, pc.sun_direction);

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

    float3 final_in_scatter = luminance * pc.sun_irradiance;
    float avg_transmittance = (view_transmittance.r + view_transmittance.g + view_transmittance.b) * (1.0 / 3.0);

    ap_volume_out[dispatch_id] = float4(final_in_scatter, avg_transmittance);
}
