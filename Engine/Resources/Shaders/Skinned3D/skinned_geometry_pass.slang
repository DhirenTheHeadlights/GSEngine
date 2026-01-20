import common;
import standard_3d;

[[vk::push_constant]]
cbuffer SkinnedPC {
    float4x4 model;
    float4x4 normal_matrix;
    uint skin_offset;
};

struct VS_OUTPUT {
    float4 clip_pos    : SV_Position;
    float3 view_pos    : TEXCOORD0;
    float3 view_normal : TEXCOORD1;
    float2 tex_coord   : TEXCOORD2;
};

[Layout(LayoutKind::Standard3D)]
[shader("vertex")]
VS_OUTPUT vs_main(
    float3 in_position   : POSITION,
    float3 in_normal     : NORMAL,
    float2 in_tex_coord  : TEXCOORD,
    uint4 in_bone_indices : BLENDINDICES,
    float4 in_bone_weights : BLENDWEIGHT
) {
    VS_OUTPUT o;

    float4x4 skin_matrix =
        skinMatrices[skin_offset + in_bone_indices.x] * in_bone_weights.x +
        skinMatrices[skin_offset + in_bone_indices.y] * in_bone_weights.y +
        skinMatrices[skin_offset + in_bone_indices.z] * in_bone_weights.z +
        skinMatrices[skin_offset + in_bone_indices.w] * in_bone_weights.w;

    float4 skinned_pos = mul(skin_matrix, float4(in_position, 1.0));
    float3 skinned_normal = normalize(mul((float3x3)skin_matrix, in_normal));

    float4 world_pos_h = mul(model, skinned_pos);
    float4 view_pos_h  = mul(view, world_pos_h);
    float4 clip_pos_h  = mul(proj, view_pos_h);

    float3 world_n = normalize(mul((float3x3)normal_matrix, skinned_normal));
    float3 view_n  = normalize(mul((float3x3)view, world_n));

    o.clip_pos    = clip_pos_h;
    o.view_pos    = view_pos_h.xyz;
    o.view_normal = view_n;
    o.tex_coord   = in_tex_coord;

    return o;
}

float2 encode_octahedron(float3 n) {
    n /= abs(n.x) + abs(n.y) + abs(n.z);
    float2 enc = n.xy;
    if (n.z < 0.0) {
        enc = (1.0 - abs(enc.yx)) * float2(
            enc.x >= 0.0 ? 1.0 : -1.0,
            enc.y >= 0.0 ? 1.0 : -1.0
        );
    }
    return enc;
}

struct FS_OUTPUT {
    float3 out_albedo : SV_Target0;
    float2 out_normal : SV_Target1;
};

[Layout(LayoutKind::Standard3D)]
[shader("fragment")]
FS_OUTPUT fs_main(VS_OUTPUT input) {
    FS_OUTPUT o;

    float3 base_color = diffuseSampler.Sample(input.tex_coord).rgb;

    o.out_albedo = base_color;
    o.out_normal = encode_octahedron(normalize(input.view_normal));

    return o;
}
