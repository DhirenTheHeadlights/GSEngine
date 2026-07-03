
void tlas_transform_update(uint3 dispatch_id) {
    uint tid = dispatch_id.x;
    if (tid >= pc.count) {
        return;
    }

    uint src_idx = index_mapping[tid];
    uint src_byte = src_idx * pc.instance_stride + pc.model_matrix_offset;

    float4 c0 = asfloat(source_instance_data.Load4(src_byte));
    float4 c1 = asfloat(source_instance_data.Load4(src_byte + 16));
    float4 c2 = asfloat(source_instance_data.Load4(src_byte + 32));
    float4 c3 = asfloat(source_instance_data.Load4(src_byte + 48));

    float4 row0 = float4(c0.x, c1.x, c2.x, c3.x);
    float4 row1 = float4(c0.y, c1.y, c2.y, c3.y);
    float4 row2 = float4(c0.z, c1.z, c2.z, c3.z);

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
