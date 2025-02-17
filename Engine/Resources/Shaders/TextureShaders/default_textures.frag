#version 430 core

out vec3 gPosition;
out vec3 gNormal;
out vec4 gAlbedoSpec;

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoords;

uniform sampler2D texture_diffuse1;   // Diffuse texture map
uniform sampler2D texture_specular1;  // Specular texture map
uniform sampler2D texture_normal1;    // Normal map (optional)

uniform vec3 color;                 // Solid color if no texture
uniform bool useDiffuseTexture;
uniform bool useSpecularTexture;
uniform bool useNormalTexture;

uniform samplerCube environmentMap;
uniform vec3 viewPos;

// Material properties
uniform bool usemtl;
uniform vec3 ambient;
uniform vec3 diffuse;
uniform vec3 specular;
uniform vec3 emission;
uniform float shininess;
uniform float optical_density;
uniform float transparency;
uniform int illumination_model;

void main() {

    if (!usemtl) {
        gPosition = FragPos;
        gNormal = normalize(Normal);

        if (useDiffuseTexture) {
            gAlbedoSpec.rgb = texture(texture_diffuse1, TexCoords).rgb;
        } 
        else {
            gAlbedoSpec.rgb = color;
        }

        gAlbedoSpec.a = 1.0;
    }

    else {
        gPosition = FragPos;

        // Normal Mapping
        if (useNormalTexture) {
            gNormal = normalize(texture(texture_normal1, TexCoords).rgb * 2.0 - 1.0);
        } else {
            gNormal = normalize(Normal);
        }

        // Color Assignments
        vec3 finalDiffuse = useDiffuseTexture ? texture(texture_diffuse1, TexCoords).rgb : diffuse;
        vec3 finalSpecular = useSpecularTexture ? texture(texture_specular1, TexCoords).rgb : specular;
        vec3 ambientLight = ambient * finalDiffuse;

            // Apply illumination model behavior
        vec3 finalColor = vec3(0.0);
    
        if (illumination_model == 0) {  // No lighting (constant shading)
            finalColor = finalDiffuse;
        }
        else if (illumination_model == 1) {  // Diffuse lighting only
            finalColor = ambientLight + finalDiffuse;
        }
        else if (illumination_model == 2) {  // Diffuse + Specular
            finalColor = ambientLight + finalDiffuse + finalSpecular;
        }
        
        finalColor += emission;

        // Store diffuse color in RGB and specular intensity in Alpha
        gAlbedoSpec.rgb = finalDiffuse;
        gAlbedoSpec.a = max(finalSpecular.r, max(finalSpecular.g, finalSpecular.b)) * transparency;
    }
}
