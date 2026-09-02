struct VS_OUT {
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

[shader("vertex")]
VS_OUT vs_main(uint vid : SV_VertexID) {
    float2 p = float2((vid << 1) & 2, vid & 2);
    VS_OUT o;
    o.uv = p;
    o.clip_pos = float4(p * 2.0 - 1.0, 1.0, 1.0);
    return o;
}

static const float history_transmittance_tolerance = 0.15;

float3 world_ray_from_ndc(float2 ndc) {
    float4 near_point = mul(pc.inv_view_proj, float4(ndc, 0.0, 1.0));
    float4 far_point = mul(pc.inv_view_proj, float4(ndc, 1.0, 1.0));
    return normalize(far_point.xyz / far_point.w - near_point.xyz / near_point.w);
}

[shader("fragment")]
float4 fs_main(VS_OUT input) : SV_Target {
    int2 pixel = int2(input.uv * float2(pc.full_extent));
    int2 offset = int2(pc.block_offset);

    if (pixel.x % pc.divisor == offset.x && pixel.y % pc.divisor == offset.y) {
        int2 low_extent = max(int2(pc.full_extent) / pc.divisor, int2(1, 1));
        return resolve_low_in.Load(int3(min(pixel / pc.divisor, low_extent - 1), 0));
    }

    float4 upscaled = resolve_low_in.SampleLevel(resolve_sampler_binding, input.uv, 0);

    if (pc.history_valid == 0) {
        return upscaled;
    }

    float3 view_dir = world_ray_from_ndc(input.uv * 2.0 - 1.0);
    float4 previous_clip = mul(pc.prev_view_proj, float4(view_dir, 0.0));

    if (previous_clip.w <= 1e-6) {
        return upscaled;
    }

    float2 previous_uv = (previous_clip.xy / previous_clip.w) * 0.5 + 0.5;

    if (any(previous_uv != saturate(previous_uv))) {
        return upscaled;
    }

    float4 history = resolve_history_in.SampleLevel(resolve_sampler_binding, previous_uv, 0);

    if (upscaled.a >= 1.0 || history.a >= 1.0) {
        return upscaled;
    }

    if (abs(history.a - upscaled.a) > history_transmittance_tolerance) {
        return upscaled;
    }

    return history;
}
