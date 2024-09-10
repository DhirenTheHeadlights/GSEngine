#version 330 core

out vec4 FragColor; // Output fragment color
uniform vec3 color; // Uniform for passing color from application

void main()
{
    FragColor = vec4(color, 1.0); // Use the color uniform, alpha is set to 1.0
}
