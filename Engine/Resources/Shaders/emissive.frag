#version 330 core

in vec2 TexCoords;

out vec4 FragColor;

uniform sampler2D diffuseTexture;
uniform sampler2D emissiveTexture; // Optional emissive texture

uniform vec3 color;
uniform float intensity;

void main() {
	vec3 texColor = texture(diffuseTexture, TexCoords).rgb;
	vec3 emissive = texture(emissiveTexture, TexCoords).rgb * color * intensity;
    FragColor = vec4(texColor + emissive, 1.0);
}