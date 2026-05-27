struct VS_OUTPUT {
    float4 clip_pos     : SV_Position;
    float2 tex_coord    : TEXCOORD0;
    float4 color        : COLOR0;
    float2 local_pos    : TEXCOORD1;
    float2 half_size    : TEXCOORD2;
    float  corner_radius: TEXCOORD3;
};

[shader("vertex")]
VS_OUTPUT vs_main(uint vid : SV_VertexID) {
    vertex v = vertex_buffer[vid];
    VS_OUTPUT output;
    output.clip_pos = mul(pc.projection, float4(v.position, 0.0, 1.0));
    output.tex_coord = v.uv;
    output.color = v.color;
    output.local_pos = v.local_pos;
    output.half_size = v.half_size;
    output.corner_radius = v.corner_radius;
    return output;
}

[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target0 {
    const uint INVALID_INDEX = 0xFFFFFFFFu;

    float4 col;
    if (pc.snapshot_tex_idx != INVALID_INDEX) {
        float2 screen_uv = input.clip_pos.xy * pc.inv_screen_size;
        float4 bg = textures[NonUniformResourceIndex(pc.snapshot_tex_idx)].Sample(screen_uv);
        float a = input.color.a;
        col.rgb = input.color.rgb * a + bg.rgb * (1.0 - a);
        col.a = 1.0;
    } else {
        col = textures[NonUniformResourceIndex(pc.tex_idx)].Sample(input.tex_coord) * input.color;
    }

    if (input.corner_radius > 0.0) {
        float2 q = abs(input.local_pos) - input.half_size + input.corner_radius;
        float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - input.corner_radius;
        float aa = smoothstep(0.0, -1.0, d);
        col *= aa;
    }

    return col;
}
