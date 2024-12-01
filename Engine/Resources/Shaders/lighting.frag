#version 430 core

out vec3 gPosition;
out vec3 gNormal;
out vec4 gAlbedoSpec;

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoords;

uniform sampler2D diffuseTexture;   // Diffuse texture map
uniform vec3 color;                 // Solid color if no texture
uniform bool useTexture;

void main() {
    gPosition = FragPos;
    gNormal = normalize(Normal);
    
    if (useTexture) {
        gAlbedoSpec.rgb = texture(diffuseTexture, TexCoords).rgb;
    } else {
        gAlbedoSpec.rgb = color;
    }

    gAlbedoSpec.a = 1.0;
}
