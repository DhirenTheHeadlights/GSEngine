struct unshadowed_lighting : IShadowVisibility, IAmbientOcclusion, IReflection {
    static float shadow_visibility(uint light_index, light l, float3 origin, float3 normal, float3 direction, float t_max, float2 pixel, shadow_config cfg, float view_dist) {
        return 1.0;
    }

    static float ambient_occlusion(float3 origin, float3 normal, float2 pixel, ao_config cfg, float view_dist) {
        return 1.0;
    }

    static float3 reflection(float3 origin, float3 normal, float3 view_dir, float roughness, float2 pixel, reflection_config cfg, float view_dist, float3 F) {
        if (view_dist > cfg.max_range || roughness > cfg.max_roughness) return float3(0.0);
        return sky_radiance_toward(reflect(-view_dir, normal)) * F;
    }
};

typealias lighting_technique = unshadowed_lighting;
