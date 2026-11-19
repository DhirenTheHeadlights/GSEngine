float interleaved_gradient_noise(float2 pixel) {
    return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}

static const float GOLDEN_ANGLE = 2.39996;

struct shadow_config {
    int max_samples;
    float max_range;
    float fade_start;
    int near_samples;
    int mid_samples;
    int far_samples;
    float near_dist;
    float mid_dist;
};

shadow_config make_trace_config(float4 distances, int4 samples) {
    shadow_config cfg;
    cfg.max_samples = samples.x;
    cfg.near_samples = samples.y;
    cfg.mid_samples = samples.z;
    cfg.far_samples = samples.w;
    cfg.max_range = distances.x;
    cfg.fade_start = distances.y;
    cfg.near_dist = distances.z;
    cfg.mid_dist = distances.w;
    return cfg;
}

shadow_config get_shadow_config(int quality) {
    switch (quality) {
        case 1: return make_trace_config(shadow_hard_distances, shadow_hard_samples);
        case 2: return make_trace_config(shadow_low_distances, shadow_low_samples);
        case 3: return make_trace_config(shadow_medium_distances, shadow_medium_samples);
        case 4: return make_trace_config(shadow_high_distances, shadow_high_samples);
        default: return make_trace_config(float4(0.0, 0.0, 0.0, 0.0), int4(0, 0, 0, 0));
    }
}

struct ao_config {
    int samples;
    float max_range;
    float fade_start;
};

ao_config make_ao_config(float4 distances, int4 samples) {
    ao_config cfg;
    cfg.samples = samples.y;
    cfg.max_range = distances.x;
    cfg.fade_start = distances.y;
    return cfg;
}

ao_config get_ao_config(int quality) {
    switch (quality) {
        case 1: return make_ao_config(ao_low_distances, ao_low_samples);
        case 2: return make_ao_config(ao_medium_distances, ao_medium_samples);
        case 3: return make_ao_config(ao_high_distances, ao_high_samples);
        case 4: return make_ao_config(ao_ultra_distances, ao_ultra_samples);
        default: return make_ao_config(float4(0.0, 0.0, 0.0, 0.0), int4(0, 0, 0, 0));
    }
}

struct reflection_config {
    int max_samples;
    float max_range;
    float fade_start;
    float max_roughness;
};

reflection_config make_reflection_config(float2 distances, int samples, float max_roughness) {
    reflection_config cfg;
    cfg.max_samples = samples;
    cfg.max_range = distances.x;
    cfg.fade_start = distances.y;
    cfg.max_roughness = max_roughness;
    return cfg;
}

reflection_config get_reflection_config(int quality) {
    switch (quality) {
        case 1: return make_reflection_config(reflection_low_distances, reflection_low_samples, reflection_low_max_roughness);
        case 2: return make_reflection_config(reflection_medium_distances, reflection_medium_samples, reflection_medium_max_roughness);
        case 3: return make_reflection_config(reflection_high_distances, reflection_high_samples, reflection_high_max_roughness);
        default: return make_reflection_config(float2(0.0, 0.0), 0, 0.0);
    }
}
