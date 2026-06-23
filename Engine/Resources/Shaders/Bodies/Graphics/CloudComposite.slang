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
    float4 sample = cloud_in.Sample(cloud_composite_sampler, input.uv);
    float opacity = saturate(1.0 - sample.a);
    if (opacity < 1e-6) {
        discard;
    }
    float3 unpremul = sample.rgb / opacity;
    return float4(unpremul, opacity);
}
