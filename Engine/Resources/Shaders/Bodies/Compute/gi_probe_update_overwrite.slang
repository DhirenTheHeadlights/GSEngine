void gi_probe_update_overwrite(uint3 dispatch_id, uint3 group_id, uint3 group_thread_id) {
    probe_irradiance_texel texel = probe_irradiance_project(group_id, group_thread_id);
    irradiance_atlas_out[texel.atlas_xy] = float4(texel.irradiance, 1.0);
}
