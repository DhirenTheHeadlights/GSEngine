void grass_tile_mesh(uint3 group_id, uint3 group_thread_id) {
    grass_params p = grass_params_in[0];
    uint per_tile = grass_tile_blade_count * grass_side_vertices;
    uint vertex_index = group_id.x * grass_sim_threads + group_thread_id.x;
    if (vertex_index >= per_tile) {
        return;
    }

    uint row = group_id.y;
    uint out_index = row * per_tile + vertex_index;
    uint tile = pc.first_tile + row;
    uint slot_plus_one = 0u;
    if (pc.dynamic != 0u) {
        uint tile_plus_one = rt_slots[row].tile_plus_one;
        if (tile_plus_one == 0u) {
            rt_vertices_out[out_index] = float4(0.0);
            return;
        }
        tile = tile_plus_one - 1u;
        slot_plus_one = row + 1u;
    }

    grass_surface_vertex v = grass_tile_vertex(p, tile, slot_plus_one, vertex_index);
    rt_vertices_out[out_index] = float4(v.position, 1.0);
}
