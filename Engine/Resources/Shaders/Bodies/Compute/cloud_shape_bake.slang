void cloud_shape_bake(uint3 dispatch_id) {
    uint3 size;
    cloud_shape_out.GetDimensions(size.x, size.y, size.z);
    if (dispatch_id.x >= size.x || dispatch_id.y >= size.y || dispatch_id.z >= size.z) {
        return;
    }

    float3 uvw = (float3(dispatch_id) + 0.5) / float3(size);
    float tiling = 4.0;
    float3 p = uvw * tiling;

    float base = perlin_worley(p, tiling);
    float worley_low = worley_3d(p * 2.0, tiling * 2.0);
    float worley_mid = worley_3d(p * 4.0, tiling * 4.0);
    float worley_high = worley_3d(p * 8.0, tiling * 8.0);

    cloud_shape_out[dispatch_id] = float4(base, worley_low, worley_mid, worley_high);
}
