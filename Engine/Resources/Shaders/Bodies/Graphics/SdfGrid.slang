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
    float4 color    : SV_Target0;
    float4 velocity : SV_Target1;
    float  depth    : SV_Depth;
};

float2 pulse_integral(float2 x, float2 half_width) {
    return floor(x) * (2.0 * half_width)
         + clamp(frac(x) - (0.5 - half_width), float2(0.0, 0.0), 2.0 * half_width);
}

float merge(float2 c) {
    return c.x + c.y - c.x * c.y;
}

float grid_factor(float2 p, float2 footprint, float spacing, float half_px) {
    float2 w = footprint / spacing;
    float2 half_width = min(half_px * w, float2(0.5, 0.5));
    float2 c = frac(p / spacing);
    float2 coverage = (pulse_integral(c + 0.5 + w * 0.5, half_width)
                     - pulse_integral(c + 0.5 - w * 0.5, half_width)) / w;
    return merge(saturate(coverage));
}

float axis_factor(float2 p, float2 footprint, float half_px) {
    float2 half_width = half_px * footprint;
    float2 hi = min(p + footprint * 0.5,  half_width);
    float2 lo = max(p - footprint * 0.5, -half_width);
    return merge(saturate((hi - lo) / footprint));
}

float density_fade(float2 footprint, float spacing, float half_px) {
    float cell_px = spacing / max(footprint.x, footprint.y);
    float line_px = 2.0 * half_px;
    return smoothstep(line_px, line_px * 3.0, cell_px);
}

[shader("fragment")]
FS_OUT fs_main(VS_OUT input) {
    float4 wn = mul(camera_ubo.inv_view_proj, float4(input.ndc, 0.0, 1.0));
    float4 wf = mul(camera_ubo.inv_view_proj, float4(input.ndc, 1.0, 1.0));
    float3 ro = wn.xyz / wn.w;
    float3 rt = wf.xyz / wf.w;
    float3 rd = normalize(rt - ro);

    float magnitude = max(abs(rd.y), 1e-5);
    float denom = rd.y < 0.0 ? -magnitude : magnitude;
    float ray_t = -ro.y / denom;
    float t = clamp(ray_t, 0.0, pc.fade_distance);

    float3 hit = ro + rd * t;
    float2 p = hit.xz;
    float2 footprint = max(abs(ddx(p)) + abs(ddy(p)), float2(1e-6, 1e-6));

    if (ray_t <= 0.0 || ray_t >= pc.fade_distance) {
        discard;
    }

    float minor_px = pc.minor_thickness + 0.5;
    float major_px = pc.major_thickness + 0.5;
    float axis_px  = pc.axis_thickness + 0.5;

    float major_lod = density_fade(footprint, pc.major_spacing, major_px);

    float minor = grid_factor(p, footprint, pc.minor_spacing, minor_px)
                * density_fade(footprint, pc.minor_spacing, minor_px);
    float major = grid_factor(p, footprint, pc.major_spacing, major_px) * major_lod;
    float axis  = axis_factor(p, footprint, axis_px) * major_lod;

    float a = max(minor, max(major, axis));
    if (a <= 0.0) {
        discard;
    }

    float dist_fade = 1.0 - saturate(t / pc.fade_distance);

    float3 col = pc.minor_color;
    col = lerp(col, pc.major_color, major);
    col = lerp(col, pc.axis_color, axis);

    float final_alpha = a * dist_fade;

    float4 curr_clip = mul(camera_ubo.proj, mul(camera_ubo.view, float4(hit, 1.0)));
    float4 prev_clip = mul(camera_ubo.prev_proj, mul(camera_ubo.prev_view, float4(hit, 1.0)));
    float2 curr_ndc = curr_clip.xy / curr_clip.w + camera_ubo.jitter_ndc;
    float2 prev_ndc = prev_clip.xy / prev_clip.w + camera_ubo.prev_jitter_ndc;
    float2 motion = (curr_ndc - prev_ndc) * float2(0.5, -0.5);

    float depth_bias = 2e-6;

    FS_OUT o;
    o.color = float4(col, final_alpha);
    o.velocity = float4(motion, 0.0, final_alpha);
    o.depth = max(curr_clip.z / curr_clip.w - depth_bias, 0.0);
    return o;
}
