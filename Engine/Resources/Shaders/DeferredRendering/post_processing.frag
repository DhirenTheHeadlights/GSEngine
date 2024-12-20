#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform bool hdr;
uniform float exposure;

uniform sampler2D bloomBlur; 
uniform bool bloom;

void main()
{
    vec3 result = texture(scene, TexCoords).rgb;
     if (bloom) {
        vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
        result += bloomColor;
    }

    const float gamma = 2.2;

    if (hdr) {
        // Reinhard or exponential tone mapping
        vec3 mapped = vec3(1.0) - exp(-result * exposure);
        mapped = pow(mapped, vec3(1.0 / gamma));
        FragColor = vec4(mapped, 1.0);
    } else {
        // Just gamma correction
        vec3 mapped = pow(result, vec3(1.0 / gamma));
        FragColor = vec4(mapped, 1.0);
    }
}

