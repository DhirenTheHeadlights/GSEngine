struct VS_OUTPUT {
    float4 clip_pos  : SV_Position;
    float2 tex_coord : TEXCOORD0;
    float4 color     : COLOR0;
};

[shader("vertex")]
VS_OUTPUT vs_main(uint vid : SV_VertexID) {
    vertex v = vertex_buffer[vid];
    VS_OUTPUT output;
    output.clip_pos = mul(pc.projection, float4(v.position, pc.depth, 1.0));
    output.tex_coord = v.uv;
    output.color = v.color;
    return output;
}

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

float msd_alpha(uint tex_idx, float2 uv, float screen_px_range) {
    float4 s = textures[NonUniformResourceIndex(tex_idx)].Sample(textures_sampler, uv);
    float sd = median(s.r, s.g, s.b);
    return clamp(screen_px_range * (sd - 0.5) + 0.5, 0.0, 1.0);
}

float tsd_alpha(uint tex_idx, float2 uv, float screen_px_range) {
    float sd = textures[NonUniformResourceIndex(tex_idx)].Sample(textures_sampler, uv).a;
    return clamp(screen_px_range * (sd - 0.5) + 0.5, 0.0, 1.0);
}

float supersampled_msd(uint tex_idx, float2 uv, float2 dx, float2 dy, float screen_px_range) {
    float2 ox1 = dx * 0.125;
    float2 ox3 = dx * 0.375;
    float2 oy1 = dy * 0.125;
    float2 oy3 = dy * 0.375;
    float a0 = msd_alpha(tex_idx, uv - ox1 - oy3, screen_px_range);
    float a1 = msd_alpha(tex_idx, uv + ox3 - oy1, screen_px_range);
    float a2 = msd_alpha(tex_idx, uv + ox1 + oy3, screen_px_range);
    float a3 = msd_alpha(tex_idx, uv - ox3 + oy1, screen_px_range);
    return (a0 + a1 + a2 + a3) * 0.25;
}

[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target0 {
    float2 dx = ddx(input.tex_coord);
    float2 dy = ddy(input.tex_coord);

    float2 screen_tex_size = float2(1.0, 1.0) / max(abs(dx) + abs(dy), float2(1e-7, 1e-7));
    float screen_px_range = max(0.5 * dot(pc.unit_range, screen_tex_size), 1.0);

    float glyph_alpha = supersampled_msd(pc.tex_idx, input.tex_coord, dx, dy, screen_px_range);

    float2 shadow_uv_offset = dy * pc.shadow_offset_px;
    float shadow_px_range = screen_px_range * pc.shadow_softness;
    float shadow_alpha = tsd_alpha(pc.tex_idx, input.tex_coord + shadow_uv_offset, shadow_px_range)
        * pc.shadow_strength;

    float fg_a = input.color.a * glyph_alpha;
    float sh_a = input.color.a * shadow_alpha;
    float out_a = fg_a + sh_a * (1.0 - fg_a);

    float3 composite = input.color.rgb * fg_a + pc.shadow_color * sh_a * (1.0 - fg_a);
    float3 out_rgb = out_a > 0.0 ? composite / out_a : float3(0.0, 0.0, 0.0);
    return float4(out_rgb, out_a);
}
