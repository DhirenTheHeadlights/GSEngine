#version 450

layout (location = 0) out vec3 g_position;
layout (location = 1) out vec3 g_normal;
layout (location = 2) out vec4 g_albedo_spec;

layout (location = 0) in vec3 frag_position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 tex_coords;

layout (binding = 0) uniform sampler2D diffuse_texture;

layout (push_constant) uniform PushConstants {
    vec3 color;
    bool use_texture;
} push_constants;

void main() {
    g_position = frag_position;
    g_normal = normalize(normal);
    
    if (push_constants.use_texture) {
        g_albedo_spec.rgb = texture(diffuse_texture, tex_coords).rgb;
    } else {
        g_albedo_spec.rgb = push_constants.color;
    }

    g_albedo_spec.a = 1.0;
}
