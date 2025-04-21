#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99) const int descriptor_layout_type = 2;

layout (location = 0) in vec3 frag_position;
layout (location = 1) in vec3 frag_normal;
layout (location = 2) in vec2 frag_tex_coord;

layout (location = 0) out vec4 out_position;
layout (location = 1) out vec4 out_normal;
layout (location = 2) out vec4 out_albedo;

layout (binding = 0) uniform sampler2D texture_sampler;

void main() {
    out_position = vec4(frag_position, 1.0);
    out_normal = vec4(normalize(frag_normal), 1.0);
    out_albedo = texture(texture_sampler, frag_tex_coord);
}
