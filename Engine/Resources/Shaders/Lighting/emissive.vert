#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99) const int descriptor_layout_type = 1;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_tex_coords;

layout (location = 0) out vec2 tex_coords;

layout (binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
} camera_ubo;

layout (binding = 1) uniform ModelUBO {
    mat4 model;
} model_ubo;

void main() {
    gl_Position = camera_ubo.projection * camera_ubo.view * model_ubo.model * vec4(in_position, 1.0);
    tex_coords = in_tex_coords;
}
