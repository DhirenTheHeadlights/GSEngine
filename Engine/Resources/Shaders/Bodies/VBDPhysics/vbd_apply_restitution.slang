
static const uint restitution_capacity = 2048;

groupshared uint64_t restitution_keys[restitution_capacity];
groupshared uint restitution_contacts[restitution_capacity];
groupshared uint restitution_count;

void apply_one_restitution(uint ci) {
    contact_constraint c = contact_data[ci];

    body_state ba = body_data[c.body_a];
    body_state bb = body_data[c.body_b];

    float3 rAW = rotate_vector(ba.orientation, c.local_anchor_a);
    float3 rBW = rotate_vector(bb.orientation, c.local_anchor_b);
    float3 va = ba.velocity + cross(ba.angular_velocity, rAW);
    float3 vb = bb.velocity + cross(bb.angular_velocity, rBW);
    float v_rel_after = dot(c.normal, va - vb);

    float speed_ratio = clamp(
        (-c.approach_speed - solver_config_data[0].restitution_threshold) / solver_config_data[0].restitution_threshold,
        0.0,
        1.0
    );
    float v_target = -c.restitution * speed_ratio * c.approach_speed;
    float dv = v_target - v_rel_after;
    if (dv <= 0.0) return;

    float w_a = (ba.locked != 0 || ba.mass <= 1e-10) ? 0.0 : 1.0 / ba.mass;
    float w_b = (bb.locked != 0 || bb.mass <= 1e-10) ? 0.0 : 1.0 / bb.mass;
    float w_total = w_a + w_b;
    if (w_total <= 0.0) return;

    float3 impulse = c.normal * (dv / w_total);

    if (ba.locked == 0) {
        body_data[c.body_a].velocity = ba.velocity + impulse * w_a;
    }
    if (bb.locked == 0) {
        body_data[c.body_b].velocity = bb.velocity - impulse * w_b;
    }
}

bool restitution_gate(uint ci) {
    if (contact_data[ci].body_a >= pc.body_count || contact_data[ci].body_b >= pc.body_count) return false;
    if (contact_data[ci].restitution <= 0.0) return false;
    if (contact_data[ci].lambda.x >= 0.0) return false;
    if (contact_data[ci].approach_speed >= -solver_config_data[0].restitution_threshold) return false;
    return true;
}

void vbd_apply_restitution(uint3 dispatch_id, uint3 group_thread_id) {
    uint tid = group_thread_id.x;
    uint num_contacts = min(collision_state[state_contact_count_index], max_contacts);

    if (tid == 0) {
        restitution_count = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    for (uint ci = tid; ci < num_contacts; ci += adjacency_workgroup_size) {
        if (!restitution_gate(ci)) {
            continue;
        }
        uint slot;
        InterlockedAdd(restitution_count, 1u, slot);
        if (slot < restitution_capacity) {
            uint pair_key = (contact_data[ci].body_a << 16) | contact_data[ci].body_b;
            uint64_t fk = contact_data[ci].feature_key;
            uint64_t feature_rank = (uint64_t)(uint(fk ^ (fk >> 32)) & 0xFFFFFFu);
            restitution_keys[slot] =
                ((uint64_t)pair_key << 32) |
                ((uint64_t)(contact_data[ci].replayed & 0xFFu) << 24) |
                feature_rank;
            restitution_contacts[slot] = ci;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    uint q = restitution_count;
    if (tid == 0) {
        InterlockedMax(collision_state[state_tick_hash_base_index], q);
    }
    if (q == 0) {
        return;
    }

    if (q > restitution_capacity) {
        if (tid == 0) {
            for (uint ci = 0; ci < num_contacts; ci++) {
                if (restitution_gate(ci)) {
                    apply_one_restitution(ci);
                }
            }
        }
        return;
    }

    uint m = 1;
    while (m < q) {
        m <<= 1;
    }

    for (uint pad = tid; pad < m; pad += adjacency_workgroup_size) {
        if (pad >= q) {
            restitution_keys[pad] = ~(uint64_t)0;
            restitution_contacts[pad] = 0xFFFFFFFFu;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    for (uint k = 2; k <= m; k <<= 1) {
        for (uint j = k >> 1; j > 0; j >>= 1) {
            for (uint i = tid; i < m; i += adjacency_workgroup_size) {
                uint partner = i ^ j;
                if (partner <= i) {
                    continue;
                }
                bool greater = restitution_keys[i] > restitution_keys[partner];
                bool ascending = (i & k) == 0;
                if (greater == ascending) {
                    uint64_t tk = restitution_keys[i];
                    restitution_keys[i] = restitution_keys[partner];
                    restitution_keys[partner] = tk;
                    uint tc = restitution_contacts[i];
                    restitution_contacts[i] = restitution_contacts[partner];
                    restitution_contacts[partner] = tc;
                }
            }
            GroupMemoryBarrierWithGroupSync();
        }
    }

    if (tid != 0) {
        return;
    }

    for (uint e = 0; e < q; e++) {
        apply_one_restitution(restitution_contacts[e]);
    }
}
