void collision_scatter_adjacency(uint3 dispatch_id) {
    uint idx = dispatch_id.x;
    uint body_count = pc.body_count;
    uint num_joints = min(pc.joint_count, max_joints);
    uint num_muscles = min(solver_config_data[0].muscle_count, max_muscles);
    uint num_contacts = min(collision_state[state_contact_count_index], max_contacts);
    uint thread_count = ((max(max(body_count, num_joints), num_muscles) + workgroup_size - 1) / workgroup_size) * workgroup_size;

    for (uint ci = idx; ci < num_contacts; ci += thread_count) {
        uint body_a = contact_data[ci].body_a;
        uint body_b = contact_data[ci].body_b;
        if (body_a < body_count && body_b < body_count) {
            uint slot_a;
            InterlockedAdd(contact_counts[body_a], 1u, slot_a);
            uint off_a = contact_offsets[body_a] + slot_a;
            if (off_a < max_contact_adjacency) {
                contact_adjacency[off_a] = ci;
            }

            uint slot_b;
            InterlockedAdd(contact_counts[body_b], 1u, slot_b);
            uint off_b = contact_offsets[body_b] + slot_b;
            if (off_b < max_contact_adjacency) {
                contact_adjacency[off_b] = ci;
            }
        }
    }

    if (idx < num_joints) {
        uint body_a = joint_data[idx].body_a;
        uint body_b = joint_data[idx].body_b;
        if (body_a < body_count && body_b < body_count) {
            uint slot_a;
            InterlockedAdd(joint_counts[body_a], 1u, slot_a);
            uint off_a = joint_offsets[body_a] + slot_a;
            if (off_a < max_joint_adjacency) {
                joint_adjacency[off_a] = idx;
            }

            uint slot_b;
            InterlockedAdd(joint_counts[body_b], 1u, slot_b);
            uint off_b = joint_offsets[body_b] + slot_b;
            if (off_b < max_joint_adjacency) {
                joint_adjacency[off_b] = idx;
            }
        }
    }

    if (idx < num_muscles) {
        muscle_constraint m = muscle_data[idx];
        for (uint k = 0; k < m.path_count; k++) {
            uint body = muscle_path_data[m.path_first + k].body;
            bool repeat = false;
            for (uint e = 0; e < k; e++) {
                repeat = repeat || muscle_path_data[m.path_first + e].body == body;
            }
            if (!repeat && body < body_count) {
                uint slot;
                InterlockedAdd(joint_counts[body], 1u, slot);
                uint off = joint_offsets[body] + slot;
                if (off < max_joint_adjacency) {
                    joint_adjacency[off] = muscle_adjacency_tag | idx;
                }
            }
        }
    }
}
