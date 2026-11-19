float cascade_split(uint index, float near_dist, float far_dist) {
    float t = float(index + 1u) / float(cascade_count);
    float uniform_split = near_dist + (far_dist - near_dist) * t;
    float log_split = near_dist * pow(far_dist / near_dist, t);
    return lerp(uniform_split, log_split, cascade_split_blend);
}

shadow_tile cascade_tile(tile_request req, light l) {
    shadow_config cfg = get_shadow_config(clamp(pc.shadow_quality, 0, 4));
    float far_range = max(cfg.max_range, cascade_near * 2.0);
    float d0 = req.face == 0u ? 0.0 : cascade_split(req.face - 1u, cascade_near, far_range);
    float d1 = cascade_split(req.face, cascade_near, far_range);

    float3 camera_pos = mul(camera_ubo.inv_view, float4(0.0, 0.0, 0.0, 1.0)).xyz;
    float3 camera_fwd = normalize(mul(camera_ubo.inv_view, float4(0.0, 0.0, -1.0, 0.0)).xyz);
    float4 corner_clip = mul(camera_ubo.proj, float4(1.0, 1.0, -1.0, 1.0));
    float tan_x = 1.0 / max(abs(corner_clip.x / corner_clip.w), 1e-4);
    float tan_y = 1.0 / max(abs(corner_clip.y / corner_clip.w), 1e-4);
    float k2 = tan_x * tan_x + tan_y * tan_y;

    float center_dist = min((d0 + d1) * (1.0 + k2) * 0.5, d1);
    float radius = sqrt((d1 - center_dist) * (d1 - center_dist) + d1 * d1 * k2);
    radius = ceil(radius * 16.0) / 16.0;
    float3 center = camera_pos + camera_fwd * center_dist;

    cube_face basis = basis_along(l.world_direction);
    float texel = 2.0 * radius / float(req.atlas_extent);
    float px = dot(center, basis.right);
    float py = dot(center, basis.up);
    center += basis.right * (round(px / texel) * texel - px) + basis.up * (round(py / texel) * texel - py);

    float near_z = -(radius + caster_reach);
    float far_z = radius;

    shadow_tile t;
    t.view_projection = reversed_orthographic_view_projection(center, basis, radius, near_z, far_z);
    t.origin = center;
    t.texel_size = texel;
    t.atlas_offset = req.atlas_offset;
    t.atlas_extent = req.atlas_extent;
    t.perspective = 0u;
    t.texel_slope = 0.0;
    t.far_distance = d1;
    t.depth_range = far_z - near_z;
    t.near_plane = 0.0;
    return t;
}

shadow_tile local_tile(tile_request req, light l) {
    cube_face basis;
    float half_angle;
    if (l.light_type == light_type::spot) {
        basis = basis_along(l.world_direction);
        half_angle = min(acos(clamp(l.outer_cut_off, -1.0, 1.0)) + local_fov_margin, spot_max_half_angle);
    }
    else {
        basis = cube_face_basis(req.face);
        half_angle = radians(45.0) + local_fov_margin;
    }
    float tan_half = tan(half_angle);

    shadow_tile t;
    t.view_projection = reversed_perspective_view_projection(l.world_position, basis, tan_half, local_near_plane);
    t.origin = l.world_position;
    t.texel_size = 0.0;
    t.atlas_offset = req.atlas_offset;
    t.atlas_extent = req.atlas_extent;
    t.perspective = 1u;
    t.texel_slope = 2.0 * tan_half / float(req.atlas_extent);
    t.far_distance = 0.0;
    t.depth_range = 0.0;
    t.near_plane = local_near_plane;
    return t;
}

void raster_shadow_setup(uint3 dispatch_id) {
    uint index = dispatch_id.x;
    if (index >= pc.tile_count) {
        return;
    }
    tile_request req = tile_requests[index];
    light l = lights[req.light_index];
    if (l.light_type == light_type::directional) {
        tiles_out[index] = cascade_tile(req, l);
    }
    else {
        tiles_out[index] = local_tile(req, l);
    }
}
