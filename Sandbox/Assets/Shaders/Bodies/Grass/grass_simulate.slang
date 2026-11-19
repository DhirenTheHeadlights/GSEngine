bool grass_body_contact(body_state body, grass_params p, float3 point, float weight, float lag, bool over_root, float root_floor, inout float3 f, inout mat3 hess) {
    float3 cn;
    float pen = p.contact_margin - shape_distance(body, p, point, lag, cn);
    if (pen <= 0.0) {
        return false;
    }
    float k = p.contact_stiffness * weight;
    if (over_root) {
        float underside;
        float floor_height = shape_underside(body, p, point.xz, lag, underside) ? max(underside - p.contact_margin, p.contact_margin) : root_floor;
        float crush = point.y - floor_height;
        if (crush > 0.0) {
            f.y -= k * crush;
            hess[1][1] += k * weight;
        }
    }
    else {
        float3 push = float3(cn.x, min(cn.y, 0.0) - length(cn.xz), cn.z);
        float push_len = length(push);
        push = push_len > 1e-4 && dot(push, cn) > 0.0 ? push / push_len : cn;
        f += k * pen * push;
        hess = mat3_add(hess, mat3_scale(outer_product3(push, push), k * weight));
    }
    return true;
}

bool grass_segment_contact(body_state body, grass_params p, float3 node, float3 neighbour, float lag, bool over_root, float root_floor, inout float3 f, inout mat3 hess) {
    float3 n;
    if (shape_distance(body, p, 0.5 * (node + neighbour), lag, n) > 0.5 * length(neighbour - node) + p.contact_margin) {
        return false;
    }
    float lo = 0.0;
    float hi = 1.0;
    for (uint i = 0; i < grass_segment_search_steps; ++i) {
        float a = lerp(lo, hi, 1.0 / 3.0);
        float b = lerp(lo, hi, 2.0 / 3.0);
        if (shape_distance(body, p, lerp(node, neighbour, a), lag, n) < shape_distance(body, p, lerp(node, neighbour, b), lag, n)) {
            hi = b;
        }
        else {
            lo = a;
        }
    }
    float t = 0.5 * (lo + hi);
    return grass_body_contact(body, p, lerp(node, neighbour, t), 1.0 - t, lag, over_root, root_floor, f, hess);
}

bool grass_node_contact(body_state body, grass_params p, float3 x[grass_nodes], uint j, float lag, bool over_root, float crush_height, inout float3 f, inout mat3 hess) {
    bool hit = grass_segment_contact(body, p, x[j], x[j - 1], lag, over_root, crush_height, f, hess);
    if (j + 1 < grass_nodes) {
        hit = grass_segment_contact(body, p, x[j], x[j + 1], lag, over_root, crush_height, f, hess) || hit;
    }
    if (over_root && x[j].y > crush_height) {
        f.y -= p.contact_stiffness * (x[j].y - crush_height);
        hess[1][1] += p.contact_stiffness;
        hit = true;
    }
    return hit;
}

float3 grass_bend_target(grass_params p, float3 x[grass_nodes], float3 plastic[grass_nodes], float rest_len[grass_nodes], uint j) {
    float kb = p.bend_stiffness / (rest_len[j] * rest_len[j]);
    float3 preload = -p.node_mass * p.local_gravity / kb;
    if (j >= 2) {
        float3 cur_parent = x[j - 1] - x[j - 2];
        float cur_len = length(cur_parent);
        if (cur_len > 1e-7) {
            return x[j - 1] + rotate_between(normalize(plastic[j - 1]), cur_parent / cur_len, plastic[j]) + preload;
        }
    }
    return x[j - 1] + plastic[j] + preload;
}

float3 grass_node_load(grass_params p, float3 x[grass_nodes], float3 plastic[grass_nodes], float rest_len[grass_nodes], uint j) {
    float3 load = p.node_mass * p.local_gravity;
    for (uint side = 0; side < 2; ++side) {
        uint k = side == 0 ? j - 1 : j + 1;
        if (k >= grass_nodes) {
            continue;
        }
        float rl = side == 0 ? rest_len[j] : rest_len[j + 1];
        float3 dvec = x[j] - x[k];
        float l = length(dvec);
        if (l > rl) {
            load -= p.stretch_stiffness * (l - rl) * (dvec / l);
        }
    }
    float kb = p.bend_stiffness / (rest_len[j] * rest_len[j]);
    return load - kb * (x[j] - grass_bend_target(p, x, plastic, rest_len, j));
}

