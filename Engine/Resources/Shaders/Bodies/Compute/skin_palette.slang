void skin_palette(uint3 dispatch_id) {
    uint bone = dispatch_id.x;
    if (bone >= pc.bone_count) {
        return;
    }

    rig_bone_binding binding = bone_data[bone];

    skin_matrix result = (skin_matrix)0;
    result.valid = binding.valid;
    result.skin = binding.valid != 0
        ? mul(binding.world, binding.inverse_bind)
        : float4x4(
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0
        );

    palette_out[bone] = result;
}
