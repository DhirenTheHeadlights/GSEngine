void vbd_prepare_active_indirect(uint3 dispatch_id) {
    if (dispatch_id.x != 0) return;

    uint count = active_data[active_pending_word];
    active_data[active_count_word] = count;
    active_data[active_half_word] = 1u - active_data[active_half_word];
    active_data[active_pending_word] = 0;
    active_args[0].x = max((count + workgroup_size - 1) / workgroup_size, 1u);
    active_args[0].y = 1;
    active_args[0].z = 1;
}
