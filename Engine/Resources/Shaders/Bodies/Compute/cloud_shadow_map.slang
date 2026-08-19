static const int shadow_march_steps = 16;
static const float shadow_mean_detail_erosion = 0.5;
static const float shadow_min_sun_elevation = 0.02;

float shadow_sample_density(float3 world_pos_km, float height_fraction) {
    float2 weather_uv = (world_pos_km.xz + cloud_shadow_ubo.wind_offset.xz) * cloud_ubo.weather_scale;
    float4 weather = cloud_weather_in.SampleLevel(
        noise_sampler_binding,
        float3(weather_uv, cloud_ubo.weather_phase),
        0
    );

    float local_coverage = cloud_weather_coverage(weather.r, cloud_ubo.cloud_coverage, cloud_ubo.weather_contrast);
    float local_type = lerp(cloud_ubo.cloud_type, weather.g, cloud_ubo.weather_type_influence);

    float3 shape_uvw = (world_pos_km + cloud_shadow_ubo.wind_offset) * cloud_ubo.shape_scale;
    float shape = cloud_shape_in.SampleLevel(noise_sampler_binding, shape_uvw, 0).r;

    float base = cloud_coverage_from_shape(shape, height_fraction, local_type, local_coverage);
    if (base <= 0.0) {
        return 0.0;
    }

    return cloud_carve_density(base, shadow_mean_detail_erosion, cloud_ubo.detail_strength, cloud_ubo.density_multiplier);
}

void cloud_shadow_map(uint3 dispatch_id) {
    uint2 size;
    cloud_shadow_out.GetDimensions(size.x, size.y);
    if (dispatch_id.x >= size.x || dispatch_id.y >= size.y) {
        return;
    }

    float3 sun_dir = cloud_shadow_ubo.sun_direction;
    if (sun_dir.y <= shadow_min_sun_elevation) {
        cloud_shadow_out[dispatch_id.xy] = float4(1.0, 0.0, 0.0, 0.0);
        return;
    }

    float2 uv = (float2(dispatch_id.xy) + 0.5) / float2(size);
    float2 plane_xz = cloud_shadow_ubo.origin_km + uv * cloud_shadow_ubo.extent_km;

    float3 origin = float3(
        plane_xz.x,
        atmosphere_ubo.bottom_radius + cloud_shadow_ubo.plane_altitude,
        plane_xz.y
    );

    float inner_radius = atmosphere_ubo.bottom_radius + cloud_ubo.cloud_bottom;
    float outer_radius = atmosphere_ubo.bottom_radius + cloud_ubo.cloud_top;
    float layer_thickness = cloud_ubo.cloud_top - cloud_ubo.cloud_bottom;

    float2 t_bottom = ray_layer_intersect(origin, sun_dir, inner_radius);
    float2 t_top = ray_layer_intersect(origin, sun_dir, outer_radius);

    float t_enter = t_bottom.y;
    float t_exit = t_top.y;

    if (t_enter < 0.0 || t_exit <= t_enter) {
        cloud_shadow_out[dispatch_id.xy] = float4(1.0, 0.0, 0.0, 0.0);
        return;
    }

    float dt = (t_exit - t_enter) / float(shadow_march_steps);
    float total_density = 0.0;

    for (int i = 0; i < shadow_march_steps; ++i) {
        float t = t_enter + dt * (float(i) + 0.5);
        float3 sample_pos = origin + sun_dir * t;
        float sample_altitude_km = length(sample_pos) - atmosphere_ubo.bottom_radius;
        float h = saturate((sample_altitude_km - cloud_ubo.cloud_bottom) / max(layer_thickness, 1e-6));

        total_density += shadow_sample_density(sample_pos, h) * dt;
    }

    float transmittance = exp(-total_density * cloud_ubo.shadow_extinction);
    cloud_shadow_out[dispatch_id.xy] = float4(transmittance, 0.0, 0.0, 0.0);
}
