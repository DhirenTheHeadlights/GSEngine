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

float3 aces_approx(float3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

[shader("fragment")]
float4 fs_main(VS_OUT input) : SV_Target {
    float3 hdr = hdr_color.Sample(input.uv).rgb;
    float3 bloom = bloom_color.Sample(input.uv).rgb;
    float3 combined = hdr + bloom * pc.bloom_intensity;
    float3 exposed = combined * pc.exposure;
    float3 mapped = aces_approx(exposed);
    return float4(mapped, 1.0);
}
