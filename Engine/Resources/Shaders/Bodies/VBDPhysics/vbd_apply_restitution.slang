
static const uint restitution_capacity = 2048;

groupshared uint64_t restitution_keys_hi[restitution_capacity];
groupshared uint restitution_keys_lo[restitution_capacity];
groupshared uint restitution_count;
groupshared uint restitution_serial;

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

static const uint restitution_ci_bits = firstbithigh(max_contacts - 1) + 1;
static const uint restitution_ci_mask = (1u << restitution_ci_bits) - 1;
static const uint restitution_local_island_bits = 28 - restitution_ci_bits;
static const uint restitution_local_island_mask = (1u << restitution_local_island_bits) - 1;
static const uint restitution_island_limit = 1u << 20;
static const uint restitution_body_bits = 18;

uint restitution_island(uint ci) {
    uint body_a = contact_data[ci].body_a;
    uint body_b = contact_data[ci].body_b;
    uint island_a = body_env_data[body_a];
    uint island_b = body_env_data[body_b];
    bool writable_a = body_data[body_a].locked == 0;
    bool writable_b = body_data[body_b].locked == 0;
    bool stray_a = writable_a && island_a == 0xFFFFFFFFu;
    bool stray_b = writable_b && island_b == 0xFFFFFFFFu;
    bool split = island_a != 0xFFFFFFFFu && island_b != 0xFFFFFFFFu && island_a != island_b;
    uint island = island_a != 0xFFFFFFFFu ? island_a : island_b;
    if (stray_a || stray_b || split || island >= restitution_island_limit) {
        restitution_serial = 1u;
        return 0u;
    }
    return island;
}

void restitution_gather(uint ci, uint local_island) {
    uint slot;
    InterlockedAdd(restitution_count, 1u, slot);
    if (slot < restitution_capacity) {
        uint64_t fk = contact_data[ci].feature_key;
        uint feature_rank = uint(fk ^ (fk >> 32)) & 0xFFFFFFu;
        restitution_keys_hi[slot] =
            ((uint64_t)contact_data[ci].body_a << (64 - restitution_body_bits)) |
            ((uint64_t)contact_data[ci].body_b << (64 - 2 * restitution_body_bits)) |
            ((uint64_t)(contact_data[ci].replayed & 0xFFu) << 20) |
            (uint64_t)(feature_rank >> 4);
        restitution_keys_lo[slot] =
            ((feature_rank & 0xFu) << 28) |
            (local_island << restitution_ci_bits) |
            ci;
    }
}

bool restitution_greater(uint i, uint j) {
    uint64_t hi_i = restitution_keys_hi[i];
    uint64_t hi_j = restitution_keys_hi[j];
    return hi_i > hi_j || (hi_i == hi_j && restitution_keys_lo[i] > restitution_keys_lo[j]);
}

void restitution_sort(uint tid, uint q) {
    uint m = 1;
    while (m < q) {
        m <<= 1;
    }

    for (uint pad = tid; pad < m; pad += adjacency_workgroup_size) {
        if (pad >= q) {
            restitution_keys_hi[pad] = ~(uint64_t)0;
            restitution_keys_lo[pad] = 0xFFFFFFFFu;
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
                bool greater = restitution_greater(i, partner);
                bool ascending = (i & k) == 0;
                if (greater == ascending) {
                    uint64_t th = restitution_keys_hi[i];
                    restitution_keys_hi[i] = restitution_keys_hi[partner];
                    restitution_keys_hi[partner] = th;
                    uint tl = restitution_keys_lo[i];
                    restitution_keys_lo[i] = restitution_keys_lo[partner];
                    restitution_keys_lo[partner] = tl;
                }
            }
            GroupMemoryBarrierWithGroupSync();
        }
    }
}

void vbd_apply_restitution(uint3 dispatch_id, uint3 group_thread_id) {
    uint tid = group_thread_id.x;
    uint group = dispatch_id.x / adjacency_workgroup_size;
    uint island_begin = group * adjacency_workgroup_size;
    uint island_end = island_begin + adjacency_workgroup_size;
    uint num_contacts = min(collision_state[state_contact_count_index], max_contacts);

    if (tid == 0) {
        restitution_count = 0;
        restitution_serial = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    for (uint ci = tid; ci < num_contacts; ci += adjacency_workgroup_size) {
        if (!restitution_gate(ci)) {
            continue;
        }
        uint island = restitution_island(ci);
        if (island >= island_begin && island < island_end) {
            restitution_gather(ci, island - island_begin);
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (restitution_serial != 0u) {
        if (group != 0) {
            return;
        }
        if (tid == 0) {
            restitution_count = 0;
        }
        GroupMemoryBarrierWithGroupSync();
        for (uint ci = tid; ci < num_contacts; ci += adjacency_workgroup_size) {
            if (restitution_gate(ci)) {
                restitution_gather(ci, 0u);
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
        restitution_sort(tid, q);
        if (tid == 0) {
            for (uint e = 0; e < q; e++) {
                apply_one_restitution(restitution_keys_lo[e] & restitution_ci_mask);
            }
        }
        return;
    }

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
                if (!restitution_gate(ci)) {
                    continue;
                }
                uint island = restitution_island(ci);
                if (island >= island_begin && island < island_end) {
                    apply_one_restitution(ci);
                }
            }
        }
        return;
    }

    restitution_sort(tid, q);

    for (uint e = 0; e < q; e++) {
        uint lo = restitution_keys_lo[e];
        if (((lo >> restitution_ci_bits) & restitution_local_island_mask) == tid) {
            apply_one_restitution(lo & restitution_ci_mask);
        }
    }
}
