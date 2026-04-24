import common;
import standard_2d;

[[vk::push_constant]]
cbuffer PushConstants {
    float4x4 projection;
    float depth;
    uint tex_idx;
};

struct VSOut {
    float4 clip_pos  : SV_Position;
    float2 tex_coord : TEXCOORD0;
    float4 color     : COLOR0;
    float  px_range  : TEXCOORD4;
};

[Layout(LayoutKind::Standard2D)]
[shader("vertex")]
VSOut vs_main(
    float2 in_position      : POSITION,
    float2 in_tex_coord     : TEXCOORD0,
    float4 in_color         : COLOR0,
    float2 in_local_pos     : TEXCOORD1,
    float2 in_half_size     : TEXCOORD2,
    float  in_corner_radius : TEXCOORD3,
    float  in_px_range      : TEXCOORD4
) {
    VSOut o;
    o.clip_pos = mul(projection, float4(in_position, depth, 1.0));
    o.tex_coord = in_tex_coord;
    o.color = in_color;
    o.px_range = in_px_range;
    return o;
}

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

[Layout(LayoutKind::Standard2D)]
[shader("fragment")]
float4 fs_main(VSOut i) : SV_Target0 {
    float4 raw = g_textures[1].Sample(i.tex_coord);
    return float4(raw.rgb, 1.0);
}