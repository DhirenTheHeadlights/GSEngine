void grass_tlas_instances(uint3 dispatch_id) {
    grass_params p = grass_params_in[0];
    uint tile = dispatch_id.x;
    if (tile >= p.tile_count) {
        return;
    }

    uint2 address = rt_static_blas[tile];
    uint slot_plus_one = grass_tiles[tile].slot_plus_one;
    if (slot_plus_one != 0u) {
        uint2 dynamic_address = rt_dynamic_blas[slot_plus_one - 1u];
        if (any(dynamic_address != uint2(0u, 0u))) {
            address = dynamic_address;
        }
    }

    float3 x_axis = grass_rotate(p.field_orientation, float3(1.0, 0.0, 0.0));
    float3 y_axis = grass_rotate(p.field_orientation, float3(0.0, 1.0, 0.0));
    float3 z_axis = grass_rotate(p.field_orientation, float3(0.0, 0.0, 1.0));
    float3 origin = p.field_position;

    uint dst = (pc.first_instance + tile) * grass_tlas_instance_bytes;
    tlas_instances.Store4(dst, asuint(float4(x_axis.x, y_axis.x, z_axis.x, origin.x)));
    tlas_instances.Store4(dst + 16u, asuint(float4(x_axis.y, y_axis.y, z_axis.y, origin.y)));
    tlas_instances.Store4(dst + 32u, asuint(float4(x_axis.z, y_axis.z, z_axis.z, origin.z)));
    tlas_instances.Store2(dst + 48u, uint2(pc.material_index | (grass_tlas_mask << 24u), grass_tlas_cull_disable << 24u));
    tlas_instances.Store2(dst + 56u, address);
}
