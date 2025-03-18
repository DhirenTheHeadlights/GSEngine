#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99) const int descriptor_layout_type = 4;

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec2 in_tex_coord;

layout (binding = 0) uniform uniform_buffer_object {
    mat4 projection;
    mat4 model;
} ubo;

layout (location = 0) out vec2 frag_tex_coord;

void main() {
    frag_tex_coord = in_tex_coord;
    gl_Position = ubo.projection * ubo.model * vec4(in_position, 0.0, 1.0);
}
