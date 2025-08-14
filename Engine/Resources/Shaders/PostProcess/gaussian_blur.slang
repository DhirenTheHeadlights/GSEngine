import common;
import post_process;

[[vk::push_constant]]
cbuffer PushConstants {
    int   blur_amount;
    float blur_radius;
    float bloom_intensity;
    bool  horizontal;
};

struct VS_OUTPUT {
    float4 clip_pos   : SV_Position;
    float2 tex_coords : TEXCOORD;
};

[Layout(LayoutKind::PostProcess)]
[shader("vertex")]
VS_OUTPUT vs_main(float3 in_position: POSITION, float2 in_tex_coords: TEXCOORD) {
    VS_OUTPUT output;
    output.tex_coords = in_tex_coords;
    output.clip_pos = float4(in_position, 1.0);
    return output;
}

float gaussian_weight(int x, float sigma) {
    return exp(-(x * x) / (2.0 * sigma * sigma)) / (sqrt(2.0 * 3.14159) * sigma);
}

[Layout(LayoutKind::PostProcess)]
[shader("fragment")]
float4 fs_main(VS_OUTPUT input) : SV_Target0 {
    float sigma = float(blur_amount) * 0.5;

    uint w, h, levels;
    inputTex0.GetDimensions(0, w, h, levels);
    float2 tex_offset = (1.0 / float2(w, h)) * blur_radius;

    float3 result = inputTex0.Sample(input.tex_coords).rgb * gaussian_weight(0, sigma);
    float total_weight = gaussian_weight(0, sigma);

    if (horizontal) {
        for (int i = 1; i < blur_amount; ++i) {
            float weight = gaussian_weight(i, sigma);
            result += inputTex0.Sample(input.tex_coords + float2(tex_offset.x * i, 0.0)).rgb * weight;
            result += inputTex0.Sample(input.tex_coords - float2(tex_offset.x * i, 0.0)).rgb * weight;
            total_weight += 2.0 * weight;
        }
    }
    else {
        for (int i = 1; i < blur_amount; ++i) {
            float weight = gaussian_weight(i, sigma);
            result += inputTex0.Sample(input.tex_coords + float2(0.0, tex_offset.y * i)).rgb * weight;
            result += inputTex0.Sample(input.tex_coords - float2(0.0, tex_offset.y * i)).rgb * weight;
            total_weight += 2.0 * weight;
        }
    }
    
    result /= total_weight;
    result *= bloom_intensity;

    return float4(result, 1.0);
}