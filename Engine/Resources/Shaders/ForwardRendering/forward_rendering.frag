#version 450

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec3 frag_position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 tex_coords;

layout (binding = 0) uniform sampler2D albedo_spec;
layout (binding = 1) uniform sampler2D diffuse_texture;

layout (push_constant) uniform PushConstants {
    vec3 view_pos;
    vec3 color;
    bool use_texture;
} push_constants;

struct Light {
    int light_type; // 0: Directional, 1: Point, 2: Spot
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    float cut_off;
    float outer_cut_off;
    float ambient_strength;
};

layout (binding = 2, std140) buffer Lights {
    Light lights[];
};

void main() {
    vec3 surface_albedo = push_constants.use_texture ? texture(diffuse_texture, tex_coords).rgb : push_constants.color;
    float specular = texture(albedo_spec, tex_coords).a;

    vec3 view_dir = normalize(push_constants.view_pos - frag_position);
    vec3 result_color = vec3(0.0);

    for (int i = 0; i < lights.length(); ++i) {
        vec3 light_dir = normalize(lights[i].position - frag_position);
        vec3 reflect_dir = reflect(-light_dir, normal);
        vec3 halfway_dir = normalize(light_dir + view_dir);
        vec3 ambient = lights[i].ambient_strength * lights[i].color * lights[i].intensity;
        float diff = max(dot(normal, light_dir), 0.0);
        vec3 diffuse = diff * lights[i].color * lights[i].intensity;
        float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 2);
        vec3 specular_component = lights[i].color * spec * lights[i].intensity * 0.5;
        float shadow = 1.0;

        if (lights[i].light_type == 0) { // Directional Light
            // No attenuation for directional lights
        } 
        else if (lights[i].light_type == 1) { // Point Light
            float distance = length(lights[i].position - frag_position);
            float attenuation = 1.0 / (lights[i].constant + lights[i].linear * distance + lights[i].quadratic * (distance * distance));
            diffuse *= attenuation * shadow;
            specular_component *= attenuation * shadow;
        }
        else { // Spot Light
            float distance = length(lights[i].position - frag_position);
            float attenuation = 1.0 / (lights[i].constant + lights[i].linear * distance + lights[i].quadratic * (distance * distance));
            float theta = dot(normalize(light_dir), normalize(lights[i].direction));
            float epsilon = lights[i].cut_off - lights[i].outer_cut_off;
            float intensity = clamp((theta - lights[i].outer_cut_off) / epsilon, 0.0, 1.0);
            diffuse *= attenuation * intensity;
            specular_component *= attenuation * intensity;
        }
        result_color += (ambient + shadow * (diffuse + specular_component)) * surface_albedo;
    }
    
    frag_color = vec4(result_color, 1.0);
}
