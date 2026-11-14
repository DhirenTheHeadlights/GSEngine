bool grass_outside(float4 plane, grass_params p, float3 lo, float3 hi) {
    float4 conjugate = float4(p.field_orientation.x, -p.field_orientation.yzw);
    float3 n = grass_rotate(conjugate, plane.xyz);
    float d = plane.w + dot(plane.xyz, p.field_position);
    float3 corner = float3(n.x >= 0.0 ? hi.x : lo.x, n.y >= 0.0 ? hi.y : lo.y, n.z >= 0.0 ? hi.z : lo.z);
    return dot(n, corner) + d < 0.0;
}

void grass_cull(uint3 dispatch_id) {
    grass_params p = grass_params_in[0];
    uint tile = dispatch_id.x;
    uint view = pc.first_view + dispatch_id.y;
    uint args = view * grass_args_words;

    if (tile == 0u) {
        cull_args_out[args] = grass_tile_blade_count * grass_blade_triangles * 3u;
    }
    if (tile >= p.tile_count) {
        return;
    }

    float4x4 m = pc.shadow_views != 0
        ? cull_shadow_tiles[view - grass_first_shadow_view].view_projection
        : pc.camera_view_projection;

    float size = p.spacing * float(grass_tile_blades);
    float2 lo2 = grass_tile_min(p, tile) - p.blade_reach;
    float2 hi2 = grass_tile_min(p, tile) + size + p.blade_reach;
    float3 lo = float3(lo2.x, 0.0, lo2.y);
    float3 hi = float3(hi2.x, p.blade_reach, hi2.y);

    if (grass_outside(m[3] + m[0], p, lo, hi) || grass_outside(m[3] - m[0], p, lo, hi) || grass_outside(m[3] + m[1], p, lo, hi) || grass_outside(m[3] - m[1], p, lo, hi)) {
        return;
    }

    uint slot;
    InterlockedAdd(cull_args_out[args + 1u], 1u, slot);
    cull_visible_out[view * p.tile_count + slot] = tile;
}
