#version 450

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_tex_coords;

layout (location = 0) out vec2 tex_coords;

void main() {
    tex_coords = in_tex_coords;
    gl_Position = vec4(in_position, 1.0);
}
