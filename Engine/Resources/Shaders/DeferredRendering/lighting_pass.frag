#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(constant_id = 99) const int descriptor_layout_type = 2;

layout(location = 0) in  vec2 tex_coords;
layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec4 bright_color;

#define MAX_LIGHTS 10

layout(set=0, binding=0) uniform sampler2D g_position;
layout(set=0, binding=1) uniform sampler2D g_normal;
layout(set=0, binding=2) uniform sampler2D g_albedo_spec;

layout(set=0, binding=3) uniform sampler2D   shadow_maps[ MAX_LIGHTS ];
layout(set=0, binding=4) uniform samplerCube shadow_cube_maps[ MAX_LIGHTS ];

layout(set=0, binding=5) uniform LightSpaceBlock {
    mat4 light_space_matrices[ MAX_LIGHTS ];
};

layout(set=0, binding=6) uniform sampler2D   diffuse_texture;
layout(set=0, binding=7) uniform samplerCube environment_map;

layout(set=0, binding=8) buffer Lights {
    Light lights[];
};

layout(push_constant) uniform PushConstants {
    vec3 view_pos;
    float bloom_threshold;
    bool  depth_map_debug;
    bool  brightness_extraction_debug;
} push_constants;

struct Light {
    int light_type;
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

layout (binding = 8, std140) buffer Lights {
    Light lights[];
};

float calculate_shadow(vec4 frag_pos_light_space, sampler2D shadow_map, vec3 light_dir, vec3 frag_to_light, float inner_cut_off, float outer_cut_off) {
    vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
    proj_coords = proj_coords * 0.5 + 0.5;

    if (proj_coords.z > 1.0 || proj_coords.x < 0.0 || proj_coords.x > 1.0 || proj_coords.y < 0.0 || proj_coords.y > 1.0) {
        return 0.0;
    }

    float closest_depth = texture(shadow_map, proj_coords.xy).r;
    float current_depth = proj_coords.z;
    float bias = max(0.005 * (1.0 - dot(normalize(frag_to_light), normalize(light_dir))), 0.005);

    float shadow = 0.0;
    vec2 texel_size = 1.0 / textureSize(shadow_map, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcf_depth = texture(shadow_map, proj_coords.xy + vec2(x, y) * texel_size).r;
            shadow += current_depth - bias > pcf_depth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}

void main() {
    if (push_constants.depth_map_debug) {
        float depth_value = texture(shadow_maps[0], tex_coords).r;
        frag_color = vec4(vec3(depth_value), 1.0);
        return;
    }

    vec3 frag_pos = texture(g_position, tex_coords).rgb;
    vec3 normal = normalize(texture(g_normal, tex_coords).rgb);
    vec3 albedo = texture(g_albedo_spec, tex_coords).rgb;
    float specular = texture(g_albedo_spec, tex_coords).a;
    vec3 view_dir = normalize(push_constants.view_pos - frag_pos);

    vec3 result_color = vec3(0.0);
    float total_attenuation = 0.0;

    for (int i = 0; i < lights.length(); ++i) {
        vec3 light_dir = normalize(lights[i].position - frag_pos);
        vec3 reflect_dir = reflect(-light_dir, normal);
        vec3 halfway_dir = normalize(light_dir + view_dir);
        vec3 ambient = lights[i].ambient_strength * lights[i].color;
        float diff = max(dot(normal, light_dir), 0.0);
        vec3 diffuse = diff * lights[i].color;
        float spec = pow(max(dot(view_dir, reflect_dir), 0.0), specular);
        vec3 specular_component = lights[i].color * spec * 0.5;

        vec4 frag_pos_light_space = light_space_matrices[i] * vec4(frag_pos, 1.0);
        float shadow = 0.0;

        if (lights[i].light_type == 0) { // Directional Light
            shadow = 1.0 - calculate_shadow(frag_pos_light_space, shadow_maps[i], -lights[i].direction, -light_dir, lights[i].cut_off, lights[i].outer_cut_off);
            result_color += (ambient + shadow * (diffuse + specular_component)) * albedo;
            total_attenuation += 1.0;
        } else if (lights[i].light_type == 1) { // Point Light
            float distance = length(lights[i].position - frag_pos);
            float attenuation = 1.0 / (lights[i].constant + lights[i].linear * distance + lights[i].quadratic * (distance * distance));
            diffuse *= attenuation * lights[i].intensity;
            specular_component *= attenuation * lights[i].intensity;
            result_color += (ambient + diffuse + specular_component) * albedo;
            total_attenuation += attenuation;
        } else { // Spot Light
            float distance = length(lights[i].position - frag_pos);
            float theta = dot(normalize(light_dir), normalize(-lights[i].direction));

            if (theta > lights[i].cut_off) {
                float epsilon = lights[i].cut_off - lights[i].outer_cut_off;
                float intensity = clamp((theta - lights[i].outer_cut_off) / epsilon, 0.0, 1.0) * lights[i].intensity;
                shadow = 1.0 - calculate_shadow(frag_pos_light_space, shadow_maps[i], lights[i].direction, light_dir, lights[i].cut_off, lights[i].outer_cut_off);
                float attenuation = 1.0 / (lights[i].constant + lights[i].linear * distance + lights[i].quadratic * (distance * distance));
                diffuse *= attenuation * intensity;
                specular_component *= attenuation * intensity;
                result_color += (ambient + shadow * (diffuse + specular_component)) * albedo;
                total_attenuation += attenuation;
            } else {
                result_color += ambient * albedo;
            }
        }
    }

    total_attenuation = clamp(total_attenuation, 0.0, 1.0);

    vec3 reflection_dir = reflect(-view_dir, normal);
    vec3 reflection_color = texture(environment_map, reflection_dir).rgb;
    float fresnel = pow(1.0 - max(dot(view_dir, normal), 0.0), 5.0);
    float reflectivity = 0.1 * fresnel * total_attenuation;
    result_color = mix(result_color, reflection_color, reflectivity);

    frag_color = vec4(result_color, 1.0);

    float brightness = dot(frag_color.rgb, vec3(0.2126, 0.7152, 0.0722));
    if (brightness > push_constants.bloom_threshold) 
        bright_color = vec4(frag_color.rgb, 1.0);
    else
        bright_color = vec4(0.0, 0.0, 0.0, 1.0);

    if (push_constants.brightness_extraction_debug) frag_color = bright_color;
}
