#version 430 core

layout (location = 0) in vec3 aPos;

layout (location = 0) uniform mat4 light_view;
layout (location = 1) uniform mat4 light_projection;
layout (location = 2) uniform mat4 model;

void main() {
    gl_Position = light_projection * light_view * model * vec4(aPos, 1.0);
}