#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99) const int descriptor_layout_type = 3;

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec2 in_tex_coord;

layout (push_constant) uniform PushConstants {
    mat4 projection;
    vec2 position;
    vec2 size;
    vec4 color;
    vec4 uv_rect;
    float rotation;
} push_constants;

layout (location = 0) out vec2 frag_tex_coord;
layout (location = 1) out vec4 frag_color;

void main() {
    vec2 unrotated_pos = in_position * push_constants.size + push_constants.position;
    vec2 pivot = push_constants.position + vec2(0.5, -0.5) * push_constants.size;

    float c = cos(push_constants.rotation);
    float s = sin(push_constants.rotation);
    mat2 rotation_matrix = mat2(c, -s, s, c);
    
    vec2 final_pos = rotation_matrix * (unrotated_pos - pivot) + pivot;

    gl_Position = push_constants.projection * vec4(final_pos, 0.0, 1.0);
    
    frag_tex_coord = push_constants.uv_rect.xy + in_tex_coord * push_constants.uv_rect.zw;
    frag_color = push_constants.color;
}