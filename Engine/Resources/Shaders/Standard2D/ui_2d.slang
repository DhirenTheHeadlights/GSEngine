import common;
import standard_2d;

[[vk::push_constant]]
cbuffer PushConstants {
    float4x4 projection;
    float2   position;
    float2   size;
    float4   color;
    float4   uv_rect;
    float    rotation;
};

struct VS_OUTPUT {
    float4 clip_pos     : SV_Position;
    float2 tex_coord    : TEXCOORD0;
    float4 color        : COLOR0;
};

[Layout(LayoutKind::Standard2D)]
[shader("vertex")]
VS_OUTPUT vs_main(float2 in_position: POSITION, float2 in_tex_coord: TEXCOORD0) {
    VS_OUTPUT output;

    float2 unrotated_pos = in_position * size + position;
    float2 pivot = position + float2(0.5, -0.5) * size;

    float c = cos(rotation);
    float s = sin(rotation);

    float2x2 rotation_matrix = { c, s, -s, c };

    float2 final_pos = mul(rotation_matrix, unrotated_pos - pivot) + pivot;

    output.clip_pos = mul(projection, float4(final_pos, 0.0, 1.0));
    output.tex_coord = uv_rect.xy + in_tex_coord * uv_rect.zw;
    output.color = color;

    return output;
}

[Layout(LayoutKind::Standard2D)]
[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target0 {
    return spriteTexture.Sample(input.tex_coord) * input.color;
}