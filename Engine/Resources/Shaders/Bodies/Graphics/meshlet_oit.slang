groupshared MeshletPayload s_payload;

[shader("amplification")]
[numthreads(32, 1, 1)]
void as_main(uint gtid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    uint meshlet_idx = gid.x * 32 + gtid;
    uint instance_index = pc.first_instance + gid.y;
    bool visible = false;

    if (meshlet_idx < pc.meshlet_count) {
        visible = true;
    }

    if (gtid == 0) {
        s_payload.count = 0;
        s_payload.instance_idx = instance_index;
    }
    GroupMemoryBarrierWithGroupSync();

    if (visible) {
        uint slot;
        InterlockedAdd(s_payload.count, 1, slot);
        s_payload.meshlet_indices[slot] = pc.meshlet_offset + meshlet_idx;
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
    nointerpolation uint instance_idx : TEXCOORD5;
};

[shader("mesh")]
[numthreads(32, 1, 1)]
[outputtopology("triangle")]
void ms_main(
    uint gtid : SV_GroupThreadID,
    uint3 group_id : SV_GroupID,
    in payload MeshletPayload p,
    OutputVertices<MeshOutput, 64> out_verts,
    OutputIndices<uint3, 124> out_tris
) {
    uint meshlet_global_idx = p.meshlet_indices[group_id.x];
    meshlet_descriptor desc = meshlets_buffer[meshlet_global_idx];
    instance_data inst = instance_data_buffer[p.instance_idx];

    SetMeshOutputCounts(desc.vertex_count, desc.triangle_count);

    for (uint v_idx = gtid; v_idx < desc.vertex_count; v_idx += 32) {
        uint global_vertex_idx = meshlet_vertex_indices[desc.vertex_offset + v_idx];
        vertex v = vertices_buffer[global_vertex_idx];

        MeshVertexOutput mv = compute_mesh_vertex(v, inst, camera_ubo.view, camera_ubo.proj);

        MeshOutput mo;
        mo.clip_pos = mv.clip_pos;
        mo.world_pos = mv.world_pos;
        mo.world_normal = mv.world_normal;
        mo.view_pos = mv.view_pos;
        mo.view_normal = mv.view_normal;
        mo.tex_coord = mv.tex_coord;
        mo.instance_idx = p.instance_idx;
        out_verts[v_idx] = mo;
    }

    for (uint t_idx = gtid; t_idx < desc.triangle_count; t_idx += 32) {
        out_tris[t_idx] = load_meshlet_triangle(meshlet_triangles, desc.triangle_offset + t_idx * 3);
    }
}

static const float OIT_PI = 3.14159265;

struct OitOutput {
    float4 accum  : SV_Target0;
    float4 reveal : SV_Target1;
};

[shader("fragment")]
OitOutput fs_oit_main(MeshOutput input) {
    float3 N = normalize(input.view_normal);
    float3 V = normalize(-input.view_pos);

    instance_data inst = instance_data_buffer[input.instance_idx];
    material_data mat = material_palette[inst.material_index];

    float3 tex = mat.diffuse_index == 0xFFFFFFFFu
        ? float3(1.0)
        : textures[NonUniformResourceIndex(mat.diffuse_index)].Sample(textures_sampler, input.tex_coord).rgb;
    float3 base_color = tex * mat.base_color * inst.tint.rgb;

    float3 L = normalize(mul((float3x3)camera_ubo.view, pc.sun_direction));
    float ndl = max(dot(N, L), 0.0);

    float3 H = normalize(L + V);
    float ndh = max(dot(N, H), 0.0);
    float shininess = lerp(24.0, 160.0, 1.0 - saturate(mat.roughness));
    float spec = pow(ndh, shininess) * (1.0 - mat.roughness);

    float3 sun = pc.sun_color * pc.sun_intensity;
    float3 diffuse = base_color * ndl / OIT_PI;
    float3 lit = (diffuse + spec) * sun;
    float3 ambient = base_color * pc.sun_color * pc.sun_ambient;
    float3 color = lit + ambient;

    float ndv = saturate(dot(N, V));
    float fresnel = pow(1.0 - ndv, 5.0);
    color += fresnel * sun * 0.4;

    float a = saturate(inst.tint.a + (1.0 - inst.tint.a) * fresnel);

    float z = input.clip_pos.z;
    float w = clamp(pow(min(1.0, a * 10.0) + 0.01, 3.0) * 1e8 * pow(1.0 - z * 0.9, 3.0), 1e-2, 3e3);

    OitOutput o;
    o.accum = float4(color * a, a) * w;
    o.reveal = float4(0.0, 0.0, 0.0, a);
    return o;
}
