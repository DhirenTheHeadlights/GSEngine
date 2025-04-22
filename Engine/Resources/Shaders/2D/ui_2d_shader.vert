#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99) const int descriptor_layout_type = 3;

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec2 in_tex_coord;

layout (push_constant) uniform PushConstants {
    vec2 position;
    vec2 size;
    vec4 color;
    vec4 uv_rect;
} push_constants;

layout (location = 0) out vec2 frag_tex_coord;
layout (location = 1) out vec4 frag_color;

void main() {
    vec2 scaled_pos = in_position * push_constants.size + push_constants.position;
    gl_Position = vec4(scaled_pos, 0.0, 1.0);
    
    frag_tex_coord = push_constants.uv_rect.xy + in_tex_coord * push_constants.uv_rect.zw;
    frag_color = push_constants.color;
}