void grass_accumulate_reaction(grass_params p, float3 x[grass_nodes], float3 plastic[grass_nodes], float rest_len[grass_nodes], float lag, uint contacts[grass_max_contacts], uint contact_count, bool over_root[grass_max_contacts], float crush_height[grass_max_contacts], inout float3 reaction[grass_max_contacts]) {
    for (uint j = 1; j < grass_nodes; ++j) {
        bool hit[grass_max_contacts];
        uint hits = 0;
        for (uint c = 0; c < contact_count; ++c) {
            float3 scratch_f = float3(0.0, 0.0, 0.0);
            mat3 scratch_hess = mat3_identity(0.0);
            hit[c] = grass_node_contact(body_data[contacts[c]], p, x, j, lag, over_root[c], crush_height[c], scratch_f, scratch_hess);
            hits += hit[c] ? 1u : 0u;
        }
        if (hits == 0) {
            continue;
        }
        float3 share = grass_node_load(p, x, plastic, rest_len, j) / float(hits);
        for (uint c = 0; c < contact_count; ++c) {
            if (hit[c]) {
                reaction[c] += share;
            }
        }
    }
}

void grass_add_fixed(uint word, float value) {
    uint2 bits = fixed_pair_encode(value, external_force_resolution);
    uint old_lo;
    InterlockedAdd(reaction_data[word], bits.x, old_lo);
    uint carry = old_lo + bits.x < old_lo ? 1u : 0u;
    InterlockedAdd(reaction_data[word + 1], bits.y + carry);
}

void grass_push_reaction(grass_params p, uint body, float3 local_force) {
    uint target = body < p.route_count ? route_data[body] : body;
    if (target >= p.reaction_bodies) {
        return;
    }
    float3 world_force = rotate_vector(p.field_orientation, local_force);
    uint base = target * external_force_words;
    grass_add_fixed(base + 0, world_force.x);
    grass_add_fixed(base + 2, world_force.y);
    grass_add_fixed(base + 4, world_force.z);
}

