struct VS_OUTPUT {
    float4 clip_pos  : SV_Position;
    float2 tex_coord : TEXCOORD0;
    float4 color     : COLOR0;
    float  px_range  : TEXCOORD4;
};

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
    output.clip_pos = mul(pc.projection, float4(in_position, pc.depth, 1.0));
    output.tex_coord = in_tex_coord;
    output.color = in_color;
    output.px_range = in_px_range;
    return output;
}

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target0 {
    float3 msdf_sample = textures[NonUniformResourceIndex(pc.tex_idx)].Sample(input.tex_coord).rgb;
    float sd = median(msdf_sample.r, msdf_sample.g, msdf_sample.b);
    float screen_px_dist = max(input.px_range, 1.0) * (0.5 - sd);
    float alpha = clamp(screen_px_dist + 0.5, 0.0, 1.0);
    return float4(input.color.rgb * alpha, input.color.a * alpha);
}
