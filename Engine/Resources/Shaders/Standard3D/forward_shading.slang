struct MeshOutput {
    precise float4 clip_pos    : SV_Position;
    float3 world_pos    : TEXCOORD0;
    float3 world_normal : TEXCOORD1;
    float3 view_pos     : TEXCOORD2;
    float3 view_normal  : TEXCOORD3;
    float2 tex_coord    : TEXCOORD4;
    nointerpolation uint instance_idx : TEXCOORD5;
};

MeshOutput make_mesh_output(MeshVertexOutput mv, uint instance_idx) {
    MeshOutput mo;
    mo.clip_pos = mv.clip_pos;
    mo.world_pos = mv.world_pos;
    mo.world_normal = mv.world_normal;
    mo.view_pos = mv.view_pos;
    mo.view_normal = mv.view_normal;
    mo.tex_coord = mv.tex_coord;
    mo.instance_idx = instance_idx;
    return mo;
}

float distribution_ggx(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 1e-5);
}

float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / max(NdotV * (1.0 - k) + k, 1e-5);
}

float geometry_smith(float NdotV, float NdotL, float roughness) {
    return geometry_schlick_ggx(NdotV, roughness) * geometry_schlick_ggx(NdotL, roughness);
}

float3 fresnel_schlick(float cos_theta, float3 F0) {
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cos_theta), 5.0);
}

float3 evaluate_pbr(float3 N, float3 V, float3 L, float3 base_color, float roughness, float metallic, float3 F0, out float3 F_out) {
    float r = max(roughness, 0.08);

    float3 H = normalize(V + L);
    float NdotV = max(dot(N, V), 0.01);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    float D = distribution_ggx(NdotH, r);
    float G = geometry_smith(NdotV, NdotL, r);
    float3 F = fresnel_schlick(HdotV, F0);
    F_out = F;

    float3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.01);
    specular = min(specular, float3(4.0));

    float3 kd = (1.0 - F) * (1.0 - metallic);
    float3 diffuse_brdf = kd * base_color / PI;

    return (diffuse_brdf + specular) * NdotL;
}

float3 env_brdf(float3 F0, float roughness, float NdotV) {
    float4 r = roughness * env_brdf_fit_c0 + env_brdf_fit_c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    float2 ab = float2(-1.04, 1.04) * a004 + r.zw;
    return F0 * ab.x + ab.y;
}

float cloud_shadow_at(float3 pos_world, float3 camera_world) {
    if (cloud_shadow_ubo.strength <= 0.0) {
        return 1.0;
    }

    float3 sun_dir = cloud_shadow_ubo.sun_direction;
    if (sun_dir.y <= 0.02) {
        return 1.0;
    }

    float2 relative_xz_km = (pos_world.xz - camera_world.xz) * 0.001;
    float height_km = pos_world.y * 0.001;
    float2 plane_xz = relative_xz_km - sun_dir.xz * (height_km / sun_dir.y);

    float2 uv = (plane_xz - cloud_shadow_ubo.origin_km) / max(cloud_shadow_ubo.extent_km, 1e-6);
    if (any(uv < 0.0) || any(uv > 1.0)) {
        return 1.0;
    }

    float transmittance = cloud_shadow_in.SampleLevel(cloud_shadow_sampler, uv, 0).r;
    return lerp(1.0, transmittance, cloud_shadow_ubo.strength);
}

