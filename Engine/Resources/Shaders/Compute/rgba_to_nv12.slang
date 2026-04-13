[[vk::binding(0, 0)]]
Sampler2D input_rgba;

[[vk::binding(1, 0)]]
RWTexture2D<float> output_y;

[[vk::binding(2, 0)]]
RWTexture2D<float2> output_uv;

struct push_constants {
    uint2 extent;
};

[[vk::push_constant]]
ConstantBuffer<push_constants> pc;

float3 srgb_to_linear(float3 c) {
    return select(c <= 0.04045,
                  c / 12.92,
                  pow((c + 0.055) / 1.055, 2.4));
}

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint2 pixel = dispatch_id.xy;
    if (pixel.x >= pc.extent.x || pixel.y >= pc.extent.y) return;

    float2 uv = (float2(pixel) + 0.5) / float2(pc.extent);
    float4 rgba = input_rgba.SampleLevel(uv, 0);
    float3 rgb = srgb_to_linear(rgba.rgb);

    float luma = 0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b;
    output_y[pixel] = saturate(luma);

    if ((pixel.x & 1) == 0 && (pixel.y & 1) == 0) {
        float cb = (rgb.b - luma) / 1.8556 + 0.5;
        float cr = (rgb.r - luma) / 1.5748 + 0.5;
        output_uv[pixel >> 1] = saturate(float2(cb, cr));
    }
}
