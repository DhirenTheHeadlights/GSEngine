DepthOutput make_depth_output(vertex v, uint global_vertex_idx, instance_data inst) {
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
    return o;
}