void grass_simulate(uint3 group_id, uint3 group_thread_id) {
    grass_params p = params_data[0];
    uint local = group_id.x * grass_sim_threads + group_thread_id.x;
    if (local >= grass_tile_blade_count) {
        return;
    }

    uint slot = awake_read_data[group_id.y];
    grass_slot slot_state = slot_data[slot];
    uint blade;
    if (!grass_tile_blade(p, slot_state.tile_plus_one - 1u, local, blade)) {
        return;
    }

    grass_blade_rest shape = grass_rest_of(p, blade);
    uint base = (slot * grass_tile_blade_count + local) * grass_nodes;
    float3 rest[grass_nodes];
    float3 x[grass_nodes];
    float3 v[grass_nodes];
    float3 plastic[grass_nodes];
    float rest_len[grass_nodes];
    for (uint i = 0; i < grass_nodes; ++i) {
        rest[i] = shape.points[i];
        if (slot_state.fresh != 0u) {
            x[i] = rest[i];
            v[i] = float3(0.0, 0.0, 0.0);
            plastic[i] = i == 0 ? float3(0.0, 0.0, 0.0) : rest[i] - rest[i - 1];
        }
        else {
            x[i] = node_data[base + i].position;
            v[i] = node_data[base + i].velocity;
            plastic[i] = node_data[base + i].plastic;
        }
        node_data[base + i].previous = x[i];
    }
    x[0] = rest[0];
    v[0] = float3(0.0, 0.0, 0.0);
    rest_len[0] = 0.0;
    for (uint i = 1; i < grass_nodes; ++i) {
        rest_len[i] = length(rest[i] - rest[i - 1]);
    }

    uint contacts[grass_max_contacts];
    uint contact_count = 0;
    {
        uint cell = grass_root_cell(p, rest[0]);
        uint cell_count = p.cells_per_side * p.cells_per_side;
        uint listed = min(grid_read_data[cell], min(p.cell_capacity, grass_max_contacts));
        for (uint k = 0; k < listed; ++k) {
            uint bi = grid_read_data[cell_count + cell * p.cell_capacity + k];
            uint at = contact_count;
            while (at > 0 && contacts[at - 1] > bi) {
                contacts[at] = contacts[at - 1];
                --at;
            }
            contacts[at] = bi;
            ++contact_count;
        }
    }

    float h = p.substep_dt;
    float inertia = p.node_mass / (h * h);
    float decay = exp(-p.damping * h);
    float kc = p.contact_stiffness;
    float ks = p.stretch_stiffness;
    float yield_blend = 1.0 - exp(-p.yield_rate * h);
    float recovery_step = p.recovery_rate * h;

    float3 reaction[grass_max_contacts];
    for (uint c = 0; c < grass_max_contacts; ++c) {
        reaction[c] = float3(0.0, 0.0, 0.0);
    }

    bool ever_touched = false;
    for (uint step = 0; step < p.substeps; ++step) {
        float lag = float(p.substeps - 1 - step) * h;
        float3 y[grass_nodes];
        float3 x_old[grass_nodes];
        bool touched[grass_nodes];
        bool blade_touched = false;
        y[0] = x[0];
        x_old[0] = x[0];
        touched[0] = false;
        for (uint j = 1; j < grass_nodes; ++j) {
            x_old[j] = x[j];
            touched[j] = false;
            x[j] += v[j] * h;
            y[j] = x[j] + p.local_gravity * (h * h);
        }

        bool over_root[grass_max_contacts];
        float crush_height[grass_max_contacts];
        for (uint c = 0; c < contact_count; ++c) {
            float underside;
            over_root[c] = shape_underside(body_data[contacts[c]], p, rest[0].xz, lag, underside);
            crush_height[c] = max(underside - p.contact_margin, p.contact_margin);
        }

        for (uint it = 0; it < p.iterations; ++it) {
            for (uint j = 1; j < grass_nodes; ++j) {
                float3 f = -inertia * (x[j] - y[j]);
                mat3 hess = mat3_identity(inertia);

                for (uint side = 0; side < 2; ++side) {
                    uint k = side == 0 ? j - 1 : j + 1;
                    if (k >= grass_nodes) {
                        continue;
                    }
                    float rl = side == 0 ? rest_len[j] : rest_len[j + 1];
                    float3 dvec = x[j] - x[k];
                    float l = length(dvec);
                    if (l < 1e-7) {
                        continue;
                    }
                    float3 nrm = dvec / l;
                    f -= ks * (l - rl) * nrm;
                    mat3 nn = outer_product3(nrm, nrm);
                    hess = mat3_add(hess, mat3_scale(nn, ks));
                    float lateral = ks * max(0.0, 1.0 - rl / l);
                    hess = mat3_add(hess, mat3_scale(mat3_add(mat3_identity(1.0), mat3_scale(nn, -1.0)), lateral));
                }

                float kb = p.bend_stiffness / (rest_len[j] * rest_len[j]);
                f -= kb * (x[j] - grass_bend_target(p, x, plastic, rest_len, j));
                hess = mat3_add(hess, mat3_identity(kb));

                for (uint c = 0; c < contact_count; ++c) {
                    bool hit = grass_node_contact(body_data[contacts[c]], p, x, j, lag, over_root[c], crush_height[c], f, hess);
                    touched[j] = touched[j] || hit;
                }

                float ground_pen = p.contact_margin - x[j].y;
                if (ground_pen > 0.0) {
                    f.y += kc * ground_pen;
                    hess[1][1] += kc;
                }

                x[j] += mat3_mul_vec(mat3_inverse(hess), f);
            }
        }

        for (uint j = 1; j < grass_nodes; ++j) {
            v[j] = (x[j] - x_old[j]) / h * decay;
            blade_touched = blade_touched || touched[j];
        }
        ever_touched = ever_touched || blade_touched;

        if (blade_touched && p.reaction_bodies != 0u) {
            grass_accumulate_reaction(p, x, plastic, rest_len, lag, contacts, contact_count, over_root, crush_height, reaction);
        }

        for (uint j = 1; j < grass_nodes; ++j) {
            float3 plastic_dir = normalize(plastic[j]);
            if (blade_touched) {
                if (!touched[j]) {
                    continue;
                }
                float3 seg = x[j] - x[j - 1];
                float seg_len = length(seg);
                if (seg_len < 1e-7) {
                    continue;
                }
                float3 seg_dir = seg / seg_len;
                float excess = acos(clamp(dot(plastic_dir, seg_dir), -1.0, 1.0)) - p.yield_angle;
                if (excess > 0.0) {
                    plastic[j] = rotate_toward(plastic_dir, seg_dir, excess * yield_blend) * rest_len[j];
                }
            }
            else {
                plastic[j] = rotate_toward(plastic_dir, normalize(rest[j] - rest[j - 1]), recovery_step) * rest_len[j];
            }
        }
    }

    contact_data[slot * grass_tile_blade_count + local] = ever_touched ? 1u : 0u;

    if (ever_touched && p.reaction_bodies != 0u && reaction_data[p.reaction_stamp_word] == p.reaction_stamp) {
        float inv_substeps = 1.0 / float(max(p.substeps, 1u));
        for (uint c = 0; c < contact_count; ++c) {
            if (any(reaction[c] != float3(0.0, 0.0, 0.0))) {
                grass_push_reaction(p, contacts[c], reaction[c] * inv_substeps);
            }
        }
    }

    bool calm = !ever_touched;
    for (uint i = 0; i < grass_nodes; ++i) {
        node_data[base + i].position = x[i];
        node_data[base + i].velocity = v[i];
        node_data[base + i].plastic = plastic[i];
        if (i > 0) {
            calm = calm
                && length(x[i] - rest[i]) <= p.sleep_tolerance
                && length(plastic[i] - (rest[i] - rest[i - 1])) <= p.sleep_tolerance
                && length(v[i]) <= p.sleep_speed;
        }
    }
    if (!calm) {
        slot_data[slot].unsettled = 1u;
    }
}
