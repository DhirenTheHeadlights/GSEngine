void collision_color_commit(uint3 dispatch_id) {
    uint bi = dispatch_id.x;
    if (bi >= pc.body_count) return;

    coloring_scratch[bi] = coloring_scratch[max_bodies + bi];

    if (bi == 0) {
        uint pending;
        InterlockedAdd(collision_state[state_color_pending_index], 0, pending);
        uint groups = (pc.body_count + workgroup_size - 1) / workgroup_size;
        indirect_args[2 + max_colors].x = pending != 0 ? groups : 0;
        indirect_args[2 + max_colors].y = 1;
        indirect_args[2 + max_colors].z = 1;
        uint previous;
        InterlockedExchange(collision_state[state_color_pending_index], 0, previous);
    }
}
