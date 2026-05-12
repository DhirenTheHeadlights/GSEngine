[[vk::binding(0, 0)]]
cbuffer FrustumUBO {
    float4 planes[6];
};

struct BatchInfo {
    uint first_instance;
    uint instance_count;
    float3 aabb_min;
    float3 aabb_max;
};

[[vk::binding(1, 0)]]
StructuredBuffer<BatchInfo> batches;

[[vk::binding(2, 0)]]
RWByteAddressBuffer indirectCommands;

struct push_constants {
    uint batch_offset;
    uint indirect_stride;
};

[[vk::push_constant]]
ConstantBuffer<push_constants> pc;

static const uint cullable_count_byte_offset = 4;

bool is_aabb_in_frustum(float3 aabb_min, float3 aabb_max) {
    for (uint i = 0; i < 6; ++i) {
        float3 positive_vertex;
        positive_vertex.x = planes[i].x >= 0.0 ? aabb_max.x : aabb_min.x;
        positive_vertex.y = planes[i].y >= 0.0 ? aabb_max.y : aabb_min.y;
        positive_vertex.z = planes[i].z >= 0.0 ? aabb_max.z : aabb_min.z;

        float distance = dot(planes[i].xyz, positive_vertex) + planes[i].w;
        if (distance < 0.0) {
            return false;
        }
    }
    return true;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void cs_main(uint3 group_id : SV_GroupID) {
    uint batch_idx = group_id.x;
    BatchInfo batch = batches[pc.batch_offset + batch_idx];

    bool batch_visible = true;
    if (batch.instance_count > 0) {
        batch_visible = is_aabb_in_frustum(batch.aabb_min, batch.aabb_max);
    }

    uint cullable_count = batch_visible ? batch.instance_count : 0;
    uint byte_offset = batch_idx * pc.indirect_stride + cullable_count_byte_offset;
    indirectCommands.Store(byte_offset, cullable_count);
}
