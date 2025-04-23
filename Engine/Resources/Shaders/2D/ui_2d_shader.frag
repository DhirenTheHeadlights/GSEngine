#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99) const int descriptor_layout_type = 3;

layout (location = 0) in vec2 frag_tex_coord;
layout (location = 1) in vec4 frag_color;

layout (binding = 1) uniform sampler2D ui_texture;
layout (location = 0) out vec4 out_color;

void main() {
    vec4 tex_color = texture(ui_texture, frag_tex_coord);
    out_color = tex_color * frag_color;
}
