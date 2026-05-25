float karis_weight(float3 c) {
    float luma = dot(c, float3(0.299, 0.587, 0.114));
    return 1.0 / (1.0 + luma);
}

float3 weighted_group(float3 a, float3 b, float3 c, float3 d, bool karis) {
    float3 avg = (a + b + c + d) * 0.25;
    if (karis) {
        float wa = karis_weight(a);
        float wb = karis_weight(b);
        float wc = karis_weight(c);
        float wd = karis_weight(d);
        float total = wa + wb + wc + wd;
        return (a * wa + b * wb + c * wc + d * wd) / max(total, 1e-5);
    }
    return avg;
}

void bloom_downsample(uint3 dispatch_id) {
    uint2 dst_size;
    bloom_out.GetDimensions(dst_size.x, dst_size.y);
    if (dispatch_id.x >= dst_size.x || dispatch_id.y >= dst_size.y) {
        return;
    }

    uint2 src_size;
    bloom_in.GetDimensions(src_size.x, src_size.y);
    float2 src_texel = 1.0 / float2(src_size);
    float2 uv = (float2(dispatch_id.xy) + 0.5) / float2(dst_size);

    float3 a = bloom_in.SampleLevel(uv + src_texel * float2(-2, -2), 0).rgb;
    float3 b = bloom_in.SampleLevel(uv + src_texel * float2( 0, -2), 0).rgb;
    float3 c = bloom_in.SampleLevel(uv + src_texel * float2( 2, -2), 0).rgb;

    float3 d = bloom_in.SampleLevel(uv + src_texel * float2(-2,  0), 0).rgb;
    float3 e = bloom_in.SampleLevel(uv + src_texel * float2( 0,  0), 0).rgb;
    float3 f = bloom_in.SampleLevel(uv + src_texel * float2( 2,  0), 0).rgb;

    float3 g = bloom_in.SampleLevel(uv + src_texel * float2(-2,  2), 0).rgb;
    float3 h = bloom_in.SampleLevel(uv + src_texel * float2( 0,  2), 0).rgb;
    float3 i = bloom_in.SampleLevel(uv + src_texel * float2( 2,  2), 0).rgb;

    float3 j = bloom_in.SampleLevel(uv + src_texel * float2(-1, -1), 0).rgb;
    float3 k = bloom_in.SampleLevel(uv + src_texel * float2( 1, -1), 0).rgb;
    float3 l = bloom_in.SampleLevel(uv + src_texel * float2(-1,  1), 0).rgb;
    float3 m = bloom_in.SampleLevel(uv + src_texel * float2( 1,  1), 0).rgb;

    bool karis = pc.use_karis_average != 0;

    float3 inner = weighted_group(j, k, l, m, karis);
    float3 top_left = weighted_group(a, b, d, e, karis);
    float3 top_right = weighted_group(b, c, e, f, karis);
    float3 bot_left = weighted_group(d, e, g, h, karis);
    float3 bot_right = weighted_group(e, f, h, i, karis);

    float3 result = inner * 0.5 + (top_left + top_right + bot_left + bot_right) * 0.125;
    bloom_out[dispatch_id.xy] = float4(result, 1.0);
}
