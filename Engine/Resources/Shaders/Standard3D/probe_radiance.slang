interface IProbeRadiance {
    static uint3 probe_coord(uint3 group_id);
    static float3 radiance(uint3 group_id, float3 probe_pos, float3 dir, float3 sky_radiance);
};
