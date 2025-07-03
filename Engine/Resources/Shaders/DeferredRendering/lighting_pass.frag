#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(constant_id = 99) const int descriptor_layout_type = 1;

layout(set = 0, binding = 0) uniform sampler2D g_albedo;
layout(set = 0, binding = 1) uniform sampler2D g_normal;
layout(set = 0, binding = 2) uniform sampler2D g_depth;

layout(set = 0, binding = 5) uniform CameraParams {
    mat4 invPV;
} cam;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 frag_color;

vec3 decode_octa(vec2 e) {
    vec3 n = vec3(e, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0) n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    return normalize(n);
}

void main() {
    float z   = texture(g_depth, uv).r;
    vec4 ndc  = vec4(uv * 2.0 - 1.0, z * 2.0 - 1.0, 1.0);
    vec4 posW = cam.invPV * ndc;
    posW /= posW.w;

    vec3 N = decode_octa(texture(g_normal, uv).rg);
    vec3 albedo = texture(g_albedo, uv).rgb;

    vec3 L = normalize(vec3(1,1,1));
    float diff = max(dot(N, L), 0.0);
    vec3 col = albedo * diff + 0.03 * albedo;

    frag_color = vec4(col, 1.0);
}