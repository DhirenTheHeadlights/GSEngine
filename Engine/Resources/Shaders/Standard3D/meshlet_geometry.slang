import common;
import forward_3d;
import meshlet_common;

[[vk::push_constant]]
cbuffer PushConstants {
    uint meshlet_offset;
    uint meshlet_count;
    uint instance_index;
    int num_lights;
    uint2 screen_size;
    int shadow_quality;
    int ao_quality;
    int reflection_quality;
};

groupshared MeshletPayload s_payload;

[Layout(LayoutKind::Forward3D)]
[shader("amplification")]
[numthreads(32, 1, 1)]
void as_main(uint gtid : SV_GroupThreadID, uint gid : SV_GroupID) {
    uint meshlet_idx = gid * 32 + gtid;
    bool visible = false;

    if (meshlet_idx < meshlet_count) {
        uint global_idx = meshlet_offset + meshlet_idx;
        MeshletBounds bounds = meshletBounds[global_idx];
        InstanceData inst = instanceData[instance_index];

        float3 world_center = mul(inst.model_matrix, float4(bounds.center, 1.0)).xyz;

        float3 scale_sq = float3(
            dot(inst.model_matrix[0].xyz, inst.model_matrix[0].xyz),
            dot(inst.model_matrix[1].xyz, inst.model_matrix[1].xyz),
            dot(inst.model_matrix[2].xyz, inst.model_matrix[2].xyz)
        );
        float max_scale = sqrt(max(scale_sq.x, max(scale_sq.y, scale_sq.z)));
        float world_radius = bounds.radius * max_scale;

        visible = !frustum_cull_sphere(world_center, world_radius, view, proj);

        if (visible && bounds.cone_cutoff >= 0.0) {
            float3 world_cone_axis = normalize(mul((float3x3)inst.model_matrix, bounds.cone_axis));
            float3 view_center = mul(view, float4(world_center, 1.0)).xyz;
            float3 view_cone_axis = normalize(mul((float3x3)view, world_cone_axis));
            float3 view_dir = normalize(-view_center);
            float cone_sin = sqrt(1.0 - bounds.cone_cutoff * bounds.cone_cutoff);
            if (dot(view_cone_axis, view_dir) < -cone_sin) {
                visible = false;
            }
        }
    }

    if (gtid == 0) {
        s_payload.count = 0;
        s_payload.instance_idx = instance_index;
    }
    GroupMemoryBarrierWithGroupSync();

    if (visible) {
        uint slot;
        InterlockedAdd(s_payload.count, 1, slot);
        s_payload.meshlet_indices[slot] = meshlet_offset + meshlet_idx;
    }
    GroupMemoryBarrierWithGroupSync();

    DispatchMesh(s_payload.count, 1, 1, s_payload);
}

struct MeshOutput {
    precise float4 clip_pos    : SV_Position;
    float3 world_pos    : TEXCOORD0;
    float3 world_normal : TEXCOORD1;
    float3 view_pos     : TEXCOORD2;
    float3 view_normal  : TEXCOORD3;
    float2 tex_coord    : TEXCOORD4;
};

