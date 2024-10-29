#version 330 core

out vec3 gPosition;
out vec3 gNormal;
out vec4 gAlbedoSpec;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

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

    // Optional: Set specular/shininess in alpha channel (if needed)
    gAlbedoSpec.a = 1.0;
}
