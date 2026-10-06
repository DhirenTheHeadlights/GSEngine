[shader("vertex")]
screen_vs_out vs_main(uint vid : SV_VertexID) {
    return screen_pass_vertex(vid, screen_depth_far);
}

[shader("fragment")]
float4 fs_main(screen_vs_out input) : SV_Target {
    float4 sample = cloud_in.Sample(cloud_composite_sampler, screen_target_uv(input.uv, pc.screen_uv_scale, pc.screen_uv_max));
    float opacity = saturate(1.0 - sample.a);
    if (opacity < 1e-6) {
        discard;
    }
    float3 unpremul = sample.rgb / opacity;
    return float4(unpremul, opacity);
}
