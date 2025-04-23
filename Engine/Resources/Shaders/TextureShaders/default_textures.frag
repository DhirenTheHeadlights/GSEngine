#version 450

layout (location = 0) out vec3 g_position;
layout (location = 1) out vec3 g_normal;
layout (location = 2) out vec4 g_albedo_spec;

layout (location = 0) in vec3 frag_position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 tex_coords;

layout (binding = 2) uniform sampler2D texture_diffuse;
layout (binding = 4) uniform sampler2D texture_specular;
layout (binding = 5) uniform sampler2D texture_normal;
layout (binding = 6) uniform samplerCube environment_map;

layout (push_constant) uniform PushConstants {
    vec3 color;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    vec3 emission;
    vec3 view_pos;
    float shininess;
    float optical_density;
    float transparency;
    int illumination_model;
    bool use_mtl;
    bool use_diffuse_texture;
    bool use_specular_texture;
    bool use_normal_texture;
} push_constants;

void main() {
    if (!push_constants.use_mtl) {
        g_position = frag_position;
        g_normal = normalize(normal);

        if (push_constants.use_diffuse_texture) {
            g_albedo_spec.rgb = texture(texture_diffuse, tex_coords).rgb;
        } else {
            g_albedo_spec.rgb = push_constants.color;
        }

        g_albedo_spec.a = 1.0;
    } else {
        g_position = frag_position;

        // Normal Mapping
        if (push_constants.use_normal_texture) {
            g_normal = normalize(texture(texture_normal, tex_coords).rgb * 2.0 - 1.0);
        } else {
            g_normal = normalize(normal);
        }

        // Texture Sampling
        vec3 final_diffuse = push_constants.use_diffuse_texture ? texture(texture_diffuse, tex_coords).rgb : push_constants.diffuse;
        vec3 final_specular = push_constants.use_specular_texture ? texture(texture_specular, tex_coords).rgb : push_constants.specular;
        vec3 ambient_light = push_constants.ambient * final_diffuse;

        // Apply illumination model behavior
        vec3 final_color = vec3(0.0);

        if (push_constants.illumination_model == 0) {  // No lighting (constant shading)
            final_color = final_diffuse;
        } else if (push_constants.illumination_model == 1) {  // Diffuse lighting only
            final_color = ambient_light + final_diffuse;
        } else if (push_constants.illumination_model == 2) {  // Diffuse + Specular
            final_color = ambient_light + final_diffuse + final_specular;
        }

        final_color += push_constants.emission;

        // Store diffuse color in RGB and specular intensity in Alpha
        g_albedo_spec.rgb = final_diffuse;
        g_albedo_spec.a = max(final_specular.r, max(final_specular.g, final_specular.b)) * push_constants.transparency;
    }
}
