public static const float pi = 3.14159265358979;

public struct medium_sample {
    public float3 scattering;
    public float3 extinction;
};

public struct medium_split {
    public float3 rayleigh_scattering;
    public float3 mie_scattering;
    public float3 extinction;
};

public medium_split sample_medium_split(float radius_km) {
    float altitude = max(radius_km - atmosphere_ubo.bottom_radius, 0.0);

    float rayleigh_density = exp(-altitude / atmosphere_ubo.rayleigh_scale_height);
    float mie_density = exp(-altitude / atmosphere_ubo.mie_scale_height);
    float ozone_density = saturate(1.0 - abs(altitude - atmosphere_ubo.ozone_peak_height) / atmosphere_ubo.ozone_half_width);

    float3 r_scattering = atmosphere_ubo.rayleigh_scattering * rayleigh_density;
    float m_scattering = atmosphere_ubo.mie_scattering * mie_density;
    float m_extinction = (atmosphere_ubo.mie_scattering + atmosphere_ubo.mie_absorption) * mie_density;
    float3 o_extinction = atmosphere_ubo.ozone_absorption * ozone_density;

    medium_split m;
    m.rayleigh_scattering = r_scattering;
    m.mie_scattering = float3(m_scattering, m_scattering, m_scattering);
    m.extinction = r_scattering + float3(m_extinction, m_extinction, m_extinction) + o_extinction;
    return m;
}

public medium_sample sample_medium(float radius_km) {
    medium_split s = sample_medium_split(radius_km);
    medium_sample m;
    m.scattering = s.rayleigh_scattering + s.mie_scattering;
    m.extinction = s.extinction;
    return m;
}

public float rayleigh_phase(float cos_theta) {
    return (3.0 / (16.0 * pi)) * (1.0 + cos_theta * cos_theta);
}

public float mie_phase(float cos_theta, float g) {
    float g2 = g * g;
    float numerator = (1.0 - g2);
    float denominator = pow(max(1.0 + g2 - 2.0 * g * cos_theta, 1e-6), 1.5);
    return (1.0 / (4.0 * pi)) * (numerator / denominator);
}

public float ray_to_top_distance(float radius, float view_zenith_cos) {
    float discriminant = radius * radius * (view_zenith_cos * view_zenith_cos - 1.0) + atmosphere_ubo.top_radius * atmosphere_ubo.top_radius;
    return max(-radius * view_zenith_cos + sqrt(max(discriminant, 0.0)), 0.0);
}

public float ray_to_ground_distance(float radius, float view_zenith_cos) {
    float height_term = (radius - atmosphere_ubo.bottom_radius) * (radius + atmosphere_ubo.bottom_radius);
    float discriminant = radius * radius * view_zenith_cos * view_zenith_cos - height_term;
    if (discriminant < 0.0 || view_zenith_cos >= 0.0) {
        return -1.0;
    }
    return max(height_term, 0.0) / (-radius * view_zenith_cos + sqrt(discriminant));
}

public bool ray_occluded_by_ground(float radius, float view_zenith_cos) {
    return ray_to_ground_distance(radius, view_zenith_cos) >= 0.0;
}

public void transmittance_uv_to_r_mu(float2 uv, out float radius, out float view_zenith_cos) {
    float h = sqrt(atmosphere_ubo.top_radius * atmosphere_ubo.top_radius - atmosphere_ubo.bottom_radius * atmosphere_ubo.bottom_radius);
    float rho = h * uv.y;
    radius = sqrt(rho * rho + atmosphere_ubo.bottom_radius * atmosphere_ubo.bottom_radius);

    float d_min = atmosphere_ubo.top_radius - radius;
    float d_max = rho + h;
    float d = d_min + uv.x * (d_max - d_min);

    view_zenith_cos = d == 0.0 ? 1.0 : (h * h - rho * rho - d * d) / (2.0 * radius * d);
    view_zenith_cos = clamp(view_zenith_cos, -1.0, 1.0);
}

public float2 transmittance_r_mu_to_uv(float radius, float view_zenith_cos) {
    float h = sqrt(atmosphere_ubo.top_radius * atmosphere_ubo.top_radius - atmosphere_ubo.bottom_radius * atmosphere_ubo.bottom_radius);
    float rho = sqrt(max((radius - atmosphere_ubo.bottom_radius) * (radius + atmosphere_ubo.bottom_radius), 0.0));

    float discriminant = radius * radius * (view_zenith_cos * view_zenith_cos - 1.0) + atmosphere_ubo.top_radius * atmosphere_ubo.top_radius;
    float d = max(-radius * view_zenith_cos + sqrt(max(discriminant, 0.0)), 0.0);

    float d_min = atmosphere_ubo.top_radius - radius;
    float d_max = rho + h;

    float u = (d_max - d_min) > 1e-6 ? (d - d_min) / (d_max - d_min) : 0.0;
    float v = rho / h;
    return float2(u, v);
}

public float2 multiscatter_r_mu_sun_to_uv(float radius, float mu_sun) {
    float u = clamp(mu_sun * 0.5 + 0.5, 0.0, 1.0);
    float v = clamp((radius - atmosphere_ubo.bottom_radius) / max(atmosphere_ubo.top_radius - atmosphere_ubo.bottom_radius, 1e-6), 0.0, 1.0);
    return float2(u, v);
}

public float3 sky_view_uv_to_dir(float2 uv) {
    float azimuth = uv.x * 2.0 * pi - pi;
    float zenith = uv.y * pi;
    float sin_zenith = sin(zenith);
    return float3(sin_zenith * cos(azimuth), cos(zenith), sin_zenith * sin(azimuth));
}

public float2 sky_view_dir_to_uv(float3 view_dir) {
    float zenith = acos(clamp(view_dir.y, -1.0, 1.0));
    float azimuth = atan2(view_dir.z, view_dir.x);
    float u = (azimuth + pi) / (2.0 * pi);
    float v = zenith / pi;
    return float2(u, v);
}
