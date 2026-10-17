
void vbd_prepare_indirect() {
    uint pair_count = min(collision_pairs[0], max_collision_pairs);
    indirect_args[0].x = max((pair_count + workgroup_size - 1) / workgroup_size, 1);
    indirect_args[0].y = 1;
    indirect_args[0].z = 1;

    uint poly_pair_count = max(min(collision_pairs[poly_pair_base], max_collision_pairs), 1);
    indirect_args[3 + max_colors].x = min(poly_pair_count, max_dispatch_row);
    indirect_args[3 + max_colors].y = (poly_pair_count + max_dispatch_row - 1) / max_dispatch_row;
    indirect_args[3 + max_colors].z = 1;
}
