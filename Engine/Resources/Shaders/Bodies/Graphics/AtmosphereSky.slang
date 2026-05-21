struct VS_OUT {
    float4 clip_pos : SV_Position;
    float2 ndc      : TEXCOORD0;
};

[shader("vertex")]
VS_OUT vs_main(uint vid : SV_VertexID) {
    float2 p = float2((vid << 1) & 2, vid & 2);
    VS_OUT o;
    o.ndc = p * 2.0 - 1.0;
    o.clip_pos = float4(o.ndc, 1.0, 1.0);
    return o;
}

float3 sample_transmittance_at(float radius, float view_zenith_cos) {
    float2 uv = transmittance_r_mu_to_uv(radius, view_zenith_cos);
    return transmittance_in.SampleLevel(uv, 0).rgb;
}

float3 sample_sky_view_at(float3 view_dir) {
    float2 uv = sky_view_dir_to_uv(view_dir);
    return sky_view_in.SampleLevel(uv, 0).rgb;
}

float3 reconstruct_world_ray(float2 ndc) {
    float4 wn = mul(pc.inv_view_proj, float4(ndc, 0.0, 1.0));
    float4 wf = mul(pc.inv_view_proj, float4(ndc, 1.0, 1.0));
    float3 ro = wn.xyz / wn.w;
    float3 rt = wf.xyz / wf.w;
    return normalize(rt - ro);
}

[shader("fragment")]
float4 fs_main(VS_OUT input) : SV_Target {
    float3 view_dir = reconstruct_world_ray(input.ndc);

    float3 lookup_dir = view_dir;
    if (lookup_dir.y < 0.0) {
        lookup_dir.y = 0.0;
        lookup_dir = normalize(lookup_dir);
    }
    float3 sky_radiance = sample_sky_view_at(lookup_dir);

    float3 total_radiance = sky_radiance * pc.sun_irradiance;

    float mu = dot(view_dir, pc.sun_direction);
    const float glare_cos_radius = 0.96593;
    if (mu > glare_cos_radius) {
        float radius = bottom_radius + pc.camera_altitude;
        float view_zenith_cos = view_dir.y;
        float3 sun_transmittance_to_eye = sample_transmittance_at(radius, view_zenith_cos);

        float disk_t = saturate((mu - pc.sun_cos_radius) / max(1.0 - pc.sun_cos_radius, 1e-6));
        float core = pow(smoothstep(0.0, 1.0, disk_t), 0.5);

        float glare_t = saturate((mu - glare_cos_radius) / max(1.0 - glare_cos_radius, 1e-6));
        float glare = pow(glare_t, 4.0);

        float3 contribution = sun_transmittance_to_eye * pc.sun_irradiance * (core * 80.0 + glare * 3.0);
        total_radiance += contribution;
    }

    return float4(total_radiance, 1.0);
}
