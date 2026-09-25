void collision_count_adjacency(uint3 dispatch_id) {
    uint idx = dispatch_id.x;
    uint body_count = pc.body_count;
    uint num_joints = min(pc.joint_count, max_joints);
    uint num_contacts = min(collision_state[state_contact_count_index], max_contacts);
    uint thread_count = ((max(body_count, num_joints) + workgroup_size - 1) / workgroup_size) * workgroup_size;

    for (uint ci = idx; ci < num_contacts; ci += thread_count) {
        uint body_a = contact_data[ci].body_a;
        uint body_b = contact_data[ci].body_b;
        if (body_a < body_count && body_b < body_count) {
            float normal_y = contact_data[ci].normal.y;
            uint dummy;
            InterlockedAdd(contact_counts[body_a], 1u, dummy);
            InterlockedAdd(contact_counts[body_b], 1u, dummy);
            if (normal_y > 0.7) {
                InterlockedOr(grounded_bits[body_a / 32u], 1u << (body_a % 32u), dummy);
            }
            if (normal_y < -0.7) {
                InterlockedOr(grounded_bits[body_b / 32u], 1u << (body_b % 32u), dummy);
            }
        }
    }

    if (idx < num_joints) {
        uint body_a = joint_data[idx].body_a;
        uint body_b = joint_data[idx].body_b;
        if (body_a < body_count && body_b < body_count) {
            uint dummy;
            InterlockedAdd(joint_counts[body_a], 1u, dummy);
            InterlockedAdd(joint_counts[body_b], 1u, dummy);
        }
    }
}
