
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
        out_verts[v_idx] = make_mesh_output(mv, p.instance_idx);
    }

    for (uint t_idx = gtid; t_idx < desc.triangle_count; t_idx += 32) {
        out_tris[t_idx] = load_meshlet_triangle(meshlet_triangles, desc.triangle_offset + t_idx * 3);
    }
}

[shader("fragment")]
float4 fs_main(MeshOutput input) : SV_Target0 {
    return shade_surface(input);
}
