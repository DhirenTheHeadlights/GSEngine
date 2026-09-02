struct VS_OUT {
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

[shader("vertex")]
VS_OUT vs_main(uint vid : SV_VertexID) {
    float2 p = float2((vid << 1) & 2, vid & 2);
    VS_OUT o;
    o.uv = p;
    o.clip_pos = float4(p * 2.0 - 1.0, 0.0, 1.0);
    return o;
}

float3 heat_ramp(float t) {
    const float3 cold = float3(0.05, 0.10, 0.45);
    const float3 cool = float3(0.05, 0.65, 0.75);
    const float3 mid = float3(0.20, 0.80, 0.25);
    const float3 warm = float3(0.95, 0.80, 0.15);
    const float3 hot = float3(0.95, 0.15, 0.10);

    if (t < 0.25) {
        return lerp(cold, cool, t / 0.25);
    }
    if (t < 0.5) {
        return lerp(cool, mid, (t - 0.25) / 0.25);
    }
    if (t < 0.75) {
        return lerp(mid, warm, (t - 0.5) / 0.25);
    }
    return lerp(warm, hot, (t - 0.75) / 0.25);
}

[shader("fragment")]
float4 fs_main(VS_OUT input) : SV_Target {
    uint2 pixel = uint2(input.clip_pos.xy);
    uint2 tile = pixel / pc.tile_size;

    if (tile.x >= pc.tile_counts.x || tile.y >= pc.tile_counts.y) {
        discard;
    }

    uint tile_index = tile.y * pc.tile_counts.x + tile.x;
    uint count = tile_light_table[tile_index].y;

    float t = saturate(float(count) / float(pc.max_lights_per_tile));
    float3 color = heat_ramp(t);

    uint2 local = pixel % pc.tile_size;
    bool on_edge = local.x == 0 || local.y == 0;
    color *= on_edge ? 0.45 : 1.0;

    float alpha = pc.opacity * (count == 0 ? 0.35 : 1.0);
    return float4(color, alpha);
}
