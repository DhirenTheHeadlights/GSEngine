import common;
import standard_2d;

[[vk::push_constant]]
cbuffer PushConstants {
    float4x4 projection;
};

struct VS_OUTPUT {
    float4 clip_pos  : SV_Position;
    float2 tex_coord : TEXCOORD0;
    float4 color     : COLOR0;
};

[Layout(LayoutKind::Standard2D)]
[shader("vertex")]
VS_OUTPUT vs_main(float2 in_position : POSITION, float2 in_tex_coord : TEXCOORD0, float4 in_color : COLOR0) {
    VS_OUTPUT output;
    output.clip_pos = mul(projection, float4(in_position, 0.0, 1.0));
    output.tex_coord = in_tex_coord;
    output.color = in_color;
    return output;
}

[Layout(LayoutKind::Standard2D)]
[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target0 {
    return spriteTexture.Sample(input.tex_coord) * input.color;
}