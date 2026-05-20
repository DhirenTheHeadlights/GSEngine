struct VS_OUTPUT {
    float4 clip_pos     : SV_Position;
    float2 tex_coord    : TEXCOORD0;
    float4 color        : COLOR0;
    float2 local_pos    : TEXCOORD1;
    float2 half_size    : TEXCOORD2;
    float  corner_radius: TEXCOORD3;
};

[shader("vertex")]
VS_OUTPUT vs_main(
    float2 in_position      : POSITION,
    float2 in_tex_coord     : TEXCOORD0,
    float4 in_color         : COLOR0,
    float2 in_local_pos     : TEXCOORD1,
    float2 in_half_size     : TEXCOORD2,
    float  in_corner_radius : TEXCOORD3
) {
    VS_OUTPUT output;
    output.clip_pos = mul(pc.projection, float4(in_position, 0.0, 1.0));
    output.tex_coord = in_tex_coord;
    output.color = in_color;
    output.local_pos = in_local_pos;
    output.half_size = in_half_size;
    output.corner_radius = in_corner_radius;
    return output;
}

[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target0 {
    const uint INVALID_INDEX = 0xFFFFFFFFu;

    float4 col;
    if (pc.snapshot_tex_idx != INVALID_INDEX) {
        // DIAGNOSTIC: show input.color.r as grayscale. We expect ~0.14 (DARK).
        // If widget is bright/white, the RED component is also wrong → vertex layout mismatch.
        // If widget is dark gray (~14% brightness), red is correct and only alpha is wrong.
        col = float4(input.color.r, input.color.r, input.color.r, 1.0);
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
