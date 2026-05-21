float3 tent_sample(float2 uv, float2 src_texel, float radius) {
    float2 r = src_texel * radius;
    float3 sum = float3(0.0);

    sum += bloom_in_lower.SampleLevel(uv + float2(-r.x, -r.y), 0).rgb;
    sum += bloom_in_lower.SampleLevel(uv + float2(   0, -r.y), 0).rgb * 2.0;
    sum += bloom_in_lower.SampleLevel(uv + float2( r.x, -r.y), 0).rgb;

    sum += bloom_in_lower.SampleLevel(uv + float2(-r.x,    0), 0).rgb * 2.0;
    sum += bloom_in_lower.SampleLevel(uv                     , 0).rgb * 4.0;
    sum += bloom_in_lower.SampleLevel(uv + float2( r.x,    0), 0).rgb * 2.0;

    sum += bloom_in_lower.SampleLevel(uv + float2(-r.x,  r.y), 0).rgb;
    sum += bloom_in_lower.SampleLevel(uv + float2(   0,  r.y), 0).rgb * 2.0;
    sum += bloom_in_lower.SampleLevel(uv + float2( r.x,  r.y), 0).rgb;

    return sum * (1.0 / 16.0);
}

// entry:
uint2 dst_size;
bloom_up_out.GetDimensions(dst_size.x, dst_size.y);
if (dispatch_id.x >= dst_size.x || dispatch_id.y >= dst_size.y) {
    return;
}

uint2 lower_size;
bloom_in_lower.GetDimensions(lower_size.x, lower_size.y);
float2 lower_texel = 1.0 / float2(lower_size);
float2 uv = (float2(dispatch_id.xy) + 0.5) / float2(dst_size);

float3 upsampled = tent_sample(uv, lower_texel, pc.radius);
float3 same = bloom_in_same.SampleLevel(uv, 0).rgb;
bloom_up_out[dispatch_id.xy] = float4(same + upsampled, 1.0);
