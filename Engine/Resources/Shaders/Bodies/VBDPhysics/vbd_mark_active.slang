static const uint active_seed_previous = 0;
static const uint active_seed_statics = 1;
static const uint active_seed_motors = 2;
static const uint active_seed_impulses = 3;
static const uint active_seed_inputs = 4;
static const uint active_seed_all = 5;
static const uint active_linger_substeps = 2;

uint active_linger_index(uint bi) {
    return active_state_words + (max_bodies() + 31u) / 32u + bi;
}

void mark_active_body(uint bi) {
    InterlockedOr(active_data[active_state_words + (bi >> 5)], 1u << (bi & 31u));
}

void mark_active_neighbours(uint bi) {
    uint co = contact_offsets[bi];
    uint cc = contact_counts[bi];
    for (uint k = 0; k < cc; k++) {
        uint ci = safe_load_uint(contact_adjacency, co + k, max_contact_adjacency());
        if (ci >= max_contacts()) continue;
        contact_constraint c = contact_data[ci];
        uint other = (c.body_a == bi) ? c.body_b : c.body_a;
        if (other < pc.body_count) mark_active_body(other);
    }

    uint j_off = joint_offsets[bi];
    uint jc = joint_counts[bi];
    for (uint k = 0; k < jc; k++) {
        uint ji = safe_load_uint(joint_adjacency, j_off + k, max_joint_adjacency());
        if ((ji & muscle_adjacency_tag) != 0) {
            muscle_constraint m = muscle_data[ji & ~muscle_adjacency_tag];
            for (uint p = 0; p < m.path_count + m.wrap_count; p++) {
                uint other = muscle_path_data[m.path_first + p].body;
                if (other < pc.body_count) mark_active_body(other);
            }
            continue;
        }
        if (ji >= max_joints()) continue;
        joint_constraint jnt = joint_data[ji];
        uint other = (jnt.body_a == bi) ? jnt.body_b : jnt.body_a;
        if (other < pc.body_count) mark_active_body(other);
    }
}

uint active_seed_body(uint mode, uint slot) {
    if (mode == active_seed_previous) {
        return slot < active_data[active_count_word] ? active_list[active_data[active_half_word] * max_bodies() + slot] : 0xFFFFFFFF;
    }
    if (mode == active_seed_statics) {
        return slot < static_bodies_data[0] ? static_bodies_data[1 + slot] : 0xFFFFFFFF;
    }
    if (mode == active_seed_motors) {
        uint motor_base = (pc.substep / max(pc.substeps_per_tick, 1u)) * pc.motor_count;
        return slot < pc.motor_count ? motor_data[motor_base + slot].body_index : 0xFFFFFFFF;
    }
    if (mode == active_seed_impulses) {
        return slot < pc.impulse_count ? impulse_data[pc.impulse_offset + slot].body_index : 0xFFFFFFFF;
    }
    if (mode == active_seed_inputs) {
        return slot < body_input_index_data[0] ? body_input_index_data[1 + slot] : 0xFFFFFFFF;
    }
    return slot;
}

void vbd_mark_active(uint3 dispatch_id) {
    uint mode = pc.color_offset;
    uint bi = active_seed_body(mode, dispatch_id.x);
    if (bi >= pc.body_count) return;

    body_state body = body_data[bi];
    bool locked = body.locked != 0;
    bool awake = !locked && body.sleep_counter < sleep_threshold;

    if (mode == active_seed_previous && !locked && !awake) {
        uint linger = active_data[active_linger_index(bi)];
        if (linger == 0) return;
        active_data[active_linger_index(bi)] = linger - 1;
    }
    if (awake) {
        active_data[active_linger_index(bi)] = active_linger_substeps;
        mark_active_neighbours(bi);
    }
    mark_active_body(bi);
}
