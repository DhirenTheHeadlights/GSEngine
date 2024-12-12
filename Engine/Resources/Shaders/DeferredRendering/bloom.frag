#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform sampler2D bloomBlur;
uniform float exposure;
uniform bool bloom;
uniform bool hdr;
uniform float bloomIntensity;

void main() {         
    vec3 color = texture(scene, TexCoords).rgb;

    if (bloom) {
        vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
        color += bloomColor * bloomIntensity; // Apply bloom intensity
    }

    if (hdr) {
        // Apply Reinhard Tone Mapping
        vec3 mapped = vec3(1.0) - exp(-color * exposure);

        // Apply Gamma Correction
        const float gamma = 2.2;
        mapped = pow(mapped, vec3(1.0 / gamma));

        FragColor = vec4(mapped, 1.0);
    }
    else {
        const float gamma = 2.2;
        vec3 gammaCorrected = pow(color, vec3(1.0 / gamma));
        FragColor = vec4(gammaCorrected, 1.0);
    }
} 
