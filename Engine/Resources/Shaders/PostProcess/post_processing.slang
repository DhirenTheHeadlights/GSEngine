#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(constant_id = 99) const int descriptor_layout_type = 4;

layout(location = 0) in  vec2 tex_coords;
layout(location = 0) out vec4 frag_color;

layout(set=0, binding=0) uniform sampler2D scene_texture;
layout(set=0, binding=1) uniform sampler2D bloom_blur_texture;

layout(push_constant) uniform PushConstants {
    float exposure;
    bool  hdr;
    bool  bloom;
} push_constants;

void main() {
    vec3 result = texture(scene_texture, tex_coords).rgb;

    if (push_constants.bloom) {
        vec3 bloom_color = texture(bloom_blur_texture, tex_coords).rgb;
        result += bloom_color;
    }

    const float gamma = 2.2;

    if (push_constants.hdr) {
        // Reinhard or exponential tone mapping
        vec3 mapped = vec3(1.0) - exp(-result * push_constants.exposure);
        mapped = pow(mapped, vec3(1.0 / gamma));
        frag_color = vec4(mapped, 1.0);
    } else {
        // Just gamma correction
        vec3 mapped = pow(result, vec3(1.0 / gamma));
        frag_color = vec4(mapped, 1.0);
    }
}

#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(constant_id = 99) const int descriptor_layout_type = 4;

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_tex_coords;

layout(location = 0) out vec2 tex_coords;

void main() {
    tex_coords = in_tex_coords;
    gl_Position = vec4(in_position, 0.0, 1.0);
}
