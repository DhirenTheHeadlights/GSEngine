float trace_shadow_ray(float3 origin, float3 direction, float t_max) {
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_CULL_BACK_FACING_TRIANGLES> q;
    RayDesc ray;
    ray.Origin    = origin;
    ray.Direction = direction;
    ray.TMin      = 0.0;
    ray.TMax      = t_max;
    q.TraceRayInline(scene_tlas, RAY_FLAG_NONE, 0xFF, ray);
    while (q.Proceed()) {}
    return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 0.0 : 1.0;
}

float trace_ao(float3 origin, float3 normal, float2 pixel, ao_config cfg, float view_dist) {
    if (view_dist > cfg.max_range) return 1.0;

    float3 biased_origin = origin + normal * 0.05;

    float3 up = abs(normal.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);

    float rotation = interleaved_gradient_noise(pixel) * 6.28318;
    float frame_phase = float(pc.frame_counter % 64u);
    float inv_count = 1.0 / float(cfg.samples);
    float occlusion = 0.0;

    [unroll]
    for (int s = 0; s < max_ao_samples; ++s) {
        if (s >= cfg.samples) break;
        float r = sqrt((float(s) + 0.5) * inv_count);
        float z = sqrt(1.0 - r * r);
        float theta = (float(s) + frame_phase) * GOLDEN_ANGLE + rotation;
        float3 local_dir = float3(r * cos(theta), r * sin(theta), z);
        float3 world_dir = tangent * local_dir.x + bitangent * local_dir.y + normal * local_dir.z;
        occlusion += trace_shadow_ray(biased_origin, normalize(world_dir), cfg.max_range);
    }
    occlusion *= inv_count;

    if (view_dist > cfg.fade_start) {
        occlusion = lerp(occlusion, 1.0, (view_dist - cfg.fade_start) / (cfg.max_range - cfg.fade_start));
    }

    return occlusion;
}

float3 reflected_hit_radiance(float3 hit_pos, float3 approx_normal, material_data hit_mat) {
    float3 incident = pc.sun_color * pc.sun_ambient;
    light sun = lights_ssbo[0];
    if (sun.light_type == light_type::directional) {
        incident += sun.color * sun.intensity * max(dot(approx_normal, normalize(-sun.world_direction)), 0.0);
    }
    if (pc.gi_enabled != 0) {
        GiGridParams gi_params;
        gi_params.origin = pc.gi_origin;
        gi_params.spacing = pc.gi_spacing;
        gi_params.grid_dim = pc.gi_grid_dim;
        gi_params.intensity = pc.gi_intensity;
        gi_params.atlas_size = pc.gi_atlas_size;
        float coverage;
        float3 probe_irradiance = sample_gi_irradiance(hit_pos, approx_normal, gi_params, gi_atlas, gi_atlas_sampler, coverage);
        float3 sky_fill = sky_radiance_toward(approx_normal) * saturate(approx_normal.y * 0.5 + 0.5);
        incident += lerp(sky_fill, probe_irradiance, coverage);
    }
    return hit_mat.base_color * incident / PI;
}

