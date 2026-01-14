import common;
import standard_2d;

[[vk::push_constant]]
cbuffer PushConstants {
    float4x4 projection;
    float depth;
};

struct VSOut {
    float4 clip_pos  : SV_Position;
    float2 tex_coord : TEXCOORD0;
    float4 color     : COLOR0;
};

[Layout(LayoutKind::Standard2D)]
[shader("vertex")]
VSOut vs_main(float2 in_position: POSITION, float2 in_tex_coord: TEXCOORD, float4 in_color: COLOR) {
    VSOut o;
    o.clip_pos = mul(projection, float4(in_position, depth, 1.0));
    o.tex_coord = in_tex_coord;
    o.color = in_color;
    return o;
}

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

[Layout(LayoutKind::Standard2D)]
[shader("fragment")]
float4 fs_main(VSOut i) : SV_Target0 {
    float3 msdf_sample = spriteTexture.Sample(i.tex_coord).rgb;
    float signed_dist = median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
    float pw = fwidth(signed_dist);
    float alpha = smoothstep(0.5 - pw, 0.5 + pw, signed_dist);
    return float4(i.color.rgb, i.color.a * (1.0 - alpha));
}