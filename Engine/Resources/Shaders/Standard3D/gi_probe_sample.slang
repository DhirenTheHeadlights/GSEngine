public struct GiGridParams {
    public float3 origin;
    public float spacing;
    public uint3 grid_dim;
    public float intensity;
    public float2 atlas_size;
};

float2 gi_oct_wrap(float2 v) {
    float2 abs_v = abs(v);
    return (1.0 - abs_v.yx) * float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

public float2 gi_oct_encode(float3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : gi_oct_wrap(n.xy);
    return n.xy;
}

public float3 sample_gi_irradiance(float3 world_pos, float3 world_normal, GiGridParams params, Texture2D<float4> atlas, SamplerState atlas_sampler, out float coverage) {
    float3 grid_uv = (world_pos - params.origin) / params.spacing;
    float3 max_index = float3(params.grid_dim) - 1.001;
    float3 edge_cells = min(grid_uv, max_index - grid_uv);
    coverage = saturate(min(min(edge_cells.x, edge_cells.y), edge_cells.z));
    grid_uv = clamp(grid_uv, float3(0.0, 0.0, 0.0), max_index);
    uint3 base = uint3(floor(grid_uv));
    float3 frac_uv = saturate(grid_uv - float3(base));

    float2 oct = gi_oct_encode(world_normal);
    float2 tile_uv = oct * 0.5 + 0.5;

    float3 sum = float3(0.0);
    float wsum = 0.0;
    for (uint i = 0u; i < 8u; ++i) {
        uint3 off = uint3(i & 1u, (i >> 1u) & 1u, (i >> 2u) & 1u);
        uint3 idx = base + off;
        float3 w3 = lerp(1.0 - frac_uv, frac_uv, float3(off));
        float w = w3.x * w3.y * w3.z;

        uint2 atlas_base = uint2(idx.x * 8u, (idx.y * params.grid_dim.z + idx.z) * 8u);
        float2 atlas_uv = (float2(atlas_base) + tile_uv * 7.0 + 0.5) / params.atlas_size;
        sum += atlas.SampleLevel(atlas_sampler, atlas_uv, 0).rgb * w;
        wsum += w;
    }
    return (sum / max(wsum, 1e-4)) * params.intensity;
}
