struct grass_blade_rest {
    float3 points[grass_nodes];
    float3 width_axis;
};

float grass_hash01(uint blade, uint salt) {
    uint x = blade * 0x9e3779b9u + salt * 0x85ebca6bu;
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return float(x) * (1.0 / 4294967296.0);
}

float3 grass_rotate(float4 q, float3 v) {
    float3 u = q.yzw;
    float s = q.x;
    return 2.0 * dot(u, v) * u + (s * s - dot(u, u)) * v + 2.0 * s * cross(u, v);
}

bool grass_tile_blade(grass_params p, uint tile, uint local, out uint blade) {
    uint bx = (tile / p.tiles_per_side) * grass_tile_blades + local / grass_tile_blades;
    uint bz = (tile % p.tiles_per_side) * grass_tile_blades + local % grass_tile_blades;
    blade = bx * p.blades_per_side + bz;
    return bx < p.blades_per_side && bz < p.blades_per_side;
}

float2 grass_tile_min(grass_params p, uint tile) {
    float size = p.spacing * float(grass_tile_blades);
    return float2(float(tile / p.tiles_per_side), float(tile % p.tiles_per_side)) * size - p.half_extent;
}

grass_blade_rest grass_rest_of(grass_params p, uint blade) {
    uint bx = blade / p.blades_per_side;
    uint bz = blade % p.blades_per_side;
    float jitter = p.spacing * grass_jitter_fraction;

    float yaw = 6.28318530718 * grass_hash01(blade, 3u);
    float3 lean_dir = float3(cos(yaw), 0.0, sin(yaw));
    float lean = grass_lean_base + grass_lean_spread * grass_hash01(blade, 4u);
    float curl = grass_curl_base + grass_curl_spread * grass_hash01(blade, 5u);
    float height = p.blade_height * (grass_height_base + grass_height_spread * grass_hash01(blade, 6u));
    float segment = height / float(grass_segments);

    grass_blade_rest rest;
    rest.width_axis = float3(-lean_dir.z, 0.0, lean_dir.x);
    rest.points[0] = float3(
        p.spacing * (float(bx) + 0.5) - p.half_extent + jitter * (grass_hash01(blade, 1u) * 2.0 - 1.0),
        0.0,
        p.spacing * (float(bz) + 0.5) - p.half_extent + jitter * (grass_hash01(blade, 2u) * 2.0 - 1.0)
    );
    float tilt = lean;
    for (uint n = 1; n < grass_nodes; ++n) {
        float rise = cos(tilt);
        float run = sin(tilt);
        rest.points[n] = rest.points[n - 1] + float3(segment * run * lean_dir.x, segment * rise, segment * run * lean_dir.z);
        tilt += curl;
    }
    return rest;
}

float grass_half_width(grass_params p, uint node) {
    return 0.5 * lerp(p.root_width, p.tip_width, float(node) / float(grass_segments));
}
