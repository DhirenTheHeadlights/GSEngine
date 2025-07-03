#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99) const int descriptor_layout_type = 2;

layout(location = 0) out vec3 out_albedo;
layout(location = 1) out vec2 out_normal;

layout(set = 1, binding = 2) uniform sampler2D diffuse_sampler;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera_ubo;

layout(push_constant) uniform ModelPC {
    mat4 model;
} pc;

layout(location = 1) in vec3 frag_normal;
layout(location = 2) in vec2 frag_tex_coord;

vec2 encode_octahedron(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    }
    return n.xy;
}

void main() {
    out_albedo = texture(diffuse_sampler, frag_tex_coord).rgb;
    mat3 normal_matrix = mat3(camera_ubo.view * pc.model);
    vec3 normal_view = normalize(normal_matrix * frag_normal);
    out_normal = encode_octahedron(normal_view);
}
