float2 screen_target_tap(float2 target_uv, float2 uv_max) {
    return min(target_uv, uv_max);
}

float2 screen_target_uv(float2 screen_uv, float2 uv_scale, float2 uv_max) {
    return screen_target_tap(screen_uv * uv_scale, uv_max);
}

bool screen_uv_inside(float2 screen_uv) {
    return all(screen_uv == saturate(screen_uv));
}

float4 screen_target_catmull_rom(Texture2D<float4> target, SamplerState target_sampler, float2 position, float2 texel, float2 uv_max, out float4 tap_min, out float4 tap_max) {
    float2 center = floor(position - 0.5) + 0.5;
    float2 f = position - center;
    float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    float2 w3 = f * f * (-0.5 + 0.5 * f);
    float2 w12 = w1 + w2;
    float2 uv0 = (center - 1.0) * texel;
    float2 uv3 = (center + 2.0) * texel;
    float2 uv12 = (center + w2 / w12) * texel;

    float4 top = target.SampleLevel(target_sampler, screen_target_tap(float2(uv12.x, uv0.y), uv_max), 0);
    float4 left = target.SampleLevel(target_sampler, screen_target_tap(float2(uv0.x, uv12.y), uv_max), 0);
    float4 middle = target.SampleLevel(target_sampler, screen_target_tap(uv12, uv_max), 0);
    float4 right = target.SampleLevel(target_sampler, screen_target_tap(float2(uv3.x, uv12.y), uv_max), 0);
    float4 bottom = target.SampleLevel(target_sampler, screen_target_tap(float2(uv12.x, uv3.y), uv_max), 0);

    float4 sum = top * (w12.x * w0.y);
    sum += left * (w0.x * w12.y);
    sum += middle * (w12.x * w12.y);
    sum += right * (w3.x * w12.y);
    sum += bottom * (w12.x * w3.y);
    float weight = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;

    tap_min = min(min(min(top, left), min(right, bottom)), middle);
    tap_max = max(max(max(top, left), max(right, bottom)), middle);
    return sum / weight;
}
