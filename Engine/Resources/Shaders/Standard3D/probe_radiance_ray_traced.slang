struct ray_traced_probe_radiance : IProbeRadiance {
    static uint3 probe_coord(uint3 group_id) {
        return group_id;
    }

    static float3 radiance(uint3 group_id, float3 probe_pos, float3 dir, float3 sky_radiance) {
        float3 radiance = sky_radiance;
        RayQuery<RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_CULL_BACK_FACING_TRIANGLES> q;
        RayDesc r;
        r.Origin = probe_pos;
        r.TMin = 0.01;
        r.Direction = dir;
        r.TMax = pc.trace_t_max;
        q.TraceRayInline(scene_tlas, RAY_FLAG_NONE, 0xFF, r);
        while (q.Proceed()) {}

        if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
            uint mat_idx = q.CommittedInstanceID();
            material_data m = material_palette[mat_idx];
            float3 N = -dir;
            float3 hit_pos = probe_pos + dir * q.CommittedRayT();

            float3 incident = float3(0.0, 0.0, 0.0);
            for (uint li = 0; li < pc.light_count; ++li) {
                light l = lights[li];
                LightSample ls = evaluate_light(l, l.world_position, l.world_direction, hit_pos);
                float ndl = max(0.0, dot(N, ls.direction));
                if (ndl <= 0.0 || ls.attenuation <= 0.0) {
                    continue;
                }
                incident += l.color * l.intensity * ls.attenuation * ndl;
            }

            radiance = m.base_color * incident * (1.0 / pi);
            radiance += m.base_color * sky_radiance * surface_sky_response;
        }
        return radiance;
    }
};

typealias probe_radiance_source = ray_traced_probe_radiance;
