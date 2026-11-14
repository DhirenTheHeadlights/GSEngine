public float cloud_height_fraction(float3 world_pos_km) {
    float altitude_km = length(world_pos_km) - atmosphere_ubo.bottom_radius;
    return saturate((altitude_km - cloud_ubo.cloud_bottom) / max(cloud_ubo.cloud_top - cloud_ubo.cloud_bottom, 1e-6));
}

public struct cloud_weather {
    public float coverage;
    public float cloud_type;
    public float core;
};

public cloud_weather sample_cloud_weather(float3 world_pos_km, float3 wind_offset) {
    float2 weather_uv = (world_pos_km.xz + wind_offset.xz) * cloud_ubo.weather_scale;
    float4 texel = cloud_weather_in.SampleLevel(noise_sampler_binding, float3(weather_uv, cloud_ubo.weather_phase), 0);

    cloud_weather weather;
    weather.coverage = cloud_weather_coverage_gate(texel.r, cloud_ubo.cloud_coverage, cloud_ubo.weather_contrast);
    weather.cloud_type = lerp(cloud_ubo.cloud_type, texel.g, cloud_ubo.weather_type_influence);
    weather.core = texel.b;
    return weather;
}

public float sample_cloud_shape(float3 world_pos_km, float height_fraction, float3 wind_offset, cloud_weather weather) {
    float billow_signal = cloud_shape_in.SampleLevel(noise_sampler_binding, (world_pos_km + wind_offset) * cloud_ubo.shape_scale, 0).r;
    return cloud_base_density(billow_signal, weather.coverage, weather.cloud_type, weather.core, height_fraction);
}

public float sample_cloud_base(float3 world_pos_km, float height_fraction, float3 wind_offset) {
    if (!cloud_layer_contains(height_fraction)) {
        return 0.0;
    }
    cloud_weather weather = sample_cloud_weather(world_pos_km, wind_offset);
    if (!cloud_base_possible(weather.coverage, weather.cloud_type, height_fraction)) {
        return 0.0;
    }
    return sample_cloud_shape(world_pos_km, height_fraction, wind_offset, weather);
}
