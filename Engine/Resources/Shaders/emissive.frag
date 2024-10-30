#version 330 core

out vec4 FragColor;
uniform vec3 color;
uniform float intensity;

void main() {
    FragColor = vec4(color * intensity, 1.0);  // Emit light color
}