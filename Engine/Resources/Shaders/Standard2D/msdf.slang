import common;
import standard_2d;

[[vk::push_constant]]
cbuffer PushConstants {
    float4x4 projection;
    float2   position;
    float2   size;
    float4   color;
    float4   uv_rect;
};

struct VSIn {
    float2 position   : POSITION;
    float2 tex_coord  : TEXCOORD;
};

struct VSOut {
    float4 clip_pos   : SV_Position;
    float2 tex_coord  : TEXCOORD0;
    float4 color      : COLOR0;
};

[Layout(LayoutKind::Standard2D)]
[shader("vertex")]
VSOut vs_main(float2 in_position: POSITION, float2 in_tex_coord: TEXCOORD) {
    VSOut o;
    float2 scaled_pos = in_position * size + position;
    o.clip_pos = mul(float4(scaled_pos, 0.0, 1.0), projection);
    float2 flipped_uv = float2(in_tex_coord.x, 1.0 - in_tex_coord.y);
    o.tex_coord = uv_rect.xy + flipped_uv * uv_rect.zw;
    o.color = color;
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
