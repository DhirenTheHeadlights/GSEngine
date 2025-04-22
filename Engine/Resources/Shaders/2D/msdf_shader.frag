#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99) const int descriptor_layout_type = 3;

layout (location = 0) in vec2 frag_tex_coord;
layout (location = 0) out vec4 out_color;

layout (binding = 1) uniform sampler2D msdf_texture;

layout (binding = 2) uniform MSDFUniforms {
    vec4 color;
    float range;
} msdf_uniforms;

float median_3(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    vec3 tex_sample = texture(msdf_texture, frag_tex_coord).rgb;
    float dist = median_3(tex_sample.r, tex_sample.g, tex_sample.b);
    float pixel_width = fwidth(dist);
    float alpha = smoothstep(0.5 - pixel_width, 0.5 + pixel_width, dist);
    out_color = vec4(msdf_uniforms.color.rgb, msdf_uniforms.color.a * alpha);
}
