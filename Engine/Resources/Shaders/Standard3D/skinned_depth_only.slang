import common;

[[vk::binding(0, 0)]]
cbuffer CameraUBO {
    float4x4 view;
    float4x4 proj;
};

struct InstanceData {
    float4x4 model_matrix;
    float4x4 normal_matrix;
    uint skin_offset;
    uint joint_count;
    uint _pad0;
    uint _pad1;
};

[[vk::binding(0, 1)]] StructuredBuffer<float4x4> skinMatrices;
[[vk::binding(1, 1)]] StructuredBuffer<InstanceData> instanceData;

struct VS_OUTPUT {
    precise float4 clip_pos : SV_Position;
};

[shader("vertex")]
VS_OUTPUT vs_main(
    float3 in_position : POSITION,
    float3 in_normal : NORMAL,
    float2 in_tex_coord : TEXCOORD,
    uint4 in_bone_indices : BLENDINDICES,
    float4 in_bone_weights : BLENDWEIGHT,
    uint instance_id : SV_InstanceID,
    uint base_instance : SV_StartInstanceLocation
) {
    VS_OUTPUT o;

    InstanceData inst = instanceData[base_instance + instance_id];
    uint instance_skin_offset = inst.skin_offset;

    float4x4 skin_matrix =
        skinMatrices[instance_skin_offset + in_bone_indices.x] * in_bone_weights.x +
        skinMatrices[instance_skin_offset + in_bone_indices.y] * in_bone_weights.y +
        skinMatrices[instance_skin_offset + in_bone_indices.z] * in_bone_weights.z +
        skinMatrices[instance_skin_offset + in_bone_indices.w] * in_bone_weights.w;

    float4 skinned_pos = mul(skin_matrix, float4(in_position, 1.0));
    float4 world_pos_h = mul(inst.model_matrix, skinned_pos);
    float4 view_pos_h = mul(view, world_pos_h);
    o.clip_pos = mul(proj, view_pos_h);

    return o;
}

[shader("fragment")]
void fs_main(VS_OUTPUT input) {
}