float3 trace_reflection(float3 origin, float3 normal, float3 view_dir, float roughness, float2 pixel, reflection_config cfg, float view_dist, float3 F) {
    if (view_dist > cfg.max_range || roughness > cfg.max_roughness) return float3(0.0);

    float3 reflect_dir = reflect(-view_dir, normal);
    float3 biased_origin = origin + normal * 0.1;

    float3 reflection = float3(0.0);
    float inv_count = 1.0 / float(cfg.max_samples);
    float frame_phase = float(pc.frame_counter % 64u);

    [unroll]
    for (int s = 0; s < max_reflection_samples; ++s) {
        if (s >= cfg.max_samples) break;

        float3 dir = reflect_dir;
        if (roughness > 0.01) {
            float3 up = abs(reflect_dir.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
            float3 tangent = normalize(cross(up, reflect_dir));
            float3 bitangent = cross(reflect_dir, tangent);
            float rotation = interleaved_gradient_noise(pixel) * 6.28318;
            float spread = roughness * roughness;
            float r = sqrt((float(s) + 0.5) * inv_count) * spread;
            float theta = (float(s) + frame_phase) * GOLDEN_ANGLE + rotation;
            dir = normalize(reflect_dir + (tangent * cos(theta) + bitangent * sin(theta)) * r);
        }

        RayQuery<RAY_FLAG_CULL_BACK_FACING_TRIANGLES> q;
        RayDesc ray;
        ray.Origin = biased_origin;
        ray.Direction = dir;
        ray.TMin = 0.0;
        ray.TMax = cfg.max_range;
        q.TraceRayInline(scene_tlas, RAY_FLAG_NONE, 0xFF, ray);
        while (q.Proceed()) {}

        if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
            uint hit_mat_idx = q.CommittedInstanceID();
            material_data hit_mat = material_palette[hit_mat_idx];
            float3 hit_pos = biased_origin + dir * q.CommittedRayT();
            reflection += reflected_hit_radiance(hit_pos, -dir, hit_mat);
        }
        else {
            reflection += sky_radiance_toward(dir);
        }
    }
    reflection *= inv_count;

    if (view_dist > cfg.fade_start) {
        reflection *= 1.0 - saturate((view_dist - cfg.fade_start) / max(cfg.max_range - cfg.fade_start, 1e-4));
    }

    return reflection * F;
}

float trace_shadow(float3 origin, float3 normal, float3 direction, float t_max, float light_radius, float2 pixel, shadow_config cfg, float view_dist) {
    if (view_dist > cfg.max_range) return 1.0;

    int sample_count = cfg.near_samples;
    if (view_dist > cfg.mid_dist) sample_count = cfg.far_samples;
    else if (view_dist > cfg.near_dist) sample_count = cfg.mid_samples;

    float range_clamp = min(t_max, cfg.max_range - view_dist);
    float3 biased_origin = origin + normal * 0.1;

    float shadow;
    if (sample_count <= 1 || light_radius <= 0.0) {
        shadow = trace_shadow_ray(biased_origin, direction, range_clamp);
    }
    else {
        float3 up = abs(direction.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
        float3 tangent = normalize(cross(up, direction));
        float3 bitangent = cross(direction, tangent);

        float jitter_scale = light_radius / max(range_clamp, 0.01);
        float rotation = interleaved_gradient_noise(pixel) * 6.28318;
        float inv_count = 1.0 / float(sample_count);
        shadow = 0.0;

        [unroll]
        for (int s = 0; s < max_shadow_samples; ++s) {
            if (s >= sample_count) break;
            float r = sqrt((float(s) + 0.5) * inv_count);
            float theta = float(s) * GOLDEN_ANGLE + rotation;
            float2 disk = float2(cos(theta), sin(theta)) * r;
            float3 jittered_dir = normalize(direction + (tangent * disk.x + bitangent * disk.y) * jitter_scale);
            shadow += trace_shadow_ray(biased_origin, jittered_dir, range_clamp);
        }
        shadow *= inv_count;
    }

    if (view_dist > cfg.fade_start) {
        shadow = lerp(shadow, 1.0, (view_dist - cfg.fade_start) / (cfg.max_range - cfg.fade_start));
    }

    return shadow;
}

struct ray_traced_lighting : IShadowVisibility, IAmbientOcclusion, IReflection {
    static float shadow_visibility(uint light_index, light l, float3 origin, float3 normal, float3 direction, float t_max, float2 pixel, shadow_config cfg, float view_dist) {
        return trace_shadow(origin, normal, direction, t_max, l.source_radius, pixel, cfg, view_dist);
    }

    static float ambient_occlusion(float3 origin, float3 normal, float2 pixel, ao_config cfg, float view_dist) {
        return trace_ao(origin, normal, pixel, cfg, view_dist);
    }

    static float3 reflection(float3 origin, float3 normal, float3 view_dir, float roughness, float2 pixel, reflection_config cfg, float view_dist, float3 F) {
        return trace_reflection(origin, normal, view_dir, roughness, pixel, cfg, view_dist, F);
    }
};

typealias lighting_technique = ray_traced_lighting;
