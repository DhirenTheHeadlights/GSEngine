void cloud_weather_bake(uint3 dispatch_id) {
    uint3 size;
    cloud_weather_out.GetDimensions(size.x, size.y, size.z);
    if (dispatch_id.x >= size.x || dispatch_id.y >= size.y || dispatch_id.z >= size.z) {
        return;
    }

    float3 uvw = (float3(dispatch_id) + 0.5) / float3(size);

    float coverage_tiling = 3.0;
    float3 coverage_p = uvw * coverage_tiling;
    float coverage_low = perlin_worley(coverage_p, coverage_tiling);
    float coverage_mid = worley_fbm(coverage_p * 2.0, coverage_tiling * 2.0);
    float coverage_raw = coverage_low * 0.7 + coverage_mid * 0.3;
    float coverage = saturate(remap(coverage_raw, 0.48, 0.74, 0.0, 1.0));

    float type_tiling = 2.0;
    float3 type_p = uvw * type_tiling + 17.31;
    float cloud_type = saturate(perlin_grad_3d(type_p, type_tiling));

    cloud_weather_out[dispatch_id] = float4(coverage, cloud_type, 0.0, 1.0);
}
