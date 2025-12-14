import common;
import standard_3d;

[[vk::push_constant]]
cbuffer ModelPC {
    float4x4 model;
    float4x4 normal_matrix;
};

struct VS_OUTPUT {
    float4 clip_pos   : SV_Position;
    float3 world_pos  : TEXCOORD0;
    float3 normal     : TEXCOORD1;
    float2 tex_coord  : TEXCOORD2;
};

[Layout(LayoutKind::Standard3D)]
[shader("vertex")]
VS_OUTPUT vs_main(
    float3 in_position : POSITION,
    float3 in_normal   : NORMAL,
    float2 in_tex_coord: TEXCOORD
) {
    VS_OUTPUT output;

    float4 world_pos_h = mul(model, float4(in_position, 1.0));
    float4 view_pos_h  = mul(view, world_pos_h);
    float4 clip_pos_h  = mul(proj, view_pos_h);

    output.clip_pos   = clip_pos_h;
    output.world_pos  = world_pos_h.xyz;
    output.normal     = normalize(mul((float3x3)normal_matrix, in_normal));
    output.tex_coord  = in_tex_coord;

    return output;
}

float2 encode_octahedron(float3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    }
    return n.xy;
}

struct FS_OUTPUT {
    float3 out_albedo   : SV_Target0;
    float2 out_normal   : SV_Target1;
    float3 out_position : SV_Target2;
};

[Layout(LayoutKind::Standard3D)]
[shader("fragment")]
FS_OUTPUT fs_main(VS_OUTPUT input) {
    FS_OUTPUT output;

    float3 albedo   = diffuseSampler.Sample(input.tex_coord).rgb;
    float3 n_world  = normalize(input.normal);
    float2 enc_n    = encode_octahedron(n_world);

    output.out_albedo   = albedo;
    output.out_normal   = enc_n;
    output.out_position = input.world_pos;

    return output;
}
