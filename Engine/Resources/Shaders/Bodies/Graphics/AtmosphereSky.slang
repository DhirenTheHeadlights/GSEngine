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
    if (ray_occluded_by_ground(radius, view_zenith_cos)) {
        return float3(0.0, 0.0, 0.0);
    }

    float2 uv = transmittance_r_mu_to_uv(radius, view_zenith_cos);
    return transmittance_in.SampleLevel(lut_sampler, uv, 0).rgb;
}

float3 sample_sky_view_at(float3 view_dir) {
    float2 uv = sky_view_dir_to_uv(view_dir);
    return sky_view_in.SampleLevel(sky_view_sampler, uv, 0).rgb;
}

float3 reconstruct_world_ray(float2 ndc) {
    float4 wn = mul(pc.inv_view_proj, float4(ndc, 0.0, 1.0));
    float4 wf = mul(pc.inv_view_proj, float4(ndc, 1.0, 1.0));
    float3 ro = wn.xyz / wn.w;
    float3 rt = wf.xyz / wf.w;
    return normalize(rt - ro);
}

float3 star_hash33(float3 p) {
    p = float3(
        dot(p, float3(127.1, 311.7, 74.7)),
        dot(p, float3(269.5, 183.3, 246.1)),
        dot(p, float3(113.5, 271.9, 124.6))
    );
    return frac(sin(p) * 43758.5453);
}

float3 star_field(float3 dir, float pixel_angle) {
    if (atmosphere_ubo.star_intensity <= 0.0) {
        return float3(0.0, 0.0, 0.0);
    }

    float density = max(atmosphere_ubo.star_density, 1.0);
    float3 p = dir * density;
    float3 cell = floor(p);
    float3 f = p - cell;

    float3 h = star_hash33(cell);
    float magnitude = pow(h.z, 14.0);
    if (magnitude < 1e-4) {
        return float3(0.0, 0.0, 0.0);
    }

    float3 centre = float3(0.25, 0.25, 0.25) + h * 0.5;
    float d = length(f - centre);

    float radius = 0.16;
    float aa = max(pixel_angle * density, 1e-4);
    float core = 1.0 - smoothstep(max(radius - aa, 0.0), radius + aa, d);

    float3 warm = float3(1.0, 0.86, 0.72);
    float3 cool = float3(0.72, 0.82, 1.0);
    float3 tint = lerp(warm, cool, h.x);

    return tint * core * magnitude * atmosphere_ubo.star_intensity;
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

    if (view_dir.y > 0.0) {
        float night = saturate(-pc.sun_direction.y / max(atmosphere_ubo.star_fade, 1e-3));
        if (night > 0.0) {
            float pixel_angle = length(fwidth(view_dir));
            total_radiance += star_field(view_dir, pixel_angle) * night * saturate(view_dir.y * 6.0);
        }
    }

    float radius = atmosphere_ubo.bottom_radius + pc.camera_altitude;
    float view_zenith_cos = view_dir.y;
    float3 sun_transmittance_to_eye = sample_transmittance_at(radius, view_zenith_cos);
    float3 sun_transmittance_to_volume = sample_transmittance_at(radius, pc.sun_direction.y);

    float mu = dot(view_dir, pc.sun_direction);

    float ph_m = mie_phase(mu, atmosphere_ubo.mie_phase_g);
    const float baseline_phase = 1.0 / (4.0 * pi);
    float ph_m_excess = max(ph_m - baseline_phase, 0.0);
    const float mie_halo_strength = 25.0;
    float mie_inscatter_length = atmosphere_ubo.mie_scattering * atmosphere_ubo.mie_scale_height * mie_halo_strength;
    total_radiance += pc.sun_irradiance * sun_transmittance_to_eye * sun_transmittance_to_volume * ph_m_excess * mie_inscatter_length;

    if (mu > pc.sun_cos_radius) {
        float sin_disk_radius_sq = max(1.0 - pc.sun_cos_radius * pc.sun_cos_radius, 1e-6);
        float r_disk_sq = saturate((1.0 - mu * mu) / sin_disk_radius_sq);
        float mu_prime = sqrt(1.0 - r_disk_sq);

        const float3 limb_u = float3(0.78, 0.88, 0.91);
        float3 limb_darkening = float3(1.0, 1.0, 1.0) - limb_u * (1.0 - mu_prime);

        total_radiance += sun_transmittance_to_eye * pc.sun_irradiance * limb_darkening * 6.0;
    }

    return float4(total_radiance, 1.0);
}
