#version 430 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

uniform vec3 viewPos;

struct Light {
    int lightType;       // 0 = Directional, 1 = Point, 2 = Spot
    vec3 position;       // For Point and Spot lights
    vec3 direction;      // For Directional and Spot lights
    vec3 color;
    float intensity;
    float constant;      // Attenuation for Point and Spot lights
    float linear;
    float quadratic;
    float cutOff;        // Spot lights only
    float outerCutOff;
};

layout(std140, binding = 0) buffer Lights {
    Light lights[];
};

void main() {
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = normalize(texture(gNormal, TexCoords).rgb);
    vec3 Albedo = texture(gAlbedoSpec, TexCoords).rgb;
    float Specular = texture(gAlbedoSpec, TexCoords).a;

    vec3 resultColor = vec3(0.0);

    for (int i = 0; i < lights.length(); ++i) {
        vec3 lightDir;
        float attenuation = 1.0;

        // Handle each light type
        if (lights[i].lightType == 0) { // Directional
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

        // Spot light cut-off
        if (lights[i].lightType == 2) {  // Spot light
            float theta = dot(lightDir, normalize(-lights[i].direction));
            float epsilon = lights[i].cutOff - lights[i].outerCutOff;
            float intensity = clamp((theta - lights[i].outerCutOff) / epsilon, 0.0, 1.0);
            diffuse *= intensity;
            specular *= intensity;
        }

        // Apply attenuation for Point and Spot lights
        resultColor += (diffuse + specular) * Albedo * attenuation;
    }

    FragColor = vec4(resultColor, 1.0);
}
