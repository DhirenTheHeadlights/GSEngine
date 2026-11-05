struct raster_probe_radiance : IProbeRadiance {
    static uint3 probe_coord(uint3 group_id) {
        return probe_coord_of(pc.first_probe + group_id.x, pc.grid_dim_uniform);
    }

    static float3 radiance(uint3 group_id, float3 probe_pos, float3 dir, float3 sky_radiance) {
        float4 captured = capture_in.Load(int3(int2(capture_texel(group_id.x, dir)), 0));
        return captured.a > 0.0 ? captured.rgb : sky_radiance;
    }
};

typealias probe_radiance_source = raster_probe_radiance;
