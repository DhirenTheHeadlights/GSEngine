void cloud_shape_bake(uint3 dispatch_id) {
    uint3 size;
    cloud_shape_out.GetDimensions(size.x, size.y, size.z);
    if (dispatch_id.x >= size.x || dispatch_id.y >= size.y || dispatch_id.z >= size.z) {
        return;
    }

    float3 uvw = (float3(dispatch_id) + 0.5) / float3(size);
    float tiling = 4.0;
    float3 p = uvw * tiling;

    float low_freq = perlin_worley(p, tiling);
    float mid_freq = worley_fbm(p * 2.0, tiling * 2.0);
    float high_freq = worley_fbm(p * 4.0, tiling * 4.0);

    float fbm = mid_freq * 0.625 + high_freq * 0.25;
    float shape = remap(low_freq, fbm - 1.0, 1.0, 0.0, 1.0);

    cloud_shape_out[dispatch_id] = float4(shape, 0.0, 0.0, 1.0);
}
