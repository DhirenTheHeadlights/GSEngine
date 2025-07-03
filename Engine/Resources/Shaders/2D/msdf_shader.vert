#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99) const int descriptor_layout_type = 3;

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec2 in_tex_coord;

layout(push_constant) uniform PushConstants {
    mat4 projection;
    vec2 position;
    vec2 size;
    vec4 color;
    vec4 uv_rect;
} pc;

layout (location = 0) out vec2 frag_tex_coord;
layout (location = 1) out vec4 frag_color;

void main() {
    vec2 scaled_pos = in_position * pc.size + pc.position;
    gl_Position     = pc.projection * vec4(scaled_pos, 0.0, 1.0);
    vec2 flipped_uv = vec2(in_tex_coord.x, 1.0 - in_tex_coord.y);
    frag_tex_coord  = pc.uv_rect.xy + flipped_uv * pc.uv_rect.zw;
    frag_color      = pc.color;
}