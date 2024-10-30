#version 430 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D shadowMap; // Shadow map

uniform vec3 viewPos;
uniform mat4 lightSpaceMatrix; // Matrix to transform to light space

struct Light {
    int lightType;
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    float cutOff;
    float outerCutOff;
};

layout(std140, binding = 0) buffer Lights {
    Light lights[];
};

// Shadow calculation function
float calculateShadow(vec4 FragPosLightSpace) {
    // Transform fragment position to [0,1] space for texture lookup
    vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    // Depth of the fragment in light space
    float currentDepth = projCoords.z;

    // Compare current depth to the depth stored in the shadow map
    float closestDepth = texture(shadowMap, projCoords.xy).r;

    // Basic shadow bias to avoid shadow acne
    float shadowBias = 0.005;
    float shadow = currentDepth - shadowBias > closestDepth ? 1.0 : 0.0;

    // Return shadow factor (1 = in shadow, 0 = fully lit)
    return shadow;
}

void main() {
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = normalize(texture(gNormal, TexCoords).rgb);
    vec3 Albedo = texture(gAlbedoSpec, TexCoords).rgb;
    float Specular = texture(gAlbedoSpec, TexCoords).a;

    vec4 FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0); // Transform fragment position to light space

    vec3 resultColor = vec3(0.0);

    for (int i = 0; i < lights.length(); ++i) {
        vec3 lightDir;
        float attenuation = 1.0;

        if (lights[i].lightType == 0) { // Directional light
            lightDir = normalize(-lights[i].direction);
        } else {
            lightDir = normalize(lights[i].position - FragPos);
            float distance = length(lights[i].position - FragPos);
            attenuation = 1.0 / (lights[i].constant + lights[i].linear * distance + lights[i].quadratic * (distance * distance));
        }

        // Diffuse component
        float diff = max(dot(Normal, lightDir), 0.0);
        vec3 diffuse = diff * lights[i].color * lights[i].intensity;

        // Specular component
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, Normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), Specular);
        vec3 specular = lights[i].color * spec * lights[i].intensity;

        // Shadow factor
        float shadow = (lights[i].lightType == 0) ? calculateShadow(FragPosLightSpace) : 0.0;

        // Apply attenuation and shadow
        resultColor += ((diffuse + specular) * (1.0 - shadow) * attenuation) * Albedo;
    }

    FragColor = vec4(resultColor, 1.0);
}
