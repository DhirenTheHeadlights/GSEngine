#version 450

layout (location = 0) in vec2 frag_tex_coord;
layout (location = 0) out vec4 out_color;

layout (binding = 1) uniform sampler2D u_msdf;
layout (binding = 2) uniform msdf_uniforms {
    vec4 color;
    float range;
} msdf_uniforms;

float median_3(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    vec3 sample = texture(u_msdf, frag_tex_coord).rgb;
    float dist = median_3(sample.r, sample.g, sample.b);
    float pixel_width = fwidth(dist);
    float alpha = smoothstep(0.5 - pixel_width, 0.5 + pixel_width, dist);
    out_color = vec4(msdf_uniforms.color.rgb, msdf_uniforms.color.a * alpha);
}
