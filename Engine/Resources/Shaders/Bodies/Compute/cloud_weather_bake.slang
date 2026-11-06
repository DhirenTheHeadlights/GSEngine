void cloud_weather_bake(uint3 dispatch_id) {
    uint3 size;
    cloud_weather_out.GetDimensions(size.x, size.y, size.z);
    if (dispatch_id.x >= size.x || dispatch_id.y >= size.y || dispatch_id.z >= size.z) {
        return;
    }

    float3 uvw = (float3(dispatch_id) + 0.5) / float3(size);

    float coverage_tiling = 3.0;
    float3 coverage_p = uvw * coverage_tiling;
    float coverage_raw = perlin_worley(coverage_p, coverage_tiling) * 0.7 + worley_fbm(coverage_p * 2.0, coverage_tiling * 2.0) * 0.3;
    float coverage = saturate(remap(coverage_raw, 0.48, 0.74, 0.0, 1.0));

    float type_tiling = 2.0;
    float cloud_type = saturate(perlin_grad_3d(uvw * type_tiling + 17.31, type_tiling));

    float core_tiling = 6.0;
    float3 core_p = uvw * core_tiling + 41.7;
    float core = saturate(remap(perlin_worley(core_p, core_tiling), 0.4, 0.85, 0.0, 1.0));

    cloud_weather_out[dispatch_id] = float4(coverage, cloud_type, core * coverage, 1.0);
}
