uint round_body_priority(uint bi) {
    uint state = bi * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

void collision_color_round(uint3 dispatch_id) {
    uint bi = dispatch_id.x;
    if (bi >= pc.body_count) return;

    uint current = coloring_scratch[bi] & 0xFFu;
    if (current != 0xFFu) {
        coloring_scratch[max_bodies + bi] = current;
        return;
    }

    if (body_data[bi].locked != 0) {
        coloring_scratch[max_bodies + bi] = 0xFFu;
        return;
    }

    uint cc = contact_counts[bi];
    uint jc = joint_counts[bi];
    if (cc == 0 && jc == 0) {
        coloring_scratch[max_bodies + bi] = 0xFFu;
        return;
    }

    uint num_contacts = min(collision_state[state_contact_count_index], max_contacts);
    uint num_joints = min(pc.joint_count, max_joints);
    uint my_priority = round_body_priority(bi);
    bool blocked = false;
    uint neighbor_colors = 0;

    uint off = contact_offsets[bi];
    for (uint k = 0; k < cc; k++) {
        uint idx = off + k;
        if (idx >= max_contact_adjacency) break;
        uint ci = contact_adjacency[idx];
        if (ci >= num_contacts) continue;
        contact_constraint c = contact_data[ci];
        uint other = (c.body_a == bi) ? c.body_b : c.body_a;
        if (other < pc.body_count) {
            if (body_data[other].locked != 0) continue;
            uint other_color = coloring_scratch[other] & 0xFFu;
            if (other_color == 0xFFu) {
                uint other_priority = round_body_priority(other);
                if (other_priority > my_priority ||
                    (other_priority == my_priority && other < bi)) {
                    blocked = true;
                }
            } else if (other_color < max_colors) {
                neighbor_colors |= (1u << other_color);
            }
        }
    }

    uint j_off = joint_offsets[bi];
    for (uint k = 0; k < jc; k++) {
        uint idx = j_off + k;
        if (idx >= max_joint_adjacency) break;
        uint ji = joint_adjacency[idx];
        if (ji >= num_joints) continue;
        joint_constraint j = joint_data[ji];
        uint other = (j.body_a == bi) ? j.body_b : j.body_a;
        if (other < pc.body_count) {
            if (body_data[other].locked != 0) continue;
            uint other_color = coloring_scratch[other] & 0xFFu;
            if (other_color == 0xFFu) {
                uint other_priority = round_body_priority(other);
                if (other_priority > my_priority ||
                    (other_priority == my_priority && other < bi)) {
                    blocked = true;
                }
            } else if (other_color < max_colors) {
                neighbor_colors |= (1u << other_color);
            }
        }
    }

    if (blocked) {
        InterlockedAdd(collision_state[state_color_pending_index], 1);
        coloring_scratch[max_bodies + bi] = 0xFFu;
        return;
    }

    uint palette = pc.color_count != 0 ? pc.color_count : max_colors;
    uint chosen = 0;
    while (chosen < palette && (neighbor_colors & (1u << chosen)) != 0) {
        chosen++;
    }

    uint next;
    if (chosen < palette) {
        next = chosen;
    }
    else if (palette < max_colors) {
        next = my_priority % palette;
    }
    else {
        next = 0xFFu;
    }
    coloring_scratch[max_bodies + bi] = next;
}
