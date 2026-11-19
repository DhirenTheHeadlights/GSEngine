float shadow_atlas_depth(bool local_atlas, int2 texel) {
    return local_atlas
        ? shadow_local_atlas.Load(int3(texel, 0)).r
        : shadow_cascade_atlas.Load(int3(texel, 0)).r;
}

float sample_shadow_tile(shadow_tile tile, bool local_atlas, float3 world_pos, float3 normal, float light_distance, int kernel) {
    float footprint = tile.perspective != 0u ? tile.texel_slope * light_distance : tile.texel_size;
    float4 clip = mul(tile.view_projection, float4(world_pos + normal * footprint * 1.5, 1.0));
    if (clip.w <= 0.0) {
        return 1.0;
    }
    float3 ndc = clip.xyz / clip.w;
    if (any(abs(ndc.xy) > 1.0) || ndc.z < 0.0 || ndc.z > 1.0) {
        return 1.0;
    }

    float2 texel_pos = (ndc.xy * 0.5 + 0.5) * float(tile.atlas_extent);
    int2 tile_min = int2(tile.atlas_offset);
    int2 tile_max = tile_min + int(tile.atlas_extent) - 1;
    int2 base = tile_min + int2(floor(texel_pos)) - (kernel - 1) / 2;

    float lit = 0.0;
    for (int y = 0; y < kernel; ++y) {
        for (int x = 0; x < kernel; ++x) {
            float occluder = shadow_atlas_depth(local_atlas, clamp(base + int2(x, y), tile_min, tile_max));
            bool visible;
            if (tile.perspective != 0u) {
                float occluder_distance = occluder > 0.0 ? tile.near_plane / occluder : 1e30;
                visible = occluder_distance >= clip.w - 2.0 * footprint;
            }
            else {
                visible = occluder <= ndc.z + 2.0 * footprint / tile.depth_range;
            }
            lit += visible ? 1.0 : 0.0;
        }
    }
    return lit / float(kernel * kernel);
}

GiGridParams forward_gi_params() {
    GiGridParams gi_params;
    gi_params.origin = pc.gi_origin;
    gi_params.spacing = pc.gi_spacing;
    gi_params.grid_dim = pc.gi_grid_dim;
    gi_params.intensity = pc.gi_intensity;
    gi_params.atlas_size = pc.gi_atlas_size;
    return gi_params;
}

struct raster_lighting : IShadowVisibility, IAmbientOcclusion, IReflection {
    static float shadow_visibility(uint light_index, light l, float3 origin, float3 normal, float3 direction, float t_max, float2 pixel, shadow_config cfg, float view_dist) {
        if (view_dist > cfg.max_range) return 1.0;

        uint2 slots = light_shadow_tiles[light_index];
        if (slots.y == 0u) {
            return 1.0;
        }

        int kernel = clamp(pc.shadow_quality, 1, 4);
        float shadow = 1.0;
        if (l.light_type == light_type::directional) {
            uint cascade = 0u;
            while (cascade < slots.y && view_dist > shadow_tiles[slots.x + cascade].far_distance) {
                ++cascade;
            }
            if (cascade < slots.y) {
                shadow = sample_shadow_tile(shadow_tiles[slots.x + cascade], false, origin, normal, 0.0, kernel);
            }
        }
        else {
            float3 from_light = origin - l.world_position;
            uint face = l.light_type == light_type::point ? major_cube_face(from_light) : 0u;
            shadow = sample_shadow_tile(shadow_tiles[slots.x + face], true, origin, normal, length(from_light), kernel);
        }

        if (view_dist > cfg.fade_start) {
            shadow = lerp(shadow, 1.0, (view_dist - cfg.fade_start) / (cfg.max_range - cfg.fade_start));
        }
        return shadow;
    }

    static float ambient_occlusion(float3 origin, float3 normal, float2 pixel, ao_config cfg, float view_dist) {
        if (view_dist > cfg.max_range) return 1.0;

        float occlusion = ambient_occlusion_in.Load(int3(int2(pixel), 0)).r;
        if (view_dist > cfg.fade_start) {
            occlusion = lerp(occlusion, 1.0, (view_dist - cfg.fade_start) / (cfg.max_range - cfg.fade_start));
        }
        return occlusion;
    }

    static float3 reflection(float3 origin, float3 normal, float3 view_dir, float roughness, float2 pixel, reflection_config cfg, float view_dist, float3 F) {
        if (view_dist > cfg.max_range || roughness > cfg.max_roughness) return float3(0.0);

        float3 reflect_dir = reflect(-view_dir, normal);
        float3 fallback = sky_radiance_toward(reflect_dir);
        if (pc.gi_enabled != 0) {
            float coverage;
            float3 probe_irradiance = sample_gi_irradiance(origin, reflect_dir, forward_gi_params(), gi_atlas, gi_atlas_sampler, coverage);
            fallback = lerp(fallback, probe_irradiance / PI, coverage);
        }

        float4 screen = reflection_in.Load(int3(int2(pixel), 0));
        float3 reflection = lerp(fallback, screen.rgb, screen.a);

        if (view_dist > cfg.fade_start) {
            reflection *= 1.0 - saturate((view_dist - cfg.fade_start) / max(cfg.max_range - cfg.fade_start, 1e-4));
        }
        return reflection * F;
    }
};

typealias lighting_technique = raster_lighting;
