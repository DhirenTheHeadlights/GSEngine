void cloud_shape_bound_bake(uint3 dispatch_id) {
    uint3 blocks;
    cloud_shape_bound_out.GetDimensions(blocks.x, blocks.y, blocks.z);
    if (any(dispatch_id >= blocks)) {
        return;
    }

    cloud_shape_bound_out[dispatch_id] = cloud_shape_in.Load(int4(0, 0, 0, 0));
}
