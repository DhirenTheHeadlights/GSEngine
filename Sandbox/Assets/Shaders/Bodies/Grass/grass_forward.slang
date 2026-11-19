[shader("vertex")]
MeshOutput vs_main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID) {
    grass_params p = grass_params_in[0];
    instance_data inst = instance_data_buffer[0];
    uint tile = grass_visible[grass_camera_view * p.tile_count + instance_id];
    grass_surface_vertex v = grass_tile_vertex(p, tile, grass_tiles[tile].slot_plus_one, vertex_id);

    precise float4 world = mul(inst.model_matrix, float4(v.position, 1.0));
    precise float4 view_pos = mul(camera_ubo.view, world);
    float3 world_n = normalize(mul((float3x3)inst.normal_matrix, v.normal));

    MeshOutput o;
    o.clip_pos = mul(camera_ubo.proj, view_pos);
    o.world_pos = world.xyz;
    o.world_normal = world_n;
    o.view_pos = view_pos.xyz;
    o.view_normal = normalize(mul((float3x3)camera_ubo.view, world_n));
    o.tex_coord = v.uv;
    o.instance_idx = 0u;
    return o;
}

[shader("fragment")]
float4 fs_main(MeshOutput input) : SV_Target0 {
    if (dot(input.view_normal, input.view_pos) > 0.0) {
        input.view_normal = -input.view_normal;
        input.world_normal = -input.world_normal;
    }
    return shade_surface(input);
}
