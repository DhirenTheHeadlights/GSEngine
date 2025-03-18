#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99 ) const int descriptor_layout_type = 2;

layout (location = 0) in vec3 in_position;

layout (binding = 0) uniform LightUBO {
    mat4 light_view;
    mat4 light_projection;
} light_ubo;

layout (binding = 1) uniform ModelUBO {
    mat4 model;
} model_ubo;

void main() {
    gl_Position = light_ubo.light_projection * light_ubo.light_view * model_ubo.model * vec4(in_position, 1.0);
}
