groupshared MeshletPayload s_payload;

static const float4x4 identity_view = float4x4(
    float4(1.0, 0.0, 0.0, 0.0),
    float4(0.0, 1.0, 0.0, 0.0),
    float4(0.0, 0.0, 1.0, 0.0),
    float4(0.0, 0.0, 0.0, 1.0)
);

[shader("amplification")]
[numthreads(32, 1, 1)]
void as_main(uint gtid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    uint meshlet_idx = gid.x * 32 + gtid;
    uint instance_index = pc.first_instance + gid.y;
    bool visible = false;

    if (meshlet_idx < pc.meshlet_count) {
        shadow_tile tile = tiles[pc.tile_index];
        meshlet_bounds bounds = meshlet_bounds_buffer[pc.meshlet_offset + meshlet_idx];
        instance_data inst = instance_data_buffer[instance_index];

        float3 world_center = mul(inst.model_matrix, float4(bounds.center, 1.0)).xyz;
        float3 scale_sq = float3(
            dot(inst.model_matrix[0].xyz, inst.model_matrix[0].xyz),
            dot(inst.model_matrix[1].xyz, inst.model_matrix[1].xyz),
            dot(inst.model_matrix[2].xyz, inst.model_matrix[2].xyz)
        );
        float world_radius = bounds.radius * sqrt(max(scale_sq.x, max(scale_sq.y, scale_sq.z)));
        visible = !frustum_cull_sphere(world_center, world_radius, identity_view, tile.view_projection);
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

struct ShadowOutput {
    precise float4 clip_pos : SV_Position;
};

[shader("mesh")]
[numthreads(32, 1, 1)]
[outputtopology("triangle")]
void ms_main(
    uint gtid : SV_GroupThreadID,
    uint3 group_id : SV_GroupID,
    in payload MeshletPayload p,
    OutputVertices<ShadowOutput, 64> out_verts,
    OutputIndices<uint3, 124> out_tris
) {
    uint meshlet_global_idx = p.meshlet_indices[group_id.x];
    meshlet_descriptor desc = meshlets_buffer[meshlet_global_idx];
    instance_data inst = instance_data_buffer[p.instance_idx];
    float4x4 view_projection = tiles[pc.tile_index].view_projection;

    SetMeshOutputCounts(desc.vertex_count, desc.triangle_count);

    for (uint v_idx = gtid; v_idx < desc.vertex_count; v_idx += 32) {
        uint global_vertex_idx = meshlet_vertex_indices[desc.vertex_offset + v_idx];
        vertex v = vertices_buffer[global_vertex_idx];
        ShadowOutput o;
        o.clip_pos = mul(view_projection, mul(inst.model_matrix, float4(v.position, 1.0)));
        out_verts[v_idx] = o;
    }

    for (uint t_idx = gtid; t_idx < desc.triangle_count; t_idx += 32) {
        out_tris[t_idx] = load_meshlet_triangle(meshlet_triangles, desc.triangle_offset + t_idx * 3);
    }
}

[shader("fragment")]
void fs_main(ShadowOutput input) {
}
