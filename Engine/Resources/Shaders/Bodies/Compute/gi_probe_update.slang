groupshared float3 s_dirs[64];
groupshared float3 s_radiance[64];

float2 oct_wrap(float2 v) {
    float2 abs_v = abs(v);
    return (1.0 - abs_v.yx) * float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

float3 oct_decode(float2 e) {
    float3 v = float3(e, 1.0 - abs(e.x) - abs(e.y));
    if (v.z < 0.0) {
        v.xy = oct_wrap(v.xy);
    }
    return normalize(v);
}

float3 fibonacci_sphere(uint i, uint count, uint seed) {
    const float gold = 2.39996323;
    float seed_jitter = float(seed % 64u) * (1.0 / 64.0);
    float i_f = float(i) + 0.5 + seed_jitter;
    float phi = i_f * gold;
    float z = 1.0 - 2.0 * (i_f / float(count));
    float r = sqrt(saturate(1.0 - z * z));
    return float3(r * cos(phi), z, r * sin(phi));
}

void gi_probe_update(uint3 dispatch_id, uint3 group_id, uint3 group_thread_id) {
    uint ray_idx = group_thread_id.x;
    uint3 probe_coord = group_id;
    float3 probe_pos = pc.origin_world + float3(probe_coord) * pc.spacing;

    float3 dir = fibonacci_sphere(ray_idx, 64u, pc.frame_counter);

    RayQuery<RAY_FLAG_CULL_BACK_FACING_TRIANGLES> q;
    RayDesc r;
    r.Origin = probe_pos;
    r.TMin = 0.01;
    r.Direction = dir;
    r.TMax = pc.trace_t_max;
    q.TraceRayInline(scene_tlas, RAY_FLAG_NONE, 0xFF, r);
    while (q.Proceed()) {}

    float3 radiance;
    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
        uint mat_idx = q.CommittedInstanceID();
        material_data m = material_palette[mat_idx];
        float3 N = -dir;
        float ndl = max(0.0, dot(N, -pc.sun_direction));
        float sun_scale = pc.sun_intensity * 0.001;
        radiance = m.base_color * pc.sun_color * sun_scale * ndl;
        radiance += m.base_color * pc.sky_color * 0.1;
    } else {
        radiance = pc.sky_color;
    }
    s_dirs[ray_idx] = dir;
    s_radiance[ray_idx] = radiance;
    GroupMemoryBarrierWithGroupSync();

    uint tx = ray_idx & 7u;
    uint ty = ray_idx >> 3u;
    float2 oct_uv = (float2(tx, ty) + 0.5) / 8.0 * 2.0 - 1.0;
    float3 texel_dir = oct_decode(oct_uv);

    float3 accum = float3(0.0);
    float wsum = 0.0;
    for (uint i = 0u; i < 64u; ++i) {
        float w = max(0.0, dot(texel_dir, s_dirs[i]));
        accum += s_radiance[i] * w;
        wsum += w;
    }
    accum /= max(wsum, 1e-4);

    uint2 atlas_xy = uint2(
        probe_coord.x * 8u + tx,
        (probe_coord.y * pc.grid_dim_uniform.z + probe_coord.z) * 8u + ty
    );
    irradiance_atlas_out[atlas_xy] = float4(accum * pc.intensity, 1.0);
}
