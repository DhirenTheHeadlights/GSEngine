#version 330 core

layout (location = 0) in vec3 aPos; // Input vertex position

uniform mat4 viewProjection; // Combined view and projection matrix

void main()
{
    gl_Position = viewProjection * vec4(aPos, 1.0); // Apply view-projection matrix directly
}
