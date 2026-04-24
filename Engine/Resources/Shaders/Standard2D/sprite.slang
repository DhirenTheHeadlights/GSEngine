import common;
import standard_2d;

[[vk::push_constant]]
cbuffer PushConstants {
    float4x4 projection;
    uint tex_idx;
};

struct VS_OUTPUT {
    float4 clip_pos     : SV_Position;
    float2 tex_coord    : TEXCOORD0;
    float4 color        : COLOR0;
    float2 local_pos    : TEXCOORD1;
    float2 half_size    : TEXCOORD2;
    float  corner_radius: TEXCOORD3;
};

[Layout(LayoutKind::Standard2D)]
[shader("vertex")]
VS_OUTPUT vs_main(
    float2 in_position      : POSITION,
    float2 in_tex_coord     : TEXCOORD0,
    float4 in_color         : COLOR0,
    float2 in_local_pos     : TEXCOORD1,
    float2 in_half_size     : TEXCOORD2,
    float  in_corner_radius : TEXCOORD3,
    float  in_px_range      : TEXCOORD4
) {
    VS_OUTPUT output;
    output.clip_pos = mul(projection, float4(in_position, 0.0, 1.0));
    output.tex_coord = in_tex_coord;
    output.color = in_color;
    output.local_pos = in_local_pos;
    output.half_size = in_half_size;
    output.corner_radius = in_corner_radius;
    return output;
}

[Layout(LayoutKind::Standard2D)]
[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target0 {
    float4 col = g_textures[NonUniformResourceIndex(tex_idx)].Sample(input.tex_coord) * input.color;

    if (input.corner_radius > 0.0) {
        float2 q = abs(input.local_pos) - input.half_size + input.corner_radius;
        float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - input.corner_radius;
        float aa = smoothstep(0.0, -1.0, d);
        col *= aa;
    }

    return col;
}