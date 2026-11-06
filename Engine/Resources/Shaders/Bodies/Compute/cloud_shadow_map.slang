void cloud_shadow_map(uint3 dispatch_id) {
    uint2 size;
    cloud_shadow_out.GetDimensions(size.x, size.y);
    uint2 texel = dispatch_id.xy * pc.stride + pc.block_offset;
    if (texel.x >= size.x || texel.y >= size.y) {
        return;
    }

    float3 sun_dir = cloud_shadow_ubo.sun_direction;
    if (sun_dir.y <= sin(shadow_min_sun_elevation)) {
        cloud_shadow_out[texel] = float4(1.0, 0.0, 0.0, 0.0);
        return;
    }

    float2 uv = (float2(texel) + 0.5) / float2(size);
    float2 plane_xz = cloud_shadow_ubo.origin_km + uv * cloud_shadow_ubo.extent_km;
    float3 origin = float3(plane_xz.x, atmosphere_ubo.bottom_radius + cloud_shadow_ubo.plane_altitude, plane_xz.y);

    float t_enter = ray_layer_intersect(origin, sun_dir, atmosphere_ubo.bottom_radius + cloud_ubo.cloud_bottom).y;
    float t_exit = ray_layer_intersect(origin, sun_dir, atmosphere_ubo.bottom_radius + cloud_ubo.cloud_top).y;

    if (t_enter < 0.0 || t_exit <= t_enter) {
        cloud_shadow_out[texel] = float4(1.0, 0.0, 0.0, 0.0);
        return;
    }

    float dt = (t_exit - t_enter) / float(shadow_march_steps);
    float optical_depth = 0.0;

    for (int i = 0; i < shadow_march_steps; ++i) {
        float3 sample_pos = origin + sun_dir * (t_enter + dt * (float(i) + 0.5));
        optical_depth += sample_cloud_base(sample_pos, cloud_height_fraction(sample_pos), cloud_shadow_ubo.wind_offset) * dt;
    }

    cloud_shadow_out[texel] = float4(exp(-optical_depth * cloud_ubo.extinction), 0.0, 0.0, 0.0);
}
