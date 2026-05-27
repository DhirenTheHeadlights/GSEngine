void bloom_upsample(uint3 dispatch_id) {
    uint2 dst_size;
    bloom_up_out.GetDimensions(dst_size.x, dst_size.y);
    if (dispatch_id.x >= dst_size.x || dispatch_id.y >= dst_size.y) {
        return;
    }

    uint2 src_size;
    bloom_up_in.GetDimensions(src_size.x, src_size.y);
    float2 src_texel = pc.radius / float2(src_size);
    float2 uv = (float2(dispatch_id.xy) + 0.5) / float2(dst_size);

    float3 sum = float3(0.0);
    sum += bloom_up_in.SampleLevel(uv + src_texel * float2(-1, -1), 0).rgb * (1.0 / 16.0);
    sum += bloom_up_in.SampleLevel(uv + src_texel * float2( 0, -1), 0).rgb * (2.0 / 16.0);
    sum += bloom_up_in.SampleLevel(uv + src_texel * float2( 1, -1), 0).rgb * (1.0 / 16.0);
    sum += bloom_up_in.SampleLevel(uv + src_texel * float2(-1,  0), 0).rgb * (2.0 / 16.0);
    sum += bloom_up_in.SampleLevel(uv + src_texel * float2( 0,  0), 0).rgb * (4.0 / 16.0);
    sum += bloom_up_in.SampleLevel(uv + src_texel * float2( 1,  0), 0).rgb * (2.0 / 16.0);
    sum += bloom_up_in.SampleLevel(uv + src_texel * float2(-1,  1), 0).rgb * (1.0 / 16.0);
    sum += bloom_up_in.SampleLevel(uv + src_texel * float2( 0,  1), 0).rgb * (2.0 / 16.0);
    sum += bloom_up_in.SampleLevel(uv + src_texel * float2( 1,  1), 0).rgb * (1.0 / 16.0);

    float3 add = bloom_up_dn.SampleLevel(uv, 0).rgb;
    bloom_up_out[dispatch_id.xy] = float4(sum + add, 1.0);
}
