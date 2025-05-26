#version 450

layout(location = 0) in vec3 frag_position;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_position;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_albedo;

const vec3 debug_color = vec3(1.0, 0.25, 0.25);

void main() {
    out_position = vec4(frag_position, 1.0);
    out_normal = vec4(normalize(frag_normal), 1.0);
    out_albedo = vec4(debug_color, 1.0);
}
