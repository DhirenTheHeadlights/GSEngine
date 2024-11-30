#version 430 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

out vec4 FragColor;

uniform sampler2D albedoSpec;
uniform sampler2D diffuseTexture;   // Diffuse texture map
uniform vec3 viewPos;
uniform vec3 color;                 // Solid color if no texture
uniform bool useTexture;

struct Light {
    int lightType;                // 0: Directional, 1: Point, 2: Spot
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

void main() {
    vec3 normal = normalize(Normal);
    vec3 albedo = useTexture ? texture(diffuseTexture, TexCoords).rgb : color;
    float Specular = texture(albedoSpec, TexCoords).a;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 resultColor = vec3(0.0);

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


        //Light-Specific Calculations
        if (lights[i].lightType == 0) { // Directional Light
            //resultColor += (ambient + shadow  * (diffuse + specular)) * Albedo;
        } 

        else if (lights[i].lightType == 1) { // Point Light
            float distance = length(lights[i].position - FragPos);
            float attenuation = 1.0 / (lights[i].constant + lights[i].linear * distance + lights[i].quadratic * (distance * distance));

            diffuse *= attenuation;
            specular *= attenuation;

        }

        else {                                         
            
            float distance = length(lights[i].position - FragPos);
            float attenuation = 1.0 / (lights[i].constant + lights[i].linear * distance + lights[i].quadratic * (distance * distance));
            float theta = dot(lightDir, normalize(-lights[i].direction));
            float epsilon = lights[i].cutOff - lights[i].outerCutOff;
            float intensity = clamp((theta - lights[i].outerCutOff) / epsilon, 0.0, 1.0);
            diffuse *= attenuation * intensity;
            specular *= attenuation * intensity;
        }

        resultColor += ((ambient + (diffuse + specular)) * albedo) - resultColor;
        
    }

    FragColor = vec4(resultColor, 1.0);
}
