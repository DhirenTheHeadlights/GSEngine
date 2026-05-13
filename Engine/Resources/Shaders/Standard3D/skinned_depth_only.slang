import common;
__exported import generated.skinned_depth_only_layout;

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

    instance_data inst = instance_data_buffer[base_instance + instance_id];
    uint instance_skin_offset = inst.skin_offset;

    float4x4 skin_matrix =
        skin_matrices[instance_skin_offset + in_bone_indices.x] * in_bone_weights.x +
        skin_matrices[instance_skin_offset + in_bone_indices.y] * in_bone_weights.y +
        skin_matrices[instance_skin_offset + in_bone_indices.z] * in_bone_weights.z +
        skin_matrices[instance_skin_offset + in_bone_indices.w] * in_bone_weights.w;

    float4 skinned_pos = mul(skin_matrix, float4(in_position, 1.0));
    float4 world_pos_h = mul(inst.model_matrix, skinned_pos);
    float4 view_pos_h = mul(view, world_pos_h);
    o.clip_pos = mul(proj, view_pos_h);

    return o;
}

[shader("fragment")]
void fs_main(VS_OUTPUT input) {
}
