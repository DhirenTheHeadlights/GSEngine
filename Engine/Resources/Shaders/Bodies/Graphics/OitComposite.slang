[shader("vertex")]
screen_vs_out vs_main(uint vid : SV_VertexID) {
    return screen_pass_vertex(vid, screen_depth_near);
}

[shader("fragment")]
float4 fs_main(screen_vs_out input) : SV_Target {
    float2 target_uv = screen_target_uv(input.uv, pc.screen_uv_scale, pc.screen_uv_max);
    float reveal = oit_reveal_in.Sample(oit_sampler, target_uv).a;
    if (reveal > 0.9999) {
        discard;
    }
    float4 accum = oit_accum_in.Sample(oit_sampler, target_uv);
    float3 avg = accum.rgb / max(accum.a, 1e-5);
    return float4(avg, 1.0 - reveal);
}
