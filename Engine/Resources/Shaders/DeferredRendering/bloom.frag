#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform sampler2D bloomBlur; 
uniform bool bloom;
uniform float bloomIntensity;

void main() {             
    vec3 hdrColor = texture(scene, TexCoords).rgb;

    if (bloom) {
        vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
        hdrColor += bloomColor;
    }

    FragColor = vec4(hdrColor, 1.0);
}

