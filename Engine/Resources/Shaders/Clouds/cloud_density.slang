public float cloud_height_fraction(float3 world_pos_km) {
    float altitude_km = length(world_pos_km) - atmosphere_ubo.bottom_radius;
    return saturate((altitude_km - cloud_ubo.cloud_bottom) / max(cloud_ubo.cloud_top - cloud_ubo.cloud_bottom, 1e-6));
}

public float sample_cloud_base(float3 world_pos_km, float height_fraction, float3 wind_offset) {
    float2 weather_uv = (world_pos_km.xz + wind_offset.xz) * cloud_ubo.weather_scale;
    float4 weather = cloud_weather_in.SampleLevel(noise_sampler_binding, float3(weather_uv, cloud_ubo.weather_phase), 0);

    float coverage = cloud_weather_coverage_gate(weather.r, cloud_ubo.cloud_coverage, cloud_ubo.weather_contrast);
    if (coverage <= 0.0) {
        return 0.0;
    }

    float cloud_type = lerp(cloud_ubo.cloud_type, weather.g, cloud_ubo.weather_type_influence);
    float4 shape = cloud_shape_in.SampleLevel(noise_sampler_binding, (world_pos_km + wind_offset) * cloud_ubo.shape_scale, 0);

    return cloud_base_density(shape, coverage, cloud_type, weather.b, height_fraction);
}
