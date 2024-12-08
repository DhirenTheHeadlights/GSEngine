#version 430 core

layout (location = 0) in vec3 aPos;

layout (location = 0) uniform mat4 lightSpaceMatrix;
layout (location = 1) uniform mat4 model;

void main() {
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}