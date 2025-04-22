#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99) const int descriptor_layout_type = 0;

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec2 tex_coords;

layout (binding = 0) uniform sampler2D diffuse_texture;
layout (binding = 1) uniform sampler2D emissive_texture; // Optional emissive texture

layout (push_constant) uniform PushConstants {
    vec3 color;
    float intensity;
} push_constants;

void main() {
    vec3 tex_color = texture(diffuse_texture, tex_coords).rgb;
    vec3 emissive = texture(emissive_texture, tex_coords).rgb * push_constants.color * push_constants.intensity;
    frag_color = vec4(tex_color + emissive, 1.0);
}
