#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99) const int descriptor_layout_type = 4;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_tex_coords;

layout (location = 0) out vec2 tex_coords;

void main() {
    tex_coords = in_tex_coords;
    gl_Position = vec4(in_position, 1.0);
}
