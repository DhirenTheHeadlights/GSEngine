void collision_clear_adjacency(uint3 dispatch_id) {
    uint idx = dispatch_id.x;
    uint body_count = pc.body_count;
    if (idx < body_count) {
        contact_counts[idx] = 0;
        joint_counts[idx] = 0;
    }
    if (idx < (body_count + 31u) / 32u) {
        grounded_bits[idx] = 0;
    }
    if (idx == 0) {
        unowned_contacts[0] = 0;
    }
}
