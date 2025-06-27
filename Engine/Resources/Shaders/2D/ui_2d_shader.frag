#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99) const int descriptor_layout_type = 3;

layout (location = 0) in vec2 frag_tex_coord;
layout (location = 1) in vec4 frag_color;

layout (set = 1, binding = 0) uniform sampler2D ui_texture;
layout (location = 0) out vec4 out_color;

void main() {
    out_color = texture(ui_texture, frag_tex_coord) * frag_color;
}
