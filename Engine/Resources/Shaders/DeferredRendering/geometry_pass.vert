#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99) const int descriptor_layout_type = 2;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_tex_coord;

layout (push_constant) uniform PushConstants {
    mat4 view;
    mat4 proj;
    mat4 model;
} push_constants;

layout (location = 0) out vec3 frag_position;
layout (location = 1) out vec3 frag_normal;
layout (location = 2) out vec2 frag_tex_coord;

void main() {
    vec4 world_position = push_constants.model * vec4(in_position, 1.0);
    frag_position = world_position.xyz;
    frag_normal = normalize(mat3(push_constants.model) * in_normal);
    frag_tex_coord  = in_tex_coord;

    gl_Position = push_constants.proj * push_constants.view * world_position;
}


