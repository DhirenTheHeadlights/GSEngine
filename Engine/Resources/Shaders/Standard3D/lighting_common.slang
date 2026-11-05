static const float PI = 3.14159265;

float3 sky_radiance_toward(float3 dir) {
    float3 lookup = dir;
    if (lookup.y < 0.0) {
        lookup.y = 0.0;
        lookup = normalize(lookup);
    }
    return sky_view_in.SampleLevel(sky_view_sampler, sky_view_dir_to_uv(lookup), 0).rgb * atmosphere_ubo.sun_irradiance;
}

interface IShadowVisibility {
    static float shadow_visibility(uint light_index, light l, float3 origin, float3 normal, float3 direction, float t_max, float2 pixel, shadow_config cfg, float view_dist);
};

interface IAmbientOcclusion {
    static float ambient_occlusion(float3 origin, float3 normal, float2 pixel, ao_config cfg, float view_dist);
};

interface IReflection {
    static float3 reflection(float3 origin, float3 normal, float3 view_dir, float roughness, float2 pixel, reflection_config cfg, float view_dist, float3 F);
};
