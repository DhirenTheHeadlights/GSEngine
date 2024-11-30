
#version 430 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

#define MAX_LIGHTS 10
uniform sampler2D shadowMaps[MAX_LIGHTS];
uniform mat4 lightSpaceMatrices[MAX_LIGHTS];

uniform vec3 viewPos;
uniform samplerCube environmentMap;

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
    float ambientStrength;
};

layout(std140, binding = 0) buffer Lights {
    Light lights[];
};

// Shadow calculation function
float calculateShadow(vec4 FragPosLightSpace, sampler2D shadowMap) {
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
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    Normal = normalize(Normal);
    vec3 Albedo = texture(gAlbedoSpec, TexCoords).rgb;
    float Specular = texture(gAlbedoSpec, TexCoords).a;

    vec3 resultColor = vec3(0.0);
    vec3 viewDir = normalize(viewPos - FragPos);
    


    for (int i = 0; i < lights.length(); ++i) {

        //General Values
        vec3 lightDir = normalize(lights[i].position - FragPos);
        vec3 reflectDir = reflect(-lightDir, Normal);
        vec3 halfwayDir = normalize(lightDir + viewDir);
        //Ambient lighting
        vec3 ambient = lights[i].ambientStrength * lights[i].color * lights[i].intensity;

        //Diffused lighting
        float diff = max(dot(Normal, lightDir), 0.0);
        vec3 diffuse = diff * lights[i].color * lights[i].intensity;

        //Specular lighting
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 2);
        vec3 specular = lights[i].color * spec * lights[i].intensity * 0.5;

        vec4 FragPosLightSpace = lightSpaceMatrices[i] * vec4(FragPos, 1.0);
        float shadow = 1.0 - calculateShadow(FragPosLightSpace, shadowMaps[i]);


        //Light-Specific Calculations
        if (lights[i].lightType == 0) { // Directional Light
            //resultColor += (ambient + shadow  * (diffuse + specular)) * Albedo;
        } 

        else if (lights[i].lightType == 1) { // Point Light
            float distance = length(lights[i].position - FragPos);
            float attenuation = 1.0 / (lights[i].constant + lights[i].linear * distance + lights[i].quadratic * (distance * distance));

            diffuse *= attenuation * shadow;
            specular *= attenuation * shadow;

        }

        else {                                         
            
            float distance = length(lights[i].position - FragPos);
            float attenuation = 1.0 / (lights[i].constant + lights[i].linear * distance + lights[i].quadratic * (distance * distance));
            float theta = dot(lightDir, normalize(-lights[i].direction));
            float epsilon = lights[i].cutOff - lights[i].outerCutOff;
            float intensity = clamp((theta - lights[i].outerCutOff) / epsilon, 0.0, 1.0);
            diffuse *= attenuation * intensity * shadow;
            specular *= attenuation * intensity * shadow;
        }

        resultColor += ((ambient + shadow  * (diffuse + specular)) * Albedo) - resultColor;

    }

    vec3 reflectDir = reflect(-viewDir, Normal);
    vec3 reflectionColor = texture(environmentMap, reflectDir).rgb;
    float reflectivity = 0.01;
    float fresnel = pow(1.0 - max(dot(viewDir, Normal), 0.0), 5.0);
    reflectivity *= fresnel;
    resultColor = mix(resultColor, reflectionColor, reflectivity);

    FragColor = vec4(resultColor, 1.0);
}
