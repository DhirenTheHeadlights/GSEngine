
groupshared MeshletPayload s_payload;

[shader("amplification")]
[numthreads(32, 1, 1)]
void as_main(uint gtid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    uint meshlet_idx = gid.x * 32 + gtid;
    uint instance_index = pc.first_instance + gid.y;
    bool visible = false;

    if (meshlet_idx < pc.meshlet_count && pc.skip_meshlet_cull != 0) {
        visible = true;
    }
    else if (meshlet_idx < pc.meshlet_count) {
        uint global_idx = pc.meshlet_offset + meshlet_idx;
        meshlet_bounds bounds = meshlet_bounds_buffer[global_idx];
        instance_data inst = instance_data_buffer[instance_index];

        float3 world_center = mul(inst.model_matrix, float4(bounds.center, 1.0)).xyz;

        float3 scale_sq = float3(
            dot(inst.model_matrix[0].xyz, inst.model_matrix[0].xyz),
            dot(inst.model_matrix[1].xyz, inst.model_matrix[1].xyz),
            dot(inst.model_matrix[2].xyz, inst.model_matrix[2].xyz)
        );
        float max_scale = sqrt(max(scale_sq.x, max(scale_sq.y, scale_sq.z)));
        float world_radius = bounds.radius * max_scale;

        visible = !frustum_cull_sphere(world_center, world_radius, camera_ubo.view, camera_ubo.proj);

        if (visible && bounds.cone_cutoff >= 0.0) {
            float3 world_cone_axis = normalize(mul((float3x3)inst.model_matrix, bounds.cone_axis));
            float3 view_center = mul(camera_ubo.view, float4(world_center, 1.0)).xyz;
            float3 view_cone_axis = normalize(mul((float3x3)camera_ubo.view, world_cone_axis));
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
        s_payload.meshlet_indices[slot] = pc.meshlet_offset + meshlet_idx;
    }
    GroupMemoryBarrierWithGroupSync();

    DispatchMesh(s_payload.count, 1, 1, s_payload);
}

struct DepthOutput {
    precise float4 clip_pos : SV_Position;
    precise float4 curr_clip_pos : TEXCOORD0;
    precise float4 prev_clip_pos : TEXCOORD1;
};

[shader("mesh")]
[numthreads(32, 1, 1)]
[outputtopology("triangle")]
void ms_main(
    uint gtid : SV_GroupThreadID,
    uint3 group_id : SV_GroupID,
    in payload MeshletPayload p,
    OutputVertices<DepthOutput, 64> out_verts,
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

        precise float4 prev_world = pc.use_prev_vertices != 0
            ? float4(prev_vertices_buffer[global_vertex_idx].position, 1.0)
            : mul(inst.prev_model_matrix, float4(v.position, 1.0));
        precise float4 prev_view_pos = mul(camera_ubo.prev_view, prev_world);
        precise float4 prev_clip = mul(camera_ubo.prev_proj, prev_view_pos);

        DepthOutput o;
        o.clip_pos = mv.clip_pos;
        o.curr_clip_pos = mv.clip_pos;
        o.prev_clip_pos = prev_clip;
        out_verts[v_idx] = o;
    }

    for (uint t_idx = gtid; t_idx < desc.triangle_count; t_idx += 32) {
        out_tris[t_idx] = load_meshlet_triangle(meshlet_triangles, desc.triangle_offset + t_idx * 3);
    }
}

[shader("fragment")]
float2 fs_main(DepthOutput input) : SV_Target0 {
    float2 curr_ndc = input.curr_clip_pos.xy / input.curr_clip_pos.w + camera_ubo.jitter_ndc;
    float2 prev_ndc = input.prev_clip_pos.xy / input.prev_clip_pos.w + camera_ubo.prev_jitter_ndc;
    return (curr_ndc - prev_ndc) * float2(0.5, -0.5);
}
