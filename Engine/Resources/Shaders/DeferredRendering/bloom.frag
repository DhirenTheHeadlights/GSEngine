#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform sampler2D bloomBlur; 
uniform bool hdr;
uniform bool bloom;
uniform float exposure;

void main() {             
    const float gamma = 2.2;
    vec3 hdrColor = texture(scene, TexCoords).rgb;

    if (bloom) {
        vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
        hdrColor += bloomColor; // Additive blending
    }

    // Tone mapping and gamma correction
    if (hdr) {
        vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
        mapped = pow(mapped, vec3(1.0 / gamma));
        FragColor = vec4(mapped, 1.0);
    } else {
        vec3 mapped = pow(hdrColor, vec3(1.0 / gamma));
        FragColor = vec4(mapped, 1.0);
    }
}
