[shader("vertex")]
DepthOutput vs_main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID) {
    grass_params p = grass_params_in[0];
    instance_data inst = instance_data_buffer[0];
    uint tile = grass_visible[pc.view * p.tile_count + instance_id];
    grass_surface_vertex v = grass_tile_vertex(p, tile, grass_tiles[tile].slot_plus_one, vertex_id);

    precise float4 world = mul(inst.model_matrix, float4(v.position, 1.0));
    precise float4 clip = mul(camera_ubo.proj, mul(camera_ubo.view, world));
    precise float4 prev_world = mul(inst.prev_model_matrix, float4(v.previous, 1.0));
    precise float4 prev_clip = mul(camera_ubo.prev_proj, mul(camera_ubo.prev_view, prev_world));

    DepthOutput o;
    o.clip_pos = clip;
    o.curr_clip_pos = clip;
    o.prev_clip_pos = prev_clip;
    return o;
}

[shader("fragment")]
float4 fs_main(DepthOutput input) : SV_Target0 {
    return float4(depth_velocity(input), 0.0, 0.0);
}
