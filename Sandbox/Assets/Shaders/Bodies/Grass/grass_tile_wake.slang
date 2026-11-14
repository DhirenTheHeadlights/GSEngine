void grass_tile_wake(uint3 dispatch_id) {
    grass_params p = params_data[0];
    uint tile = dispatch_id.x;
    if (tile >= p.tile_count) {
        return;
    }

    float size = p.spacing * float(grass_tile_blades);
    float2 lo = float2(float(tile / p.tiles_per_side), float(tile % p.tiles_per_side)) * size - p.spacing;
    float2 hi = lo + size + 2.0 * p.spacing;

    int n = int(p.cells_per_side);
    int x0 = clamp(int(floor(lo.x / p.cell_size)), 0, n - 1);
    int x1 = clamp(int(floor(hi.x / p.cell_size)), 0, n - 1);
    int z0 = clamp(int(floor(lo.y / p.cell_size)), 0, n - 1);
    int z1 = clamp(int(floor(hi.y / p.cell_size)), 0, n - 1);

    uint wake = 0u;
    for (int z = z0; z <= z1; ++z) {
        for (int x = x0; x <= x1; ++x) {
            if (grid_read_data[uint(z) * p.cells_per_side + uint(x)] != 0u) {
                wake = 1u;
            }
        }
    }
    tile_data[tile].wake = wake;
}
