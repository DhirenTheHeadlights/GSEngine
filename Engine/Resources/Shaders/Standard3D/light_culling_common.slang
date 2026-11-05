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

float sample_tile_depth(uint2 pixel) {
    if (pixel.x >= culling_params.screen_size.x || pixel.y >= culling_params.screen_size.y) {
        return 0.0;
    }
    float2 uv = screen_uv(float2(pixel) + 0.5);
    return depth_texture.SampleLevel(depth_sampler, screen_target_uv(uv, culling_params.depth_uv_scale, culling_params.depth_uv_max), 0).r;
}

bool light_visible_in_tile(light l, frustum tile_frustum, float z_near, float z_far) {
    if (l.light_type == light_type::directional) {
        return true;
    }
    return sphere_inside_frustum(l.position, l.radius, tile_frustum, z_near, z_far);
}
