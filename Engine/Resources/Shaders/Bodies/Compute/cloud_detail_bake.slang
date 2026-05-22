void cloud_detail_bake(uint3 dispatch_id) {
    uint3 size;
    cloud_detail_out.GetDimensions(size.x, size.y, size.z);
    if (dispatch_id.x >= size.x || dispatch_id.y >= size.y || dispatch_id.z >= size.z) {
        return;
    }

    float3 uvw = (float3(dispatch_id) + 0.5) / float3(size);
    float tiling = 8.0;
    float3 p = uvw * tiling;

    float octave_a = worley_3d(p, tiling);
    float octave_b = worley_3d(p * 2.0, tiling * 2.0);
    float octave_c = worley_3d(p * 4.0, tiling * 4.0);

    float detail = octave_a * 0.625 + octave_b * 0.25 + octave_c * 0.125;

    cloud_detail_out[dispatch_id] = float4(detail, 0.0, 0.0, 1.0);
}
