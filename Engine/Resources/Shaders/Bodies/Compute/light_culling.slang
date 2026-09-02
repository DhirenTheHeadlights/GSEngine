
groupshared uint tile_light_count;
groupshared uint tile_light_indices[max_lights_per_tile];
groupshared uint tile_depth_min;
groupshared uint tile_depth_max;

float2 screen_uv(float2 pixel) {
    return pixel / float2(culling_params.screen_size);
}

float3 reconstruct_view_position(float2 uv, float depth) {
    float2 ndc_xy = uv * 2.0 - 1.0;
    float4 clip = float4(ndc_xy, depth, 1.0);
    float4 view_pos_h = mul(culling_params.inv_proj, clip);
    return view_pos_h.xyz / view_pos_h.w;
}

struct frustum {
    float4 planes[4];
};

float4 make_plane_from_rays(float3 a, float3 b) {
    return float4(normalize(cross(a, b)), 0.0);
}

frustum build_tile_frustum(uint2 tile_id) {
    uint2 pixel_min = tile_id * tile_size;
    uint2 pixel_max = min(pixel_min + uint2(tile_size, tile_size), culling_params.screen_size);

    float3 top_left = normalize(reconstruct_view_position(screen_uv(float2(pixel_min.x, pixel_min.y)), 1.0));
    float3 top_right = normalize(reconstruct_view_position(screen_uv(float2(pixel_max.x, pixel_min.y)), 1.0));
    float3 bottom_left = normalize(reconstruct_view_position(screen_uv(float2(pixel_min.x, pixel_max.y)), 1.0));
    float3 bottom_right = normalize(reconstruct_view_position(screen_uv(float2(pixel_max.x, pixel_max.y)), 1.0));

    frustum f;
    f.planes[0] = make_plane_from_rays(bottom_left, top_left);
    f.planes[1] = make_plane_from_rays(top_right, bottom_right);
    f.planes[2] = make_plane_from_rays(top_left, top_right);
    f.planes[3] = make_plane_from_rays(bottom_right, bottom_left);
    return f;
}

bool sphere_inside_frustum(float3 center, float radius, frustum f, float z_near, float z_far) {
    float center_depth = -center.z;
    if (center_depth + radius < z_near || center_depth - radius > z_far) {
        return false;
    }

    for (uint i = 0; i < 4; ++i) {
        if (dot(f.planes[i].xyz, center) + f.planes[i].w < -radius) {
            return false;
        }
    }

    return true;
}

void light_culling(uint3 group_id, uint3 group_thread_id, uint group_index) {
    uint2 tile_count = (culling_params.screen_size + tile_size - 1) / tile_size;
    uint tile_index = group_id.y * tile_count.x + group_id.x;
    uint2 pixel = group_id.xy * tile_size + group_thread_id.xy;

    if (group_index == 0) {
        tile_light_count = 0;
        tile_depth_min = 0x7F7FFFFF;
        tile_depth_max = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    float depth = 1.0;
    if (pixel.x < culling_params.screen_size.x && pixel.y < culling_params.screen_size.y) {
        float2 uv = screen_uv(float2(pixel) + 0.5);
        depth = depth_texture.SampleLevel(depth_sampler, uv, 0).r;
    }

    if (depth < 1.0 - 1e-5) {
        float view_depth = -reconstruct_view_position(screen_uv(float2(pixel) + 0.5), depth).z;
        uint d_bits = asuint(view_depth);
        InterlockedMin(tile_depth_min, d_bits);
        InterlockedMax(tile_depth_max, d_bits);
    }

    GroupMemoryBarrierWithGroupSync();

    frustum tile_frustum = build_tile_frustum(group_id.xy);
    float z_near = tile_depth_min == 0x7F7FFFFF ? 0.0 : asfloat(tile_depth_min);
    float z_far  = tile_depth_min == 0x7F7FFFFF ? 1e30 : asfloat(tile_depth_max);

    for (uint i = group_index; i < culling_params.num_lights; i += tile_size * tile_size) {
        light l = lights[i];
        bool visible;
        if (l.light_type == light_type::directional) {
            visible = true;
        } else {
            visible = sphere_inside_frustum(l.position, l.radius, tile_frustum, z_near, z_far);
        }
        if (visible) {
            uint slot;
            InterlockedAdd(tile_light_count, 1, slot);
            if (slot < max_lights_per_tile) {
                tile_light_indices[slot] = i;
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    uint count = min(tile_light_count, max_lights_per_tile);
    uint list_offset = tile_index * max_lights_per_tile;

    if (group_index == 0) {
        tile_light_table[tile_index] = uint2(list_offset, count);
    }

    for (uint j = group_index; j < count; j += tile_size * tile_size) {
        light_index_list[list_offset + j] = tile_light_indices[j];
    }
}
