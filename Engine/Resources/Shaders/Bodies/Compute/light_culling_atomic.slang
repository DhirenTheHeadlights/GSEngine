
groupshared Atomic<uint> tile_light_count;
groupshared uint tile_light_indices[max_lights_per_tile];
groupshared Atomic<uint> tile_depth_min;
groupshared Atomic<uint> tile_depth_max;

void light_culling_atomic(uint3 group_id, uint3 group_thread_id, uint group_index) {
    uint2 tile_count = (culling_params.screen_size + tile_size - 1) / tile_size;
    uint tile_index = group_id.y * tile_count.x + group_id.x;
    uint2 pixel = group_id.xy * tile_size + group_thread_id.xy;

    if (group_index == 0) {
        tile_light_count.store(0);
        tile_depth_min.store(0x7F7FFFFF);
        tile_depth_max.store(0);
    }

    GroupMemoryBarrierWithGroupSync();

    float depth = sample_tile_depth(pixel);

    if (depth > 1e-5) {
        float view_depth = -reconstruct_view_position(screen_uv(float2(pixel) + 0.5), depth).z;
        uint d_bits = asuint(view_depth);
        tile_depth_min.min(d_bits);
        tile_depth_max.max(d_bits);
    }

    GroupMemoryBarrierWithGroupSync();

    uint depth_min_bits = tile_depth_min.load();
    frustum tile_frustum = build_tile_frustum(group_id.xy);
    float z_near = depth_min_bits == 0x7F7FFFFF ? 0.0 : asfloat(depth_min_bits);
    float z_far  = depth_min_bits == 0x7F7FFFFF ? 1e30 : asfloat(tile_depth_max.load());

    for (uint i = group_index; i < culling_params.num_lights; i += tile_size * tile_size) {
        if (light_visible_in_tile(lights[i], tile_frustum, z_near, z_far)) {
            uint slot = tile_light_count.add(1);
            if (slot < max_lights_per_tile) {
                tile_light_indices[slot] = i;
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    uint count = min(tile_light_count.load(), max_lights_per_tile);
    uint list_offset = tile_index * max_lights_per_tile;

    if (group_index == 0) {
        tile_light_table[tile_index] = uint2(list_offset, count);
    }

    for (uint j = group_index; j < count; j += tile_size * tile_size) {
        light_index_list[list_offset + j] = tile_light_indices[j];
    }
}
