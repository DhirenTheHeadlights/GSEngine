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

[shader("fragment")]
float4 fs_main(VS_OUT input) : SV_Target {
    float reveal = oit_reveal_in.Sample(oit_sampler, input.uv).a;
    if (reveal > 0.9999) {
        discard;
    }
    float4 accum = oit_accum_in.Sample(oit_sampler, input.uv);
    float3 avg = accum.rgb / max(accum.a, 1e-5);
    return float4(avg, 1.0 - reveal);
}
