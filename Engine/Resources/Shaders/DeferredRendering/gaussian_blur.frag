#version 450

layout (location = 0) out vec4 frag_color;
layout (location = 0) in vec2 tex_coords;

layout (binding = 0) uniform sampler2D image;

layout (push_constant) uniform PushConstants {
    int blur_amount;
    float blur_radius;
    float bloom_intensity;
    bool horizontal;
} push_constants;

float gaussian_weight(int x, float sigma) {
    return exp(-(x * x) / (2.0 * sigma * sigma)) / (sqrt(2.0 * 3.14159) * sigma);
}

void main() {             
    float sigma = float(push_constants.blur_amount) * 0.5;
    vec2 tex_offset = 1.0 / vec2(textureSize(image, 0)) * push_constants.blur_radius; // Gets size of single texel
    vec3 result = texture(image, tex_coords).rgb * gaussian_weight(0, sigma);
    float total_weight = gaussian_weight(0, sigma);

    if (push_constants.horizontal) {
        for (int i = 1; i < push_constants.blur_amount; ++i) {
            float weight = gaussian_weight(i, sigma);
            result += texture(image, tex_coords + vec2(tex_offset.x * i, 0.0)).rgb * weight;
            result += texture(image, tex_coords - vec2(tex_offset.x * i, 0.0)).rgb * weight;
            total_weight += 2.0 * weight;
        }
    } else {
        for (int i = 1; i < push_constants.blur_amount; ++i) {
            float weight = gaussian_weight(i, sigma);
            result += texture(image, tex_coords + vec2(0.0, tex_offset.y * i)).rgb * weight;
            result += texture(image, tex_coords - vec2(0.0, tex_offset.y * i)).rgb * weight;
            total_weight += 2.0 * weight;
        }
    }
    result /= total_weight;                
    result *= push_constants.bloom_intensity; 
    frag_color = vec4(result, 1.0);
}
