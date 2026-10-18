static const uint SHAPE_ARC = 1u;
static const uint SHAPE_RING = 2u;

struct VS_OUTPUT {
    float4 clip_pos     : SV_Position;
    float2 tex_coord    : TEXCOORD0;
    float4 color        : COLOR0;
    float2 local_pos    : TEXCOORD1;
    float2 half_size    : TEXCOORD2;
    float4 corner_radius: TEXCOORD3;
    nointerpolation uint shape_kind : TEXCOORD4;
    float2 arc_sc       : TEXCOORD5;
    float  arc_radius   : TEXCOORD6;
    float  stroke_thickness : TEXCOORD7;
};

float corner_radius_at(float4 radii, float2 local) {
    float right = step(0.0, local.x);
    float top = step(0.0, local.y);
    float upper = lerp(radii.x, radii.y, right);
    float lower = lerp(radii.w, radii.z, right);
    return lerp(lower, upper, top);
}

float rounded_box_sdf(float2 local, float2 half_size, float radius) {
    float r = min(radius, min(half_size.x, half_size.y));
    float2 q = abs(local) - half_size + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

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
    output.stroke_thickness = v.stroke_thickness;
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
        float d = (outside > 0.0 ? length(p - input.arc_sc * input.arc_radius) : abs(length(p) - input.arc_radius)) - input.stroke_thickness;
        col *= smoothstep(0.0, -1.0, d);
    }
    else if (input.shape_kind == SHAPE_RING) {
        float radius = corner_radius_at(input.corner_radius, input.local_pos);
        float d = rounded_box_sdf(input.local_pos, input.half_size, radius);
        col *= smoothstep(0.5, -0.5, abs(d + input.stroke_thickness) - input.stroke_thickness);
    }
    else if (any(input.corner_radius > 0.0)) {
        float radius = corner_radius_at(input.corner_radius, input.local_pos);
        float d = rounded_box_sdf(input.local_pos, input.half_size, radius);
        col *= smoothstep(0.0, -1.0, d);
    }

    return col;
}
