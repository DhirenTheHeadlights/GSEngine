uint probe_mix(uint h) {
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

uint probe_fold(uint h, float3 v) {
    h = probe_mix(h ^ asuint(v.x));
    h = probe_mix(h ^ asuint(v.y));
    h = probe_mix(h ^ asuint(v.z));
    return h;
}

void probe_fold_digest(uint h) {
    InterlockedAdd(counter_data[probe_digest_a], h);
    InterlockedAdd(counter_data[probe_digest_b], probe_mix(h ^ 0x27d4eb2fu));
}

uint probe_covering_body(grass_params p, uint cell, float3 root, float3 rest_tip, float height) {
    uint cell_count = p.cells_per_side * p.cells_per_side;
    uint listed = min(grid_read_data[cell], p.cell_capacity);
    for (uint k = 0; k < listed; ++k) {
        uint bi = grid_read_data[cell_count + cell * p.cell_capacity + k];
        body_state body = body_data[bi];
        float3 n;
        if (shape_distance(body, p, float3(root.x, height, root.z), 0.0, n) <= 0.0 && shape_distance(body, p, float3(rest_tip.x, height, rest_tip.z), 0.0, n) <= 0.0) {
            return bi;
        }
    }
    return probe_no_body;
}

float probe_tip_distance(grass_params p, uint cell, float3 tip) {
    uint cell_count = p.cells_per_side * p.cells_per_side;
    uint listed = min(grid_read_data[cell], p.cell_capacity);
    float nearest = 1e9;
    for (uint k = 0; k < listed; ++k) {
        float3 n;
        nearest = min(nearest, shape_distance(body_data[grid_read_data[cell_count + cell * p.cell_capacity + k]], p, tip, 0.0, n));
    }
    return nearest;
}

float probe_body_bottom(grass_params p, body_state body) {
    float3 center;
    float4 q;
    grass_body_frame(body, p, 0.0, center, q);
    float3 he = body.half_extents.xyz;
    float reach = abs(rotate_vector(q, float3(1.0, 0.0, 0.0)).y) * he.x + abs(rotate_vector(q, float3(0.0, 1.0, 0.0)).y) * he.y + abs(rotate_vector(q, float3(0.0, 0.0, 1.0)).y) * he.z;
    return center.y - reach;
}

uint probe_tile_cell_bodies(grass_params p, uint tile) {
    float size = p.spacing * float(grass_tile_blades);
    float2 lo = float2(float(tile / p.tiles_per_side), float(tile % p.tiles_per_side)) * size;
    int n = int(p.cells_per_side);
    int x0 = clamp(int(floor(lo.x / p.cell_size)), 0, n - 1);
    int x1 = clamp(int(floor((lo.x + size) / p.cell_size)), 0, n - 1);
    int z0 = clamp(int(floor(lo.y / p.cell_size)), 0, n - 1);
    int z1 = clamp(int(floor((lo.y + size) / p.cell_size)), 0, n - 1);
    uint most = 0u;
    for (int z = z0; z <= z1; ++z) {
        for (int x = x0; x <= x1; ++x) {
            most = max(most, grid_read_data[uint(z) * p.cells_per_side + uint(x)]);
        }
    }
    return most;
}

void probe_recovery_check(uint blade, float ratio) {
    uint last = last_under_data[blade];
    if (last == 0 || (last & probe_checked_bit) != 0) {
        return;
    }
    if (pc.step + 1 - last < pc.recovery_steps) {
        return;
    }
    InterlockedAdd(counter_data[probe_recovery_checks], 1u);
    InterlockedMax(counter_data[probe_recovery_deficit], asuint(max(1.0 - ratio, 0.0)));
    last_under_data[blade] = last | probe_checked_bit;
}

void probe_contact_recovery_check(uint blade, float ratio, bool asleep) {
    uint covered = last_under_data[blade] & probe_stamp_mask;
    if (covered == 0) {
        return;
    }
    uint contact = last_contact_data[blade];
    uint contact_step = contact & probe_stamp_mask;
    if ((contact & probe_checked_bit) != 0 && contact_step >= covered) {
        return;
    }
    uint since = max(covered, contact_step);
    if (pc.step + 1 - since < pc.recovery_steps) {
        if (asleep && (contact & probe_early_bit) == 0) {
            InterlockedAdd(counter_data[probe_early_sleeps], 1u);
            last_contact_data[blade] = contact | probe_early_bit;
        }
        return;
    }
    InterlockedAdd(counter_data[probe_contact_recovery_checks], 1u);
    InterlockedMax(counter_data[probe_contact_recovery_deficit], asuint(max(1.0 - ratio, 0.0)));
    if (contact_step > covered) {
        InterlockedMax(counter_data[probe_contact_after_cover], contact_step - covered);
    }
    last_contact_data[blade] = since | probe_checked_bit;
}

void grass_probe(uint3 group_id, uint3 group_thread_id) {
    grass_params p = params_data[0];
    uint tile = group_id.y;
    uint local = group_id.x * grass_sim_threads + group_thread_id.x;
    if (tile >= p.tile_count || local >= grass_tile_blade_count) {
        return;
    }

    grass_tile tile_state = tile_data[tile];
    bool awake = tile_state.slot_plus_one != 0u;

    if (local == 0u) {
        if (pc.digest != 0) {
            uint h = probe_mix(tile ^ 0x165667b1u);
            h = probe_mix(h ^ (awake ? 1u : 0u));
            h = probe_mix(h ^ tile_state.calm_steps);
            h = probe_mix(h ^ tile_state.sleep_ready);
            probe_fold_digest(h);
        }
        if (pc.track != 0) {
            InterlockedMax(counter_data[probe_max_cell_bodies], probe_tile_cell_bodies(p, tile));
        }
        if (tile == 0u) {
            counter_data[probe_awake_tiles] = status_data[0].awake_tiles;
            counter_data[probe_slot_overflow] = status_data[0].overflow_total;
            bool watched = pc.reaction_body < p.reaction_bodies && reaction_read_data[p.reaction_stamp_word] == p.reaction_stamp;
            for (uint w = 0; w < external_force_words; ++w) {
                counter_data[probe_reaction + w] = watched ? reaction_read_data[pc.reaction_body * external_force_words + w] : 0u;
            }
            if (pc.digest != 0) {
                counter_data[probe_digest_step] = pc.step;
                counter_data[probe_digest_valid] = 1u;
            }
        }
    }

    uint blade;
    if (!grass_tile_blade(p, tile, local, blade)) {
        return;
    }

    if (!awake) {
        if (pc.track != 0) {
            probe_recovery_check(blade, 1.0);
            probe_contact_recovery_check(blade, 1.0, true);
        }
        return;
    }

    uint slot_blade = (tile_state.slot_plus_one - 1u) * grass_tile_blade_count + local;
    uint base = slot_blade * grass_nodes;

    if (pc.digest != 0) {
        uint h = probe_mix(blade ^ 0x9e3779b9u);
        for (uint i = 0; i < grass_nodes; ++i) {
            h = probe_fold(h, node_data[base + i].position);
            h = probe_fold(h, node_data[base + i].velocity);
            h = probe_fold(h, node_data[base + i].plastic);
        }
        probe_fold_digest(h);
    }

    if (pc.track == 0) {
        return;
    }

    if (contact_read_data[slot_blade] != 0u) {
        last_contact_data[blade] = pc.step + 1;
    }

    grass_blade_rest shape = grass_rest_of(p, blade);
    float3 root = shape.points[0];
    float3 rest_tip = shape.points[grass_nodes - 1];
    float3 tip = node_data[base + grass_nodes - 1].position;
    float reference = max(rest_tip.y, 1e-4);
    float ratio = max(tip.y, 0.0) / reference;
    uint cell = grass_root_cell(p, root);

    uint cover = p.body_count > 0 ? probe_covering_body(p, cell, root, rest_tip, pc.flatten_ratio * reference) : probe_no_body;
    if (cover != probe_no_body) {
        InterlockedAdd(counter_data[probe_under_samples], 1u);
        InterlockedMax(counter_data[probe_under_max_ratio], asuint(ratio));
        uint bin = ratio <= 0.3 ? probe_under_bin_30 : (ratio <= 0.6 ? probe_under_bin_60 : (ratio <= 0.9 ? probe_under_bin_90 : probe_under_bin_rest));
        InterlockedAdd(counter_data[bin], 1u);
        float tip_distance = probe_tip_distance(p, cell, tip);
        if (tip_distance < 0.0) {
            InterlockedAdd(counter_data[probe_under_tip_inside], 1u);
            InterlockedMax(counter_data[probe_under_max_penetration], asuint(-tip_distance));
        }
        InterlockedMax(counter_data[probe_under_max_offset], asuint(length(tip.xz - rest_tip.xz)));
        if (ratio > pc.flatten_ratio) {
            float tip_underside;
            InterlockedAdd(counter_data[shape_underside(body_data[cover], p, tip.xz, 0.0, tip_underside) ? probe_high_under : probe_high_escaped], 1u);
        }
        if (probe_body_bottom(p, body_data[cover]) <= 2.0 * p.contact_margin) {
            InterlockedAdd(counter_data[probe_planted_samples], 1u);
            InterlockedMax(counter_data[probe_planted_max_ratio], asuint(ratio));
        }
        else {
            InterlockedAdd(counter_data[probe_airborne_samples], 1u);
            InterlockedMax(counter_data[probe_airborne_max_ratio], asuint(ratio));
        }
        if (last_under_data[blade] == 0) {
            InterlockedAdd(counter_data[probe_blades_touched], 1u);
        }
        last_under_data[blade] = pc.step + 1;
        return;
    }

    if (last_under_data[blade] == 0) {
        float3 plastic_tip = root;
        for (uint i = 1; i < grass_nodes; ++i) {
            plastic_tip += node_data[base + i].plastic;
        }
        InterlockedMax(counter_data[probe_calm_max_drift], asuint(length(tip - plastic_tip)));
        InterlockedMax(counter_data[probe_calm_max_speed], asuint(length(node_data[base + grass_nodes - 1].velocity)));
        return;
    }

    probe_recovery_check(blade, ratio);
    probe_contact_recovery_check(blade, ratio, false);
}