float4 shade_surface(MeshOutput input) {
    float3 normal_vs = normalize(input.view_normal);
    float3 pos_vs = input.view_pos;
    float3 pos_world = input.world_pos;

    instance_data inst = instance_data_buffer[input.instance_idx];
    material_data mat = material_palette[inst.material_index];
    float3 tex_sample = mat.diffuse_index == 0xFFFFFFFFu
        ? float3(1.0)
        : material_diffuse(mat.diffuse_index, input.tex_coord);
    float3 base_color = tex_sample * mat.base_color * inst.tint.rgb;
    float3 F0 = lerp(float3(0.04), base_color, mat.metallic);

    float3 V = normalize(-pos_vs);
    float3 result = float3(0.0, 0.0, 0.0);
    float3 cam_world = mul(camera_ubo.inv_view, float4(0.0, 0.0, 0.0, 1.0)).xyz;

    uint2 tile_id = uint2(input.clip_pos.xy) / TILE_SIZE;
    uint tiles_x = (pc.screen_size.x + TILE_SIZE - 1) / TILE_SIZE;
    uint tile_index = tile_id.y * tiles_x + tile_id.x;
    uint2 tile_info = tile_light_table[tile_index];
    uint list_offset = tile_info.x;
    uint tile_count = tile_info.y;

    float3 world_n = normalize(input.world_normal);
    float2 pixel = input.clip_pos.xy;
    float view_dist = length(pos_vs);

    shadow_config scfg = get_shadow_config(clamp(pc.shadow_quality, 0, 4));

    ao_config aocfg = get_ao_config(clamp(pc.ao_quality, 0, 4));
    float ao = 1.0;
    if (aocfg.samples > 0) {
        ao = lighting_technique::ambient_occlusion(pos_world, world_n, pixel, aocfg, view_dist);
    }

    for (uint t = 0; t < tile_count; ++t) {
        int i = (int)light_index_list[list_offset + t];
        light l = lights_ssbo[i];

        float3 light_color = l.color;
        float light_intensity = l.intensity;

        LightSample ls = evaluate_light(l, l.position, l.direction, pos_vs);
        float3 L = ls.direction;
        float attenuation = ls.attenuation;
        float3 shadow_dir_world = float3(0, 0, 0);
        float shadow_t_max = 1000.0;

        if (l.light_type == light_type::directional) {
            shadow_dir_world = normalize(-l.world_direction);
        }
        else {
            float3 to_light = l.world_position - pos_world;
            float dist = length(to_light);
            shadow_dir_world = to_light / max(dist, 1e-4);
            shadow_t_max = dist - 1.0;
        }

        float NdotL = max(dot(normal_vs, L), 0.0);
        bool shadow_worth_tracing = l.light_type == light_type::directional
            || light_intensity * attenuation >= pc.shadow_cutoff;
        float shadow = 1.0;
        if (scfg.max_samples > 0 && NdotL > 0.0 && shadow_worth_tracing) {
            shadow = lighting_technique::shadow_visibility(uint(i), l, pos_world, world_n, shadow_dir_world, shadow_t_max, pixel, scfg, view_dist);
        }
        if (l.light_type == light_type::directional && NdotL > 0.0) {
            shadow *= cloud_shadow_at(pos_world, cam_world);
        }

        float3 F;
        float3 brdf = evaluate_pbr(normal_vs, V, L, base_color, mat.roughness, mat.metallic, F0, F);
        float3 Lo = brdf * light_color * light_intensity * attenuation;
        result += Lo * shadow;
    }

    result += base_color * pc.sun_color * pc.sun_ambient * ao;

    reflection_config rcfg = get_reflection_config(clamp(pc.reflection_quality, 0, 3));
    if (rcfg.max_samples > 0 && mat.roughness <= rcfg.max_roughness) {
        float3 world_V = normalize(cam_world - pos_world);
        float NdotV_world = max(dot(world_n, world_V), 0.0);
        float3 F_refl = env_brdf(F0, mat.roughness, NdotV_world);
        result += lighting_technique::reflection(pos_world, world_n, world_V, mat.roughness, pixel, rcfg, view_dist, F_refl);
    }

    if (pc.gi_enabled != 0) {
        GiGridParams gi_params;
        gi_params.origin = pc.gi_origin;
        gi_params.spacing = pc.gi_spacing;
        gi_params.grid_dim = pc.gi_grid_dim;
        gi_params.intensity = pc.gi_intensity;
        gi_params.atlas_size = pc.gi_atlas_size;
        float coverage;
        float3 gi_irradiance = sample_gi_irradiance(pos_world, world_n, gi_params, gi_atlas, gi_atlas_sampler, coverage);
        float3 sky_fill = sky_radiance_toward(world_n) * saturate(world_n.y * 0.5 + 0.5);
        result += base_color * (1.0 - mat.metallic) * lerp(sky_fill, gi_irradiance, coverage) / PI;
    }

    float2 screen_uv = pixel / float2(pc.screen_size);
    float view_dist_km = view_dist * 0.001;
    float slice = sqrt(saturate(view_dist_km / atmosphere_ubo.max_view_distance));
    float4 ap = aerial_perspective_in.SampleLevel(ap_sampler, float3(screen_uv, slice), 0);
    float3 in_scatter = ap.rgb * atmosphere_ubo.sun_irradiance;
    float transmittance = ap.a;
    result = result * transmittance + in_scatter;

    return float4(result, 1.0);
}
