#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(constant_id = 99) const int descriptor_layout_type = 1;

#define MAX_LIGHTS 10

layout(set = 0, binding = 0) uniform sampler2D g_albedo;
layout(set = 0, binding = 1) uniform sampler2D g_normal;
layout(set = 0, binding = 2) uniform sampler2D g_depth;

layout(set = 0, binding = 3) uniform CameraParams {
    mat4 inv_pv;
    vec3 view_pos;
} cam;

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

layout(set = 0, binding = 4) readonly buffer LightsSSBO {
    Light lights[MAX_LIGHTS];
} lights_ssbo;

layout(push_constant) uniform PushConstants {
    int num_lights;
} pc;

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_frag_color;

vec3 decode_octa(vec2 e) {
    vec3 n = vec3(e, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0) n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    return normalize(n);
}

void main() {
    float z = texture(g_depth, in_uv).r;
    vec4 ndc = vec4(in_uv * 2.0 - 1.0, z, 1.0);
    vec4 pos_w_h = cam.inv_pv * ndc;
    vec3 pos_w = pos_w_h.xyz / pos_w_h.w;

    vec3 albedo = texture(g_albedo, in_uv).rgb;
    vec3 normal = decode_octa(texture(g_normal, in_uv).rg);
    float specular_power = 32.0;

    vec3 result_color = vec3(0.0);
    vec3 view_dir = normalize(cam.view_pos - pos_w);

    for (int i = 0; i < pc.num_lights; ++i) {
        Light light = lights_ssbo.lights[i];
        
        vec3 ambient = light.ambient_strength * light.color * albedo;
        vec3 light_dir;
        float attenuation = 1.0;
        float spotlight_effect = 1.0;

        if (light.light_type == 0) {
            light_dir = normalize(-light.direction);
        } else {
            light_dir = normalize(light.position - pos_w);
            float distance = length(light.position - pos_w);
            attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
        
            if (light.light_type == 2) {
                float theta = dot(light_dir, normalize(-light.direction));
                float epsilon = light.cut_off - light.outer_cut_off;
                spotlight_effect = clamp((theta - light.outer_cut_off) / epsilon, 0.0, 1.0);
            }
        }

        float diff = max(dot(normal, light_dir), 0.0);
        vec3 diffuse = diff * light.color;

        vec3 halfway_dir = normalize(light_dir + view_dir);
        float spec = pow(max(dot(normal, halfway_dir), 0.0), specular_power);
        vec3 specular = spec * light.color;

        result_color += ambient + (attenuation * light.intensity * spotlight_effect * (diffuse * albedo + specular));
    }
    
    result_color += 0.03 * albedo;

    out_frag_color = vec4(result_color, 1.0);
}