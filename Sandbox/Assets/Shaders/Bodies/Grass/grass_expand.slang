static const uint grass_nodes = 5;
static const uint grass_side_vertices = 9;

void grass_expand(uint3 dispatch_id) {
    uint vi = dispatch_id.x;
    if (vi >= pc.vertex_count) {
        return;
    }

    uint blade = vi / (grass_side_vertices * 2);
    uint local = vi % (grass_side_vertices * 2);
    uint side = local / grass_side_vertices;
    uint k = local % grass_side_vertices;

    uint node = k < 8 ? k / 2 : 4;
    float lateral = k < 8 ? ((k % 2) == 0 ? -1.0 : 1.0) : 0.0;

    uint base = blade * grass_nodes;
    uint lo = node == 0 ? 0 : node - 1;
    uint hi = node == grass_nodes - 1 ? node : node + 1;
    float3 p = node_data[base + node].position;
    float3 span = node_data[base + hi].position - node_data[base + lo].position;
    float span_len = length(span);
    float3 t = span_len > 1e-7 ? span / span_len : float3(0.0, 1.0, 0.0);

    grass_rest_node rest = rest_data[base + node];
    float3 a = rest.width_axis - dot(rest.width_axis, t) * t;
    float a_len = length(a);
    a = a_len > 1e-6 ? a / a_len : rest.width_axis;

    float3 n = normalize(cross(a, t));
    if (side == 1) {
        n = -n;
    }

    vertex result;
    result.position = p + a * (lateral * rest.half_width);
    result.normal = n;
    result.tex_coords = float2(lateral * 0.5 + 0.5, float(node) / float(grass_nodes - 1));
    deformed_vertices[vi] = result;
}
