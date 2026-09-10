
void tlas_transform_update(uint3 dispatch_id) {
    uint tid = dispatch_id.x;
    if (tid >= pc.count) {
        return;
    }

    uint src_idx = index_mapping[tid];
    float4x4 model = source_instance_data[src_idx].model_matrix;

    float4 row0 = model[0];
    float4 row1 = model[1];
    float4 row2 = model[2];

    float3 lin0 = row0.xyz;
    float3 lin1 = row1.xyz;
    float3 lin2 = row2.xyz;
    float3 translation = float3(row0.w, row1.w, row2.w);
    float det = dot(lin0, cross(lin1, lin2));
    float lin_scale = sqrt(max(max(dot(lin0, lin0), dot(lin1, lin1)), dot(lin2, lin2)));
    bool finite = all(isfinite(row0)) && all(isfinite(row1)) && all(isfinite(row2));
    bool invertible = abs(det) > 1e-6 * lin_scale * lin_scale * lin_scale;
    bool bounded = all(abs(translation) < 1e6) && lin_scale < 1e6;

    if (!finite || !invertible || !bounded) {
        row0 = float4(1.0, 0.0, 0.0, 0.0);
        row1 = float4(0.0, 1.0, 0.0, 0.0);
        row2 = float4(0.0, 0.0, 1.0, 0.0);
    }

    uint dst_byte = tid * 64;
    tlas_instances.Store4(dst_byte, asuint(row0));
    tlas_instances.Store4(dst_byte + 16, asuint(row1));
    tlas_instances.Store4(dst_byte + 32, asuint(row2));
}
