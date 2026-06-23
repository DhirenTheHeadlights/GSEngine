struct VS_OUT {
    float4 clip_pos : SV_Position;
    float2 ndc      : TEXCOORD0;
};

[shader("vertex")]
VS_OUT vs_main(uint vid : SV_VertexID) {
    float2 p = float2((vid << 1) & 2, vid & 2);
    VS_OUT o;
    o.ndc = p * 2.0 - 1.0;
    o.clip_pos = float4(o.ndc, 0.0, 1.0);
    return o;
}

struct FS_OUT {
    float4 color : SV_Target;
    float  depth : SV_Depth;
};

float grid_factor(float2 p, float spacing, float thickness) {
    float2 q = p / spacing;
    float2 d = max(fwidth(q), float2(1e-6, 1e-6));
    float2 g = abs(frac(q - 0.5) - 0.5) / d;
    float line_dist = min(g.x, g.y);
    return 1.0 - smoothstep(thickness, thickness + 1.0, line_dist);
}

float axis_factor(float2 p, float thickness) {
    float2 d = max(fwidth(p), float2(1e-6, 1e-6));
    float2 g = abs(p) / d;
    float line_dist = min(g.x, g.y);
    return 1.0 - smoothstep(thickness, thickness + 1.0, line_dist);
}

[shader("fragment")]
FS_OUT fs_main(VS_OUT input) {
    float4 wn = mul(camera_ubo.inv_view_proj, float4(input.ndc, 0.0, 1.0));
    float4 wf = mul(camera_ubo.inv_view_proj, float4(input.ndc, 1.0, 1.0));
    float3 ro = wn.xyz / wn.w;
    float3 rt = wf.xyz / wf.w;
    float3 rd = normalize(rt - ro);

    if (abs(rd.y) < 1e-5) {
        discard;
    }

    float t = -ro.y / rd.y;
    if (t < 0.0) {
        discard;
    }

    float3 hit = ro + rd * t;
    float2 p = hit.xz;

    float minor = grid_factor(p, pc.minor_spacing, pc.minor_thickness);
    float major = grid_factor(p, pc.major_spacing, pc.major_thickness);
    float axis  = axis_factor(p, pc.axis_thickness);

    float a = max(minor, max(major, axis));
    if (a <= 0.0) {
        discard;
    }

    float dist_fade = 1.0 - saturate(t / pc.fade_distance);
    if (dist_fade <= 0.0) {
        discard;
    }

    float3 col = pc.minor_color;
    col = lerp(col, pc.major_color, major);
    col = lerp(col, pc.axis_color, axis);

    float final_alpha = a * dist_fade;

    float4 clip = mul(camera_ubo.proj, mul(camera_ubo.view, float4(hit, 1.0)));
    FS_OUT o;
    o.color = float4(col, final_alpha);
    o.depth = clip.z / clip.w;
    return o;
}
