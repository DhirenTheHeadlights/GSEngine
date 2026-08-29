static const uint SHAPE_ARC = 1u;

struct VS_OUTPUT {
    float4 clip_pos     : SV_Position;
    float2 tex_coord    : TEXCOORD0;
    float4 color        : COLOR0;
    float2 local_pos    : TEXCOORD1;
    float2 half_size    : TEXCOORD2;
    float  corner_radius: TEXCOORD3;
    nointerpolation uint shape_kind : TEXCOORD4;
    float2 arc_sc       : TEXCOORD5;
    float  arc_radius   : TEXCOORD6;
    float  arc_thickness: TEXCOORD7;
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
    output.shape_kind = v.shape_kind;
    output.arc_sc = float2(sin(v.arc_half_sweep), cos(v.arc_half_sweep));
    output.arc_radius = v.arc_radius;
    output.arc_thickness = v.arc_thickness;
    return output;
}

[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target0 {
    const uint INVALID_INDEX = 0xFFFFFFFFu;

    float4 col;
    if (pc.snapshot_tex_idx != INVALID_INDEX) {
        float2 screen_uv = input.clip_pos.xy * pc.inv_screen_size;
        float4 bg = textures[NonUniformResourceIndex(pc.snapshot_tex_idx)].Sample(textures_sampler, screen_uv);
        float a = input.color.a;
        col.rgb = input.color.rgb * a + bg.rgb * (1.0 - a);
        col.a = 1.0;
    } else {
        col = textures[NonUniformResourceIndex(pc.tex_idx)].Sample(textures_sampler, input.tex_coord) * input.color;
    }

    if (input.shape_kind == SHAPE_ARC) {
        float2 p = float2(abs(input.local_pos.x), input.local_pos.y);
        float outside = input.arc_sc.y * p.x - input.arc_sc.x * p.y;
        float d = (outside > 0.0 ? length(p - input.arc_sc * input.arc_radius) : abs(length(p) - input.arc_radius)) - input.arc_thickness;
        col *= smoothstep(0.0, -1.0, d);
    }
    else if (input.corner_radius > 0.0) {
        float2 q = abs(input.local_pos) - input.half_size + input.corner_radius;
        float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - input.corner_radius;
        float aa = smoothstep(0.0, -1.0, d);
        col *= aa;
    }

    return col;
}
