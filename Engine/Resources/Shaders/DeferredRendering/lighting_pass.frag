#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99) const int descriptor_layout_type = 1;

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput g_position;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput g_normal;
layout (input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput g_albedo_spec;

layout(location = 0) out vec4 frag_color;

void main() {
    vec3 frag_pos = subpassLoad(g_position).rgb;
    vec3 normal = normalize(subpassLoad(g_normal).rgb);
    vec3 albedo = subpassLoad(g_albedo_spec).rgb;
    float specular = subpassLoad(g_albedo_spec).a;

    frag_color = vec4(albedo, 1.0); 
}
