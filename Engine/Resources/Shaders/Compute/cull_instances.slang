[[vk::binding(0, 0)]]
cbuffer FrustumUBO {
    float4 planes[6];
};

struct InstanceData {
    float4x4 model_matrix;
    float4x4 normal_matrix;
    uint skin_offset;
    uint joint_count;
    uint _pad0;
    uint _pad1;
};

[[vk::binding(1, 0)]]
StructuredBuffer<InstanceData> instances;

struct BatchInfo {
    uint first_instance;
    uint instance_count;
    float3 aabb_min;
    float3 aabb_max;
};

[[vk::binding(2, 0)]]
StructuredBuffer<BatchInfo> batches;

struct DrawIndexedIndirectCommand {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

[[vk::binding(3, 0)]]
RWStructuredBuffer<DrawIndexedIndirectCommand> indirectCommands;

[[vk::push_constant]]
cbuffer CullPC {
    uint batch_offset;
};

bool is_aabb_in_frustum(float3 aabb_min, float3 aabb_max) {
    for (uint i = 0; i < 6; ++i) {
        // Get the positive vertex (furthest point in direction of plane normal)
        float3 positive_vertex;
        positive_vertex.x = planes[i].x >= 0.0 ? aabb_max.x : aabb_min.x;
        positive_vertex.y = planes[i].y >= 0.0 ? aabb_max.y : aabb_min.y;
        positive_vertex.z = planes[i].z >= 0.0 ? aabb_max.z : aabb_min.z;

        // If the positive vertex is outside this plane, the entire AABB is outside
        float distance = dot(planes[i].xyz, positive_vertex) + planes[i].w;
        if (distance < 0.0) {
            return false;
        }
    }
    return true;
}

[numthreads(1, 1, 1)]
void cs_main(uint3 group_id : SV_GroupID) {
    uint batch_idx = group_id.x;
    BatchInfo batch = batches[batch_offset + batch_idx];

    bool batch_visible = true;

    if (batch.instance_count > 0) {
        // AABB is already in world space, so test directly
        batch_visible = is_aabb_in_frustum(batch.aabb_min, batch.aabb_max);
    }

    indirectCommands[batch_offset + batch_idx].instanceCount = batch_visible ? batch.instance_count : 0;
}
