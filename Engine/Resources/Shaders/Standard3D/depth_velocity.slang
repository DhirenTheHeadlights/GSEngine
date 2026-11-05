struct DepthOutput {
    precise float4 clip_pos : SV_Position;
    precise float4 curr_clip_pos : TEXCOORD0;
    precise float4 prev_clip_pos : TEXCOORD1;
};

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

float2 depth_velocity(DepthOutput input) {
    float2 curr_ndc = input.curr_clip_pos.xy / input.curr_clip_pos.w + camera_ubo.jitter_ndc;
    float2 prev_ndc = input.prev_clip_pos.xy / input.prev_clip_pos.w + camera_ubo.prev_jitter_ndc;
    return (curr_ndc - prev_ndc) * float2(0.5, -0.5);
}
