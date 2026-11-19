struct grass_surface_vertex {
    float3 position;
    float3 previous;
    float3 normal;
    float2 uv;
};

float3 grass_node_position(grass_params p, uint base, uint node, bool awake, grass_blade_rest rest) {
    return awake ? grass_pool[base + node].position : rest.points[node];
}

float3 grass_node_previous(grass_params p, uint base, uint node, bool awake, grass_blade_rest rest) {
    return awake && p.substeps != 0 ? grass_pool[base + node].previous : grass_node_position(p, base, node, awake, rest);
}

grass_surface_vertex grass_tile_vertex(grass_params p, uint tile, uint slot_plus_one, uint vertex_index) {
    grass_surface_vertex result;
    result.position = float3(0.0);
    result.previous = float3(0.0);
    result.normal = float3(0.0, 1.0, 0.0);
    result.uv = float2(0.0);

    uint local = vertex_index / grass_side_vertices;
    uint k = vertex_index % grass_side_vertices;
    uint blade;
    if (!grass_tile_blade(p, tile, local, blade)) {
        return result;
    }

    grass_blade_rest rest = grass_rest_of(p, blade);
    bool awake = slot_plus_one != 0u;
    uint base = awake ? ((slot_plus_one - 1u) * grass_tile_blade_count + local) * grass_nodes : 0u;

    bool tip = k == grass_side_vertices - 1u;
    uint node = tip ? grass_nodes - 1u : k / 2u;
    float lateral = tip ? 0.0 : ((k % 2u) == 0u ? -1.0 : 1.0);
    uint lo = node == 0u ? 0u : node - 1u;
    uint hi = node == grass_nodes - 1u ? node : node + 1u;

    float3 centre = grass_node_position(p, base, node, awake, rest);
    float3 span = grass_node_position(p, base, hi, awake, rest) - grass_node_position(p, base, lo, awake, rest);
    float span_len = length(span);
    float3 t = span_len > 1e-7 ? span / span_len : float3(0.0, 1.0, 0.0);

    float3 a = rest.width_axis - dot(rest.width_axis, t) * t;
    float a_len = length(a);
    a = a_len > 1e-6 ? a / a_len : rest.width_axis;

    float3 offset = a * (lateral * grass_half_width(p, node));
    result.position = centre + offset;
    result.previous = grass_node_previous(p, base, node, awake, rest) + offset;
    result.normal = normalize(cross(a, t));
    result.uv = float2(lateral * 0.5 + 0.5, float(node) / float(grass_nodes - 1u));
    return result;
}
