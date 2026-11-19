uint3 probe_coord_of(uint linear_index, uint3 grid_dim) {
    uint total = grid_dim.x * grid_dim.y * grid_dim.z;
    uint index = linear_index % total;
    return uint3(index % grid_dim.x, (index / grid_dim.x) % grid_dim.y, index / (grid_dim.x * grid_dim.y));
}

uint2 capture_texel(uint slot, float3 dir) {
    uint face = major_cube_face(dir);
    cube_face basis = cube_face_basis(face);
    float z = dot(dir, basis.forward);
    float2 ndc = float2(dot(dir, basis.right), -dot(dir, basis.up)) / z;
    float2 uv = saturate(ndc * 0.5 + 0.5);
    uint2 local = min(uint2(uv * float(capture_resolution)), uint2(capture_resolution - 1u, capture_resolution - 1u));
    return uint2(face * capture_resolution, slot * capture_resolution) + local;
}
