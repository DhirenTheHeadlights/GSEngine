public struct LightSample {
    public float3 direction;
    public float attenuation;
};

public float attenuation_point(float3 light_pos, float3 frag_pos, float constant, float linear_k, float quadratic, float radius) {
    float d = length(light_pos - frag_pos);
    float ratio = d / max(radius, 1e-4);
    float ratio2 = ratio * ratio;
    float window = saturate(1.0 - ratio2 * ratio2);
    return window * window / (constant + linear_k * d + quadratic * d * d);
}

public float spot_factor(float3 frag_to_light, float3 spot_dir, float cut_off, float outer_cut_off) {
    float3 L = normalize(frag_to_light);
    float3 D = normalize(spot_dir);
    float cos_theta = dot(-L, D);
    float eps = cut_off - outer_cut_off;
    float t = saturate((cos_theta - outer_cut_off) / max(eps, 1e-4));
    return t;
}

public LightSample evaluate_light(light l, float3 light_pos, float3 light_dir, float3 surface_pos) {
    LightSample s;
    s.direction = float3(0.0, 0.0, 0.0);
    s.attenuation = 1.0;

    if (l.light_type == light_type::directional) {
        s.direction = normalize(-light_dir);
        return s;
    }

    float3 to_light = light_pos - surface_pos;
    s.direction = normalize(to_light);
    s.attenuation = attenuation_point(light_pos, surface_pos, l.constant, l.linear, l.quadratic, l.radius);

    if (l.light_type == light_type::spot) {
        s.attenuation *= spot_factor(to_light, light_dir, l.cut_off, l.outer_cut_off);
    }

    return s;
}
