float3 srgb_to_linear(float3 c) {
    return select(c <= 0.04045,
                  c / 12.92,
                  pow((c + 0.055) / 1.055, 2.4));
}

// entry:
uint2 pixel = dispatch_id.xy;
if (pixel.x >= pc.extent.x || pixel.y >= pc.extent.y) return;
if (pc.rgba_index == 0xFFFFFFFFu) return;

float2 uv = (float2(pixel) + 0.5) / float2(pc.extent);
float4 rgba = textures[NonUniformResourceIndex(pc.rgba_index)].SampleLevel(uv, 0);
float3 rgb = srgb_to_linear(rgba.rgb);

float luma = 0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b;
output_y[pixel] = saturate(luma);

if ((pixel.x & 1) == 0 && (pixel.y & 1) == 0) {
    float cb = (rgb.b - luma) / 1.8556 + 0.5;
    float cr = (rgb.r - luma) / 1.5748 + 0.5;
    output_uv[pixel >> 1] = saturate(float2(cb, cr));
}
