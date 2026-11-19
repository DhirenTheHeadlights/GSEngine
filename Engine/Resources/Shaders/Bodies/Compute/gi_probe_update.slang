void gi_probe_update(uint3 dispatch_id, uint3 group_id, uint3 group_thread_id) {
    probe_irradiance_texel texel = probe_irradiance_project(group_id, group_thread_id);
    float3 previous = irradiance_atlas_out[texel.atlas_xy].rgb;
    float blend = pc.history_valid != 0u ? temporal_blend : 1.0;
    irradiance_atlas_out[texel.atlas_xy] = float4(lerp(previous, texel.irradiance, blend), 1.0);
}
