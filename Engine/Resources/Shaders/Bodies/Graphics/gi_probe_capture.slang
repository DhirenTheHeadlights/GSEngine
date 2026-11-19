struct CapturePayload {
    uint meshlet_indices[32];
    uint count;
    uint instance_idx;
    uint view;
};

groupshared CapturePayload s_payload;

bool face_sees_sphere(float3 relative, float radius, cube_face basis) {
    float z = dot(relative, basis.forward);
    if (z + radius < pc.near_plane) {
        return false;
    }
    float reach = z + radius * 1.4143;
    return abs(dot(relative, basis.right)) - radius * 1.4143 <= reach && abs(dot(relative, basis.up)) - radius * 1.4143 <= reach;
}

[shader("amplification")]
[numthreads(32, 1, 1)]
void as_main(uint gtid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    uint meshlet_idx = gid.x * 32 + gtid;
    uint instance_index = pc.first_instance + gid.y;
    uint view = gid.z;
    bool visible = false;

    if (meshlet_idx < pc.meshlet_count) {
        meshlet_bounds bounds = meshlet_bounds_buffer[pc.meshlet_offset + meshlet_idx];
        instance_data inst = instance_data_buffer[instance_index];
        float3 world_center = mul(inst.model_matrix, float4(bounds.center, 1.0)).xyz;
        float3 scale_sq = float3(
            dot(inst.model_matrix[0].xyz, inst.model_matrix[0].xyz),
            dot(inst.model_matrix[1].xyz, inst.model_matrix[1].xyz),
            dot(inst.model_matrix[2].xyz, inst.model_matrix[2].xyz)
        );
        float world_radius = bounds.radius * sqrt(max(scale_sq.x, max(scale_sq.y, scale_sq.z)));
        float3 relative = world_center - capture_probe_position(view / 6u);
        visible = length(relative) - world_radius <= pc.trace_t_max && face_sees_sphere(relative, world_radius, cube_face_basis(view % 6u));
    }

    if (gtid == 0) {
        s_payload.count = 0;
        s_payload.instance_idx = instance_index;
        s_payload.view = view;
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
    in payload CapturePayload p,
    OutputVertices<CaptureOutput, 64> out_verts,
    OutputIndices<uint3, 124> out_tris
) {
    meshlet_descriptor desc = meshlets_buffer[p.meshlet_indices[group_id.x]];
    instance_data inst = instance_data_buffer[p.instance_idx];

    SetMeshOutputCounts(desc.vertex_count, desc.triangle_count);

    for (uint v_idx = gtid; v_idx < desc.vertex_count; v_idx += 32) {
        uint global_vertex_idx = meshlet_vertex_indices[desc.vertex_offset + v_idx];
        vertex v = vertices_buffer[global_vertex_idx];
        float3 world = mul(inst.model_matrix, float4(v.position, 1.0)).xyz;
        out_verts[v_idx] = capture_vertex(world, p.view, inst.material_index);
    }

    for (uint t_idx = gtid; t_idx < desc.triangle_count; t_idx += 32) {
        out_tris[t_idx] = load_meshlet_triangle(meshlet_triangles, desc.triangle_offset + t_idx * 3);
    }
}

[shader("fragment")]
float4 fs_main(CaptureOutput input) : SV_Target0 {
    return shade_capture(input);
}
