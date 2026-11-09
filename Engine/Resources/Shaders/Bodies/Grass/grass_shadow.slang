struct GrassShadowOutput {
    precise float4 clip_pos : SV_Position;
};

[shader("vertex")]
GrassShadowOutput vs_main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID) {
    grass_params p = grass_params_in[0];
    instance_data inst = instance_data_buffer[0];
    uint tile = grass_visible[(grass_first_shadow_view + pc.tile_index) * p.tile_count + instance_id];
    grass_surface_vertex v = grass_tile_vertex(p, tile, grass_tiles[tile].slot_plus_one, vertex_id);

    GrassShadowOutput o;
    o.clip_pos = mul(tiles[pc.tile_index].view_projection, mul(inst.model_matrix, float4(v.position, 1.0)));
    return o;
}

[shader("fragment")]
void fs_main(GrassShadowOutput input) {
}
