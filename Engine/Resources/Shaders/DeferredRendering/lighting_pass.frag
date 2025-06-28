#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(constant_id = 99) const int descriptor_layout_type = 1;

layout(location = 0) in vec2 inUV;

layout(set = 0, binding = 0) uniform sampler2D g_position;
layout(set = 0, binding = 1) uniform sampler2D g_normal;
layout(set = 0, binding = 2) uniform sampler2D g_albedo_spec;

layout(location = 0) out vec4 frag_color;

void main() {
    vec3 position = texture(g_position, inUV).rgb;
    vec3 normal = texture(g_normal, inUV).rgb;
    vec4 albedo = texture(g_albedo_spec, inUV);

    frag_color = vec4(albedo.rgb * normal, albedo.a); 
}
