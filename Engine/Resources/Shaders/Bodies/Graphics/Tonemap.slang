struct VS_OUT {
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

[shader("vertex")]
VS_OUT vs_main(uint vid : SV_VertexID) {
    float2 p = float2((vid << 1) & 2, vid & 2);
    VS_OUT o;
    o.uv = p;
    o.clip_pos = float4(p * 2.0 - 1.0, 0.0, 1.0);
    return o;
}

float3 agx_default_contrast_approx(float3 x) {
    float3 x2 = x * x;
    float3 x4 = x2 * x2;
    return 15.5    * x4 * x2
         - 40.14   * x4 * x
         + 31.96   * x4
         - 6.868   * x2 * x
         + 0.4298  * x2
         + 0.1191  * x
         - 0.00232;
}

float3 agx(float3 val) {
    const float3x3 agx_input_mat = float3x3(
        0.842479062253094, 0.0784335999999992, 0.0792237451477643,
        0.0423282422610123, 0.878468636469772, 0.0791661274605434,
        0.0423756549057051, 0.0784336,         0.879142973793104
    );
    const float min_ev = -12.47393;
    const float max_ev = 4.026069;

    val = mul(agx_input_mat, val);
    val = clamp(log2(max(val, 1e-10)), min_ev, max_ev);
    val = (val - min_ev) / (max_ev - min_ev);
    return agx_default_contrast_approx(val);
}

float3 agx_punchy_look(float3 val) {
    const float3 luma_weights = float3(0.2126, 0.7152, 0.0722);
    float luma = dot(val, luma_weights);

    const float3 power = float3(1.35, 1.35, 1.35);
    const float saturation = 1.4;

    val = pow(max(val, 0.0), power);
    return luma + saturation * (val - luma);
}

float3 agx_eotf(float3 val) {
    const float3x3 agx_output_mat = float3x3(
         1.19687900512017,   -0.0528968517574562, -0.0529716355144438,
        -0.0980208811401368,  1.15190312990417,   -0.0980434501171241,
        -0.0990297440797205, -0.0989611768448433,  1.15107367264116
    );
    return mul(agx_output_mat, val);
}

float3 hsv_to_rgb(float h, float s, float v) {
    float3 c = float3(h, s, v);
    float4 k = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(c.xxx + k.xyz) * 6.0 - k.www);
    return c.z * lerp(k.xxx, saturate(p - k.xxx), c.y);
}

float3 visualize_velocity(float2 v) {
    float magnitude = saturate(length(v) * 50.0);
    float angle = atan2(v.y, v.x) / (2.0 * 3.14159265) + 0.5;
    return hsv_to_rgb(angle, 1.0, magnitude);
}

[shader("fragment")]
float4 fs_main(VS_OUT input) : SV_Target {
    if (pc.show_velocity != 0) {
        float2 v = velocity_color.Sample(color_sampler, input.uv).rg;
        return float4(visualize_velocity(v), 1.0);
    }

    float3 hdr = hdr_color.Sample(color_sampler, input.uv).rgb;
    float3 bloom = bloom_color.Sample(color_sampler, input.uv).rgb;
    float3 combined = hdr + bloom * pc.bloom_intensity;
    float3 exposed = combined * pc.exposure;

    float3 mapped = agx(exposed);
    mapped = agx_punchy_look(mapped);
    mapped = agx_eotf(mapped);

    return float4(saturate(mapped), 1.0);
}
