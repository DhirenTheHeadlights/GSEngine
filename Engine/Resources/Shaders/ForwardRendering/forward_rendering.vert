#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99) const int descriptor_layout_type = 2;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_tex_coords;

layout (location = 0) out vec3 frag_position;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 tex_coords;

layout (push_constant) uniform PushConstants {
    mat4 view;
    mat4 projection;
    mat4 model;
} push_constants;

void main() {
    frag_position = vec3(push_constants.model * vec4(in_position, 1.0));
    normal = normalize(mat3(transpose(inverse(push_constants.model))) * in_normal); 
    tex_coords = in_tex_coords;
    gl_Position = push_constants.projection * push_constants.view * push_constants.model * vec4(in_position, 1.0);
}