[Layout(LayoutKind::Forward3D)]
[shader("mesh")]
[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void ms_main(
    uint gtid : SV_GroupThreadID,
    uint3 group_id : SV_GroupID,
    in payload MeshletPayload p,
    OutputVertices<MeshOutput, 64> out_verts,
    OutputIndices<uint3, 124> out_tris
) {
    uint meshlet_global_idx = p.meshlet_indices[group_id.x];
    MeshletDescriptor desc = meshlets[meshlet_global_idx];
    InstanceData inst = instanceData[p.instance_idx];

    SetMeshOutputCounts(desc.vertex_count, desc.triangle_count);

    if (gtid < desc.vertex_count) {
        uint global_vertex_idx = meshletVertexIndices[desc.vertex_offset + gtid];
        Vertex v = vertices[global_vertex_idx];

        MeshVertexOutput mv = compute_mesh_vertex(v, inst, view, proj);

        MeshOutput mo;
        mo.clip_pos = mv.clip_pos;
        mo.world_pos = mv.world_pos;
        mo.world_normal = mv.world_normal;
        mo.view_pos = mv.view_pos;
        mo.view_normal = mv.view_normal;
        mo.tex_coord = mv.tex_coord;
        out_verts[gtid] = mo;
    }

    if (gtid < desc.triangle_count) {
        out_tris[gtid] = load_meshlet_triangle(meshletTriangles, desc.triangle_offset + gtid * 3);
    }
}

static const float PI = 3.14159265;

float attenuation_point(float3 light_pos, float3 frag_pos, float constant, float linear_k, float quadratic) {
    float d = length(light_pos - frag_pos);
    return 1.0 / (constant + linear_k * d + quadratic * d * d);
}

float spot_factor(float3 frag_to_light, float3 spot_dir, float cut_off, float outer_cut_off) {
    float3 L = normalize(frag_to_light);
    float3 D = normalize(spot_dir);
    float cos_theta = dot(-L, D);
    float eps = cut_off - outer_cut_off;
    float t = saturate((cos_theta - outer_cut_off) / max(eps, 1e-4));
    return t;
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
    specular = min(specular, float3(32.0));

    float3 kd = (1.0 - F) * (1.0 - metallic);
    float3 diffuse_brdf = kd * base_color / PI;

    return (diffuse_brdf + specular) * NdotL;
}

float trace_shadow_ray(float3 origin, float3 direction, float t_max) {
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_CULL_BACK_FACING_TRIANGLES> q;
    RayDesc ray;
    ray.Origin    = origin;
    ray.Direction = direction;
    ray.TMin      = 0.0;
    ray.TMax      = t_max;
    q.TraceRayInline(tlas, RAY_FLAG_NONE, 0xFF, ray);
    while (q.Proceed()) {}
    return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 0.0 : 1.0;
}

float interleaved_gradient_noise(float2 pixel) {
    return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}

static const int MAX_SHADOW_SAMPLES = 4;
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

shadow_config get_shadow_config(int quality) {
    switch (quality) {
        case 1: return { 1, 50.0,  40.0,  1, 1, 1, 10.0, 30.0 };
        case 2: return { 1, 100.0, 80.0,  1, 1, 1, 20.0, 60.0 };
        case 3: return { 2, 200.0, 150.0, 2, 1, 1, 30.0, 100.0 };
        case 4: return { 4, 500.0, 400.0, 4, 2, 1, 50.0, 200.0 };
        default: return { 0, 0.0, 0.0, 0, 0, 0, 0.0, 0.0 };
    }
}

static const int MAX_AO_SAMPLES = 8;

struct ao_config {
    int max_samples;
    float max_range;
    float fade_start;
    int near_samples;
    int mid_samples;
    int far_samples;
    float near_dist;
    float mid_dist;
};

ao_config get_ao_config(int quality) {
    switch (quality) {
        case 1: return { 1, 5.0,  4.0,  1, 1, 1, 3.0, 4.0 };
        case 2: return { 2, 10.0, 8.0,  2, 1, 1, 5.0, 8.0 };
        case 3: return { 4, 20.0, 15.0, 4, 2, 1, 8.0, 15.0 };
        case 4: return { 8, 30.0, 25.0, 8, 4, 2, 10.0, 20.0 };
        default: return { 0, 0.0, 0.0, 0, 0, 0, 0.0, 0.0 };
    }
}

float trace_ao(float3 origin, float3 normal, float2 pixel, ao_config cfg, float view_dist) {
    if (view_dist > cfg.max_range) return 1.0;

    int sample_count = cfg.near_samples;
    if (view_dist > cfg.mid_dist) sample_count = cfg.far_samples;
    else if (view_dist > cfg.near_dist) sample_count = cfg.mid_samples;

    float3 biased_origin = origin + normal * 0.05;

    float3 up = abs(normal.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);

    float rotation = interleaved_gradient_noise(pixel) * 6.28318;
    float inv_count = 1.0 / float(sample_count);
    float occlusion = 0.0;

    [unroll]
    for (int s = 0; s < MAX_AO_SAMPLES; ++s) {
        if (s >= sample_count) break;
        float r = sqrt((float(s) + 0.5) * inv_count);
        float z = sqrt(1.0 - r * r);
        float theta = float(s) * GOLDEN_ANGLE + rotation;
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

static const int MAX_REFLECTION_SAMPLES = 2;

struct reflection_config {
    int max_samples;
    float max_range;
    float fade_start;
    float max_roughness;
};

reflection_config get_reflection_config(int quality) {
    switch (quality) {
        case 1: return { 1, 100.0, 80.0,  0.1 };
        case 2: return { 1, 200.0, 150.0, 0.3 };
        case 3: return { 2, 500.0, 400.0, 0.5 };
        default: return { 0, 0.0, 0.0, 0.0 };
    }
}

float3 trace_reflection(float3 origin, float3 normal, float3 view_dir, float roughness, float2 pixel, reflection_config cfg, float view_dist, float3 F) {
    if (view_dist > cfg.max_range || roughness > cfg.max_roughness) return float3(0.0);

    float3 reflect_dir = reflect(-view_dir, normal);
    float3 biased_origin = origin + normal * 0.1;

    float3 reflection = float3(0.0);
    float inv_count = 1.0 / float(cfg.max_samples);

    [unroll]
    for (int s = 0; s < MAX_REFLECTION_SAMPLES; ++s) {
        if (s >= cfg.max_samples) break;

        float3 dir = reflect_dir;
        if (roughness > 0.01 && cfg.max_samples > 1) {
            float3 up = abs(reflect_dir.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
            float3 tangent = normalize(cross(up, reflect_dir));
            float3 bitangent = cross(reflect_dir, tangent);
            float rotation = interleaved_gradient_noise(pixel) * 6.28318;
            float spread = roughness * roughness;
            float r = sqrt((float(s) + 0.5) * inv_count) * spread;
            float theta = float(s) * GOLDEN_ANGLE + rotation;
            dir = normalize(reflect_dir + (tangent * cos(theta) + bitangent * sin(theta)) * r);
        }

        RayQuery<RAY_FLAG_CULL_BACK_FACING_TRIANGLES> q;
        RayDesc ray;
        ray.Origin = biased_origin;
        ray.Direction = dir;
        ray.TMin = 0.0;
        ray.TMax = cfg.max_range;
        q.TraceRayInline(tlas, RAY_FLAG_NONE, 0xFF, ray);
        while (q.Proceed()) {}

        if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
            uint hit_mat_idx = q.CommittedInstanceID();
            MaterialData hit_mat = material_palette[hit_mat_idx];
            reflection += hit_mat.base_color * 0.8;
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
        for (int s = 0; s < MAX_SHADOW_SAMPLES; ++s) {
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

[Layout(LayoutKind::Forward3D)]
[shader("fragment")]
float4 fs_main(MeshOutput input) : SV_Target0 {
    float3 tex_sample = diffuseSampler.Sample(input.tex_coord).rgb;
    float3 normal_vs = normalize(input.view_normal);
    float3 pos_vs = input.view_pos;
    float3 pos_world = input.world_pos;

    InstanceData inst = instanceData[instance_index];
    MaterialData mat = material_palette[inst.material_index];
    float3 base_color = tex_sample * mat.base_color;
    float3 F0 = lerp(float3(0.04), base_color, mat.metallic);

    float3 V = normalize(-pos_vs);
    float3 result = float3(0.0, 0.0, 0.0);

    uint2 tile_id = uint2(input.clip_pos.xy) / TILE_SIZE;
    uint tiles_x = (screen_size.x + TILE_SIZE - 1) / TILE_SIZE;
    uint tile_index = tile_id.y * tiles_x + tile_id.x;
    uint2 tile_info = tile_light_table[tile_index];
    uint list_offset = tile_info.x;
    uint tile_count = tile_info.y;

    float3 world_n = normalize(input.world_normal);
    float2 pixel = input.clip_pos.xy;
    float view_dist = length(pos_vs);

    shadow_config scfg = get_shadow_config(clamp(shadow_quality, 0, 4));

    ao_config aocfg = get_ao_config(clamp(ao_quality, 0, 4));
    float ao = 1.0;
    if (aocfg.max_samples > 0) {
        ao = trace_ao(pos_world, world_n, pixel, aocfg, view_dist);
    }

    for (uint t = 0; t < tile_count; ++t) {
        int i = (int)light_index_list[list_offset + t];
        Light light = lights_ssbo[i];

        float3 light_color = light.color;
        float light_intensity = light.intensity;
        float light_ambient = light.ambient_strength;

        float3 L = float3(0, 0, 0);
        float attenuation = 1.0;
        float3 shadow_dir_world = float3(0, 0, 0);
        float shadow_t_max = 1000.0;

        if (light.light_type == LightType::Directional) {
            L = normalize(-light.direction);
            shadow_dir_world = normalize(-light.world_direction);
        }
        else if (light.light_type == LightType::Point) {
            float3 Lvec = light.position - pos_vs;
            L = normalize(Lvec);
            attenuation = attenuation_point(light.position, pos_vs, light.constant, light.linear, light.quadratic);
            float3 to_light = light.world_position - pos_world;
            float dist = length(to_light);
            shadow_dir_world = to_light / max(dist, 1e-4);
            shadow_t_max = dist - 1.0;
        }
        else if (light.light_type == LightType::Spot) {
            float3 Lvec = light.position - pos_vs;
            L = normalize(Lvec);
            attenuation = attenuation_point(light.position, pos_vs, light.constant, light.linear, light.quadratic);
            attenuation *= spot_factor(Lvec, light.direction, light.cut_off, light.outer_cut_off);
            float3 to_light = light.world_position - pos_world;
            float dist = length(to_light);
            shadow_dir_world = to_light / max(dist, 1e-4);
            shadow_t_max = dist - 1.0;
        }

        float NdotL = max(dot(normal_vs, L), 0.0);
        float shadow = 1.0;
        if (scfg.max_samples > 0 && NdotL > 0.0) {
            shadow = trace_shadow(pos_world, world_n, shadow_dir_world, shadow_t_max, light.source_radius, pixel, scfg, view_dist);
        }

        float3 F;
        float3 brdf = evaluate_pbr(normal_vs, V, L, base_color, mat.roughness, mat.metallic, F0, F);
        float3 Lo = brdf * light_color * light_intensity * attenuation;
        float3 ambient = base_color * light_color * light_ambient * ao;
        result += Lo * shadow + ambient;
    }

    reflection_config rcfg = get_reflection_config(clamp(reflection_quality, 0, 3));
    if (rcfg.max_samples > 0 && mat.roughness <= rcfg.max_roughness) {
        float3 cam_world = mul(inv_view, float4(0.0, 0.0, 0.0, 1.0)).xyz;
        float3 world_V = normalize(cam_world - pos_world);
        float NdotV_world = max(dot(world_n, world_V), 0.0);
        float3 F_refl = fresnel_schlick(NdotV_world, F0);
        result += trace_reflection(pos_world, world_n, world_V, mat.roughness, pixel, rcfg, view_dist, F_refl);
    }

    return float4(result, 1.0);
}
