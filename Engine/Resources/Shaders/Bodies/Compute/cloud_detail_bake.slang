void cloud_detail_bake(uint3 dispatch_id) {
    uint3 size;
    cloud_detail_out.GetDimensions(size.x, size.y, size.z);
    if (dispatch_id.x >= size.x || dispatch_id.y >= size.y || dispatch_id.z >= size.z) {
        return;
    }

    float3 uvw = (float3(dispatch_id) + 0.5) / float3(size);
    float3 tiling = cloud_detail_tiling * cloud_detail_octave_frequencies;

    cloud_detail_out[dispatch_id] = float4(
        worley_3d(uvw * tiling.x, tiling.x),
        worley_3d(uvw * tiling.y, tiling.y),
        worley_3d(uvw * tiling.z, tiling.z),
        1.0
    );
}
