import common;
import forward_3d;
import meshlet_common;

[[vk::binding(0, 1)]] StructuredBuffer<Vertex> vertices;
[[vk::binding(1, 1)]] StructuredBuffer<MeshletDescriptor> meshlets;
[[vk::binding(2, 1)]] StructuredBuffer<uint> meshletVertexIndices;
[[vk::binding(3, 1)]] ByteAddressBuffer meshletTriangles;
[[vk::binding(4, 1)]] StructuredBuffer<MeshletBounds> meshletBounds;
[[vk::binding(5, 1)]] StructuredBuffer<InstanceData> instanceData;
[[vk::binding(6, 1)]] Sampler2D diffuseSampler;

[[vk::push_constant]]
cbuffer PushConstants {
    uint meshlet_offset;
    uint meshlet_count;
    uint instance_index;
    int num_lights;
    uint2 screen_size;
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
    float3 world_pos   : TEXCOORD0;
    float3 view_pos    : TEXCOORD1;
    float3 view_normal : TEXCOORD2;
    float2 tex_coord   : TEXCOORD3;
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
        mo.view_pos = mv.view_pos;
        mo.view_normal = mv.view_normal;
        mo.tex_coord = mv.tex_coord;
        out_verts[gtid] = mo;
    }

    if (gtid < desc.triangle_count) {
        out_tris[gtid] = load_meshlet_triangle(meshletTriangles, desc.triangle_offset + gtid * 3);
    }
}

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

float pcf_shadow(int s, float2 uv, float depth, float bias) {
    float sum = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float2 duv = float2((float)x, (float)y) * shadow_texel_size;
            float closest = shadow_maps[s].SampleLevel(uv + duv, 0).r;
            sum += (depth > closest + bias) ? 0.0 : 1.0;
        }
    }
    return sum / 9.0;
}

float compute_point_shadow(int light_index, float3 frag_pos_world) {
    for (int s = 0; s < point_shadow_count; ++s) {
        if (point_shadow_light_indices[s] != light_index) {
            continue;
        }

        float3 light_to_frag = frag_pos_world - point_shadow_positions_world[s];
        float sampled_depth = point_shadow_maps[s].SampleLevel(light_to_frag, 0).r;

        float3 abs_dir = abs(light_to_frag);
        float d = max(abs_dir.x, max(abs_dir.y, abs_dir.z));

        float n = point_shadow_near[s];
        float f = point_shadow_far[s];
        float current_depth = f * (d - n) / (d * (f - n));

        float bias = 0.05;
        return (current_depth > sampled_depth + bias) ? 0.0 : 1.0;
    }
    return 1.0;
}

float compute_shadow_for_light(int light_index, float3 pos_world) {
    for (int s = 0; s < shadow_light_count; ++s) {
        if (shadow_light_indices[s] != light_index) {
            continue;
        }

        float4 light_clip = mul(shadow_view_proj[s], float4(pos_world, 1.0));

        if (light_clip.w <= 0.0) {
            return 1.0;
        }

        float3 light_ndc = light_clip.xyz / light_clip.w;

        float2 shadow_uv = light_ndc.xy * 0.5 + 0.5;

        float depth = light_ndc.z;

        if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 || shadow_uv.y < 0.0 || shadow_uv.y > 1.0) {
            return 1.0;
        }
        if (depth < 0.0 || depth > 1.0) {
            return 1.0;
        }

        float bias = 0.005;
        return pcf_shadow(s, shadow_uv, depth, bias);
    }

    return 1.0;
}

[Layout(LayoutKind::Forward3D)]
[shader("fragment")]
float4 fs_main(MeshOutput input) : SV_Target0 {
    float3 albedo = diffuseSampler.Sample(input.tex_coord).rgb;
    float3 normal_vs = normalize(input.view_normal);
    float3 pos_vs = input.view_pos;
    float3 pos_world = input.world_pos;

    float3 result = float3(0.0, 0.0, 0.0);

    uint2 tile_id = uint2(input.clip_pos.xy) / TILE_SIZE;
    uint tiles_x = (screen_size.x + TILE_SIZE - 1) / TILE_SIZE;
    uint tile_index = tile_id.y * tiles_x + tile_id.x;
    uint2 tile_info = tile_light_table[tile_index];
    uint list_offset = tile_info.x;
    uint tile_count = tile_info.y;

    for (uint t = 0; t < tile_count; ++t) {
        int i = (int)light_index_list[list_offset + t];
        Light light = lights_ssbo[i];
        float shadow = compute_shadow_for_light(i, pos_world);

        if (light.light_type == LightType::Directional) {
            float3 L = normalize(-light.direction);
            float NdotL = max(dot(normal_vs, L), 0.0);
            result += albedo * light.color * light.intensity * NdotL * shadow;
            result += albedo * light.color * light.ambient_strength;
        } else if (light.light_type == LightType::Point) {
            float3 Lvec = light.position - pos_vs;
            float NdotL = max(dot(normal_vs, normalize(Lvec)), 0.0);
            float att = attenuation_point(light.position, pos_vs, light.constant, light.linear, light.quadratic);
            float point_shadow = compute_point_shadow(i, pos_world);
            result += albedo * light.color * light.intensity * NdotL * att * point_shadow;
            result += albedo * light.color * light.ambient_strength;
        } else if (light.light_type == LightType::Spot) {
            float3 Lvec = light.position - pos_vs;
            float NdotL = max(dot(normal_vs, normalize(Lvec)), 0.0);
            float att = attenuation_point(light.position, pos_vs, light.constant, light.linear, light.quadratic);
            float sf = spot_factor(Lvec, light.direction, light.cut_off, light.outer_cut_off);
            result += albedo * light.color * light.intensity * NdotL * att * sf * shadow;
            result += albedo * light.color * light.ambient_strength;
        }
    }

    return float4(result, 1.0);
}
