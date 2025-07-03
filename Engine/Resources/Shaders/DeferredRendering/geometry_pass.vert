#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99) const int descriptor_layout_type = 2;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera_ubo;

layout(push_constant) uniform ModelPC {
    mat4 model;
} pc;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 1) out vec3 frag_normal; 
layout(location = 2) out vec2 frag_tex_coord;

void main() {
    gl_Position = camera_ubo.proj * camera_ubo.view * pc.model * vec4(in_position, 1.0);

    frag_normal = in_normal;
    frag_tex_coord = in_tex_coord;
}
