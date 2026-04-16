[[vk::binding(0, 0)]]
ByteAddressBuffer instance_data;

[[vk::binding(1, 0)]]
StructuredBuffer<uint> index_mapping;

[[vk::binding(2, 0)]]
RWByteAddressBuffer tlas_instances;

struct push_constants {
    uint count;
    uint instance_stride;
    uint model_matrix_offset;
};

[[vk::push_constant]]
ConstantBuffer<push_constants> pc;

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dtid : SV_DispatchThreadID) {
    uint tid = dtid.x;
    if (tid >= pc.count) {
        return;
    }

    uint src_idx = index_mapping[tid];
    uint src_byte = src_idx * pc.instance_stride + pc.model_matrix_offset;

    float4 c0 = asfloat(instance_data.Load4(src_byte));
    float4 c1 = asfloat(instance_data.Load4(src_byte + 16));
    float4 c2 = asfloat(instance_data.Load4(src_byte + 32));
    float4 c3 = asfloat(instance_data.Load4(src_byte + 48));

    float4 row0 = float4(c0.x, c1.x, c2.x, c3.x);
    float4 row1 = float4(c0.y, c1.y, c2.y, c3.y);
    float4 row2 = float4(c0.z, c1.z, c2.z, c3.z);

    uint dst_byte = tid * 64;
    tlas_instances.Store4(dst_byte, asuint(row0));
    tlas_instances.Store4(dst_byte + 16, asuint(row1));
    tlas_instances.Store4(dst_byte + 32, asuint(row2));
}
