void grass_grid_build(uint3 dispatch_id) {
    uint bi = dispatch_id.x;
    grass_params p = params_data[0];
    if (bi >= p.body_count) {
        return;
    }

    body_state body = body_data[bi];
    body_copy_data[bi] = body;

    if (bi / 32 < p.passthrough_words && (passthrough_data[bi / 32] & (1u << (bi % 32))) != 0) {
        return;
    }

    float3 he = body.half_extents.xyz;
    if (dot(he, he) <= 0.0) {
        return;
    }

    float4 inv_field = quat_conjugate(p.field_orientation);
    float3 center = rotate_vector(inv_field, body_origin(body.position.xyz, body.orientation, body.com_local.xyz) - p.field_position);
    float4 q = quat_mul(inv_field, body.orientation);
    float3 ax = rotate_vector(q, float3(1.0, 0.0, 0.0));
    float3 ay = rotate_vector(q, float3(0.0, 1.0, 0.0));
    float3 az = rotate_vector(q, float3(0.0, 0.0, 1.0));
    float3 ext = abs(ax) * he.x + abs(ay) * he.y + abs(az) * he.z + p.contact_margin;

    if (center.y + ext.y <= 2.0 * p.contact_margin || center.y - ext.y > p.blade_reach) {
        return;
    }

    float2 swept = rotate_vector(inv_field, body.velocity.xyz).xz * (p.substep_dt * float(p.substeps));
    float2 lo = min(center.xz, center.xz - swept) - ext.xz - p.blade_reach;
    float2 hi = max(center.xz, center.xz - swept) + ext.xz + p.blade_reach;
    if (hi.x < -p.half_extent || lo.x > p.half_extent || hi.y < -p.half_extent || lo.y > p.half_extent) {
        return;
    }

    int n = int(p.cells_per_side);
    int x0 = clamp(int(floor((lo.x + p.half_extent) / p.cell_size)), 0, n - 1);
    int x1 = clamp(int(floor((hi.x + p.half_extent) / p.cell_size)), 0, n - 1);
    int z0 = clamp(int(floor((lo.y + p.half_extent) / p.cell_size)), 0, n - 1);
    int z1 = clamp(int(floor((hi.y + p.half_extent) / p.cell_size)), 0, n - 1);

    uint cell_count = p.cells_per_side * p.cells_per_side;
    for (int z = z0; z <= z1; ++z) {
        for (int x = x0; x <= x1; ++x) {
            uint cell = uint(z) * p.cells_per_side + uint(x);
            uint slot;
            InterlockedAdd(grid_data[cell], 1u, slot);
            if (slot < p.cell_capacity) {
                grid_data[cell_count + cell * p.cell_capacity + slot] = bi;
            }
        }
    }
}
