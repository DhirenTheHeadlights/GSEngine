#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform bool hdr;
uniform float exposure;

void main()
{
    vec3 hdrColor = texture(scene, TexCoords).rgb;
    const float gamma = 2.2;

    if (hdr) {
        // Reinhard or exponential tone mapping
        vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
        mapped = pow(mapped, vec3(1.0 / gamma));
        FragColor = vec4(mapped, 1.0);
    } else {
        // Just gamma correction
        vec3 mapped = pow(hdrColor, vec3(1.0 / gamma));
        FragColor = vec4(mapped, 1.0);
    }
}

