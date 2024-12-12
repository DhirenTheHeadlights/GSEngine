#version 430 core

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

#define MAX_LIGHTS 10
uniform sampler2D shadowMaps[MAX_LIGHTS];
layout(std140, binding = 4) uniform LightSpaceBlock {
    mat4 lightSpaceMatrices[MAX_LIGHTS];
};

uniform sampler2D diffuseTexture;
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

layout(std140, binding = 6) buffer Lights {
    Light lights[];
};

uniform vec3 viewPos;
uniform bool depthMapDebug;
uniform bool brightness_extraction_debug;

float calculateShadow(vec4 FragPosLightSpace, sampler2D shadowMap, vec3 lightDir, vec3 fragToLight, float innerCutOff, float outerCutOff) {
    vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0;
    }

    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normalize(fragToLight), normalize(lightDir))), 0.005);

    // PCF (Percentage-Closer Filtering) for soft shadows
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0; // Average the samples

    return shadow;
}

void main() {
    if (depthMapDebug) {
        float depthValue = texture(shadowMaps[0], TexCoords).r;
        FragColor = vec4(vec3(depthValue), 1.0);
        return;
    }

    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    Normal = normalize(Normal);
    vec3 Albedo = texture(gAlbedoSpec, TexCoords).rgb;
    float Specular = texture(gAlbedoSpec, TexCoords).a;

    vec3 resultColor = vec3(0.0);
    vec3 viewDir = normalize(viewPos - FragPos);
    
    for (int i = 0; i < lights.length(); ++i) {
        vec3 lightDir = normalize(lights[i].position - FragPos);
        vec3 reflectDir = reflect(-lightDir, Normal);
        vec3 halfwayDir = normalize(lightDir + viewDir);
        vec3 ambient = lights[i].ambientStrength * lights[i].color;
        float diff = max(dot(Normal, lightDir), 0.0);
        vec3 diffuse = diff * lights[i].color;
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), Specular);
        vec3 specular = lights[i].color * spec * 0.5;
        vec4 FragPosLightSpace = lightSpaceMatrices[i] * vec4(FragPos, 1.0);
        float shadow = 0;

        // Directional Light
        if (lights[i].lightType == 0) {
           shadow = 1.0 - calculateShadow(FragPosLightSpace, shadowMaps[i], -lights[i].direction, -lightDir, lights[i].cutOff, lights[i].outerCutOff);

           resultColor += (ambient + shadow * (diffuse + specular)) * Albedo;
        }
        // Point Light
        else if (lights[i].lightType == 1) {
            float distance = length(lights[i].position - FragPos);
            float attenuation = 1.0 / (lights[i].constant + lights[i].linear * distance + lights[i].quadratic * (distance * distance));

            shadow = 1.0 - calculateShadow(FragPosLightSpace, shadowMaps[i], lights[i].direction, lightDir, lights[i].cutOff, lights[i].outerCutOff);

            diffuse *= attenuation * lights[i].intensity;
            specular *= attenuation * lights[i].intensity;

            resultColor += (ambient + shadow * (diffuse + specular)) * Albedo;

        }
        // Spot Light
        else {             
            float distance = length(lights[i].position - FragPos);
            float theta = dot(normalize(lightDir), normalize(-lights[i].direction));

            if (theta > lights[i].cutOff) {
				float epsilon = lights[i].cutOff - lights[i].outerCutOff;
                float intensity = clamp((theta - lights[i].outerCutOff) / epsilon, 0.0, 1.0) * lights[i].intensity;

                shadow = 1.0 - calculateShadow(FragPosLightSpace, shadowMaps[i], lights[i].direction, lightDir, lights[i].cutOff, lights[i].outerCutOff);

                float attenuation = 1.0 / (lights[i].constant + lights[i].linear * distance + lights[i].quadratic * (distance * distance));
                diffuse *= attenuation * intensity;
                specular *= attenuation * intensity;
                resultColor += (ambient + shadow * (diffuse + specular)) * Albedo;
            }

            else resultColor += ambient * Albedo;
        }
    }

    vec3 reflectDir = reflect(-viewDir, Normal);
    vec3 reflectionColor = texture(environmentMap, reflectDir).rgb;
    float reflectivity = 0.001;
    float fresnel = pow(1.0 - max(dot(viewDir, Normal), 0.0), 5.0);
    reflectivity *= fresnel;
    resultColor = mix(resultColor, reflectionColor, reflectivity);

    FragColor = vec4(resultColor, 1.0);

    // check whether fragment output is higher than threshold, if so output as brightness color
    float brightness = dot(FragColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        BrightColor = vec4(FragColor.rgb, 1.0);
    else
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);

    if (brightness_extraction_debug) FragColor = BrightColor;
}
