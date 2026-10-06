void bloom_upsample(uint3 dispatch_id) {
    uint2 dst_size = pc.dst_active_extent;
    if (dispatch_id.x >= dst_size.x || dispatch_id.y >= dst_size.y) {
        return;
    }

    float2 src_texel = pc.radius * pc.inv_src_allocated_extent;
    float2 screen_uv = (float2(dispatch_id.xy) + 0.5) / float2(dst_size);
    float2 uv = screen_target_uv(screen_uv, pc.src_uv_scale, pc.src_uv_max);

    float3 sum = float3(0.0);
    sum += bloom_up_in.SampleLevel(bloom_up_sampler, screen_target_tap(uv + src_texel * float2(-1, -1), pc.src_uv_max), 0).rgb * (1.0 / 16.0);
    sum += bloom_up_in.SampleLevel(bloom_up_sampler, screen_target_tap(uv + src_texel * float2( 0, -1), pc.src_uv_max), 0).rgb * (2.0 / 16.0);
    sum += bloom_up_in.SampleLevel(bloom_up_sampler, screen_target_tap(uv + src_texel * float2( 1, -1), pc.src_uv_max), 0).rgb * (1.0 / 16.0);
    sum += bloom_up_in.SampleLevel(bloom_up_sampler, screen_target_tap(uv + src_texel * float2(-1,  0), pc.src_uv_max), 0).rgb * (2.0 / 16.0);
    sum += bloom_up_in.SampleLevel(bloom_up_sampler, screen_target_tap(uv + src_texel * float2( 0,  0), pc.src_uv_max), 0).rgb * (4.0 / 16.0);
    sum += bloom_up_in.SampleLevel(bloom_up_sampler, screen_target_tap(uv + src_texel * float2( 1,  0), pc.src_uv_max), 0).rgb * (2.0 / 16.0);
    sum += bloom_up_in.SampleLevel(bloom_up_sampler, screen_target_tap(uv + src_texel * float2(-1,  1), pc.src_uv_max), 0).rgb * (1.0 / 16.0);
    sum += bloom_up_in.SampleLevel(bloom_up_sampler, screen_target_tap(uv + src_texel * float2( 0,  1), pc.src_uv_max), 0).rgb * (2.0 / 16.0);
    sum += bloom_up_in.SampleLevel(bloom_up_sampler, screen_target_tap(uv + src_texel * float2( 1,  1), pc.src_uv_max), 0).rgb * (1.0 / 16.0);

    float3 add = bloom_up_dn.SampleLevel(bloom_up_sampler, screen_target_uv(screen_uv, pc.dn_uv_scale, pc.dn_uv_max), 0).rgb;
    bloom_up_out[dispatch_id.xy] = float4(sum + add, 1.0);
}
