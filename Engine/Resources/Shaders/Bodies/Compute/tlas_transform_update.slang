
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

    uint dst_byte = tid * 64;
    tlas_instances.Store4(dst_byte, asuint(row0));
    tlas_instances.Store4(dst_byte + 16, asuint(row1));
    tlas_instances.Store4(dst_byte + 32, asuint(row2));
}
