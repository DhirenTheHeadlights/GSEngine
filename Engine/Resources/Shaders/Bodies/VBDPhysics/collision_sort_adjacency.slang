
float side_inverse_mass(body_state body, float3 r, float3 dir) {
    float w = body.mass > 1e-10 ? 1.0 / body.mass : 0.0;
    if (body.update_orientation != 0) {
        w += dot(cross(mat3_mul_vec(body.inv_inertia, cross(r, dir)), r), dir);
    }
    return w;
}

float joint_effective_mass(body_state body_a, body_state body_b, float3 rAW, float3 rBW, float3 dir) {
    bool any = false;
    float heaviest = 0.0;
    if (body_a.locked == 0) {
        heaviest = side_inverse_mass(body_a, rAW, dir);
        any = true;
    }
    if (body_b.locked == 0) {
        float w_b = side_inverse_mass(body_b, rBW, dir);
        heaviest = any ? min(heaviest, w_b) : w_b;
        any = true;
    }
    if (!any || isinf(heaviest) || isnan(heaviest) || heaviest <= 1e-10) return 0.0;
    return 1.0 / heaviest;
}

float joint_angular_floor(body_state body_a, body_state body_b, float3 axis, float h_squared) {
    bool any = false;
    float heaviest = 0.0;
    if (body_a.locked == 0) {
        heaviest = dot(axis, mat3_mul_vec(body_a.inv_inertia, axis));
        any = true;
    }
    if (body_b.locked == 0) {
        float w_b = dot(axis, mat3_mul_vec(body_b.inv_inertia, axis));
        heaviest = any ? min(heaviest, w_b) : w_b;
        any = true;
    }
    return (any && heaviest > 1e-10) ? (1.0 / heaviest / h_squared) : solver_config_data[0].ang_penalty_min;
}

void collision_sort_adjacency(uint3 dispatch_id) {
    uint idx = dispatch_id.x;
    uint body_count = pc.body_count;
    uint num_joints = min(pc.joint_count, max_joints());
    uint num_muscles = min(solver_config_data[0].muscle_count, max_muscles());

    uint num_contacts = min(collision_state[state_contact_count_index], max_contacts());
    uint thread_count = ((max(max(body_count, num_joints), num_muscles) + workgroup_size - 1) / workgroup_size) * workgroup_size;
    for (uint ci = idx; ci < num_contacts; ci += thread_count) {
        uint body_a = contact_data[ci].body_a;
        uint body_b = contact_data[ci].body_b;
        if (body_a >= body_count || body_b >= body_count) continue;
        uint island_a = body_env_data[body_a];
        uint island_b = body_env_data[body_b];
        bool locked_a = body_data[body_a].locked != 0;
        bool locked_b = body_data[body_b].locked != 0;
        bool a_owner = island_a != 0xFFFFFFFFu && !locked_a && (island_a == island_b || locked_b);
        bool b_owner = island_b != 0xFFFFFFFFu && !locked_b && (island_a == island_b || locked_a);
        if (a_owner || b_owner) continue;
        uint slot;
        InterlockedAdd(unowned_contacts[0], 1, slot);
        if (slot < max_contacts()) unowned_contacts[1 + slot] = ci;
    }

    if (idx < body_count && body_data[idx].locked == 0) {
        uint coff = contact_offsets[idx];
        uint ccnt = min(contact_counts[idx], max_contact_adjacency() - coff);
        for (uint sa = 1u; sa < ccnt; sa++) {
            uint ci_a = contact_adjacency[coff + sa];
            contact_constraint ca = contact_data[ci_a];
            uint oa = (ca.body_a == idx) ? ca.body_b : ca.body_a;
            float3 ka = (ca.body_a == idx) ? ca.local_anchor_a.xyz : ca.local_anchor_b.xyz;
            uint sb = sa;
            while (sb > 0u) {
                uint ci_p = contact_adjacency[coff + sb - 1u];
                contact_constraint cp = contact_data[ci_p];
                uint op = (cp.body_a == idx) ? cp.body_b : cp.body_a;
                float3 kp = (cp.body_a == idx) ? cp.local_anchor_a.xyz : cp.local_anchor_b.xyz;
                bool gt = (op > oa) || (op == oa && (kp.x > ka.x || (kp.x == ka.x && (kp.y > ka.y || (kp.y == ka.y && (kp.z > ka.z || (kp.z == ka.z && cp.feature_key > ca.feature_key)))))));
                if (!gt) break;
                contact_adjacency[coff + sb] = ci_p;
                sb--;
            }
            contact_adjacency[coff + sb] = ci_a;
        }

        uint joff = joint_offsets[idx];
        uint jcnt = min(joint_counts[idx], max_joint_adjacency() - joff);
        for (uint ja = 1u; ja < jcnt; ja++) {
            uint ji_a = joint_adjacency[joff + ja];
            bool muscle_a = (ji_a & muscle_adjacency_tag) != 0;
            joint_constraint jca = joint_data[muscle_a ? 0u : ji_a];
            uint joa = (jca.body_a == idx) ? jca.body_b : jca.body_a;
            float3 jka = (jca.body_a == idx) ? jca.local_anchor_a.xyz : jca.local_anchor_b.xyz;
            uint jb = ja;
            while (jb > 0u) {
                uint ji_p = joint_adjacency[joff + jb - 1u];
                bool muscle_p = (ji_p & muscle_adjacency_tag) != 0;
                joint_constraint jcp = joint_data[muscle_p ? 0u : ji_p];
                uint jop = (jcp.body_a == idx) ? jcp.body_b : jcp.body_a;
                float3 jkp = (jcp.body_a == idx) ? jcp.local_anchor_a.xyz : jcp.local_anchor_b.xyz;
                bool jgt = (muscle_a || muscle_p)
                    ? (muscle_p && (!muscle_a || ji_p > ji_a))
                    : ((jop > joa) || (jop == joa && (jkp.x > jka.x || (jkp.x == jka.x && (jkp.y > jka.y || (jkp.y == jka.y && (jkp.z > jka.z || (jkp.z == jka.z && ji_p > ji_a))))))));
                if (!jgt) break;
                joint_adjacency[joff + jb] = ji_p;
                jb--;
            }
            joint_adjacency[joff + jb] = ji_a;
        }
    }

    if (idx < num_muscles) {
        muscle_constraint m = muscle_data[idx];
        float target = saturate(muscle_excitation_data[idx]);
        float recruitment = 0.5 + 1.5 * m.activation;
        float lag = target > m.activation ? activation_time * recruitment : deactivation_time / recruitment;
        m.excitation = muscle_excitation_data[idx];
        m.activation = target + (m.activation - target) * exp(-pc.dt / lag);
        float path = 0.0;
        if (m.wrap_count != 0) {
            path = wrapped_start_length(m);
        }
        else {
            for (uint k = 0; k + 1 < m.path_count; k++) {
                path += length(start_muscle_point(muscle_path_data[m.path_first + k + 1]) - start_muscle_point(muscle_path_data[m.path_first + k]));
            }
        }
        m.path_length_start = path;
        muscle_data[idx] = m;
    }

    if (idx < num_joints) {
        uint ji = idx;
        joint_constraint j = joint_data[ji];
        if (j.body_a >= body_count || j.body_b >= body_count) return;
        body_state ba = body_data[j.body_a];
        body_state bb = body_data[j.body_b];
        if (ba.reset_pending == 1 || bb.reset_pending == 1) {
            j.pos_lambda = 0.0;
            j.pos_penalty = 0.0;
            j.ang_lambda = 0.0;
            j.ang_penalty = 0.0;
        }
        bool is_coupled = j.type == joint_type::coupled;
        coupled_frame coupled = {};
        if (is_coupled) {
            coupled = coupled_frame_of(j.curve_words, ba.orientation, bb.orientation);
        }
        float3 r_aw = rotate_vector(ba.orientation, j.local_anchor_a.xyz);
        float3 r_bw = rotate_vector(bb.orientation, j.local_anchor_b.xyz);
        float3 d = (ba.position.xyz + r_aw) - (bb.position.xyz + r_bw);
        float3 r_target = is_coupled ? r_aw + coupled.target_offset : r_aw;

        {
            float wgamma = solver_config_data[0].gamma;
            float wh2 = pc.h_squared;

            int num_pos_rows = 3;
            if (j.type == joint_type::distance) num_pos_rows = 1;
            else if (j.type == joint_type::slider) num_pos_rows = 2;

            for (int wk = 0; wk < num_pos_rows; wk++) {
                float3 wdir;
                if (j.type == joint_type::distance) {
                    wdir = (length(d) > 1e-7) ? normalize(d) : float3(0, 1, 0);
                }
                else if (j.type == joint_type::slider) {
                    wdir = body_perpendicular(ba.orientation, j.local_axis_a.xyz, wk);
                }
                else {
                    wdir = (wk == 0) ? float3(1, 0, 0) : ((wk == 1) ? float3(0, 1, 0) : float3(0, 0, 1));
                }
                float eff = joint_effective_mass(ba, bb, r_target, r_bw, wdir) / wh2;
                float cap = solver_config_data[0].penalty_max;
                if (j.compliance > 0.0) cap = min(1.0 / max(j.compliance * wh2, 1e-10), solver_config_data[0].penalty_max);
                j.pos_penalty[wk] = clamp(max(j.pos_penalty[wk], eff), solver_config_data[0].penalty_min, cap);
            }

            int num_ang_rows = 3;
            if (j.type == joint_type::distance || j.type == joint_type::ball) num_ang_rows = 0;
            else if (j.type == joint_type::hinge) num_ang_rows = 2;
            else if (is_coupled) num_ang_rows = int(3 - coupled.count);
            else if (j.type == joint_type::universal) num_ang_rows = 1;

            for (int wk = 0; wk < num_ang_rows; wk++) {
                float3 wadir;
                if (is_coupled) {
                    wadir = coupled.perpendiculars[wk];
                }
                else if (j.type == joint_type::hinge) {
                    wadir = body_perpendicular(ba.orientation, j.local_axis_a.xyz, wk);
                }
                else if (j.type == joint_type::universal) {
                    wadir = rotate_vector(ba.orientation, j.local_axis_a.xyz);
                }
                else {
                    wadir = (wk == 0) ? float3(1, 0, 0) : ((wk == 1) ? float3(0, 1, 0) : float3(0, 0, 1));
                }
                float eff_ang = joint_angular_floor(ba, bb, wadir, wh2);
                j.ang_penalty[wk] = clamp(max(j.ang_penalty[wk], eff_ang), solver_config_data[0].ang_penalty_min, solver_config_data[0].ang_penalty_max);
            }

            for (int wk = 0; wk < 3; wk++) {
                if ((j.limited_rows & (1u << wk)) == 0) continue;
                float3 limit_axis;
                if (is_coupled) {
                    limit_axis = coupled.coordinate_axes[uint(wk) + coupled.count - 3];
                    limit_axis *= coupled_limit_scale(limit_axis, solver_config_data[0].coupled_limit_normalized);
                }
                else limit_axis = angular_limit_axis(ba.orientation, j.local_axis_a.xyz, j.type == joint_type::ball, wk);
                float eff_limit = joint_angular_floor(ba, bb, limit_axis, wh2);
                j.ang_penalty[wk] = clamp(max(j.ang_penalty[wk] * wgamma, eff_limit), solver_config_data[0].ang_penalty_min, solver_config_data[0].ang_penalty_max);
            }
        }

        if (j.type == joint_type::distance) {
            j.pos_c0.x = length(d) - j.target_distance;
        }
        else if (is_coupled) {
            j.pos_c0 = (ba.position.xyz + r_aw + coupled.target_offset) - (bb.position.xyz + r_bw);
            float3 phi = coupled_swing(coupled);
            for (int lk = 0; lk < int(3 - coupled.count); lk++) {
                j.ang_c0[lk] = dot(coupled.perpendiculars[lk], phi);
            }
            for (uint ck = 0; ck < coupled.count; ck++) {
                int slot = coupled_slot(coupled, ck);
                if ((j.limited_rows & (1u << slot)) == 0) continue;
                float coordinate = coupled.coordinates[ck];
                if (coordinate < j.limit_lower[slot]) j.ang_c0[slot] = coordinate - j.limit_lower[slot];
                else if (coordinate > j.limit_upper[slot]) j.ang_c0[slot] = coordinate - j.limit_upper[slot];
                else j.ang_c0[slot] = 0.0;
            }
        }
        else if (j.type == joint_type::fixed || j.type == joint_type::hinge || j.type == joint_type::ball || j.type == joint_type::universal) {
            j.pos_c0 = d;

            if (j.type != joint_type::ball || j.limited_rows != 0) {
                float4 q_error = quat_mul(bb.orientation, quat_mul(quat_conjugate(ba.orientation), quat_conjugate(j.rest_orientation)));
                float3 theta = to_axis_angle(q_error);

                for (int lk = 0; lk < 3; lk++) {
                    if ((j.limited_rows & (1u << lk)) == 0) continue;
                    float coordinate = dot(angular_limit_axis(ba.orientation, j.local_axis_a.xyz, j.type == joint_type::ball, lk), theta);
                    if (coordinate < j.limit_lower[lk]) j.ang_c0[lk] = coordinate - j.limit_lower[lk];
                    else if (coordinate > j.limit_upper[lk]) j.ang_c0[lk] = coordinate - j.limit_upper[lk];
                    else j.ang_c0[lk] = 0.0;
                }

                if (j.type == joint_type::fixed) {
                    j.ang_c0 = theta;
                }
                else if (j.type == joint_type::universal) {
                    float3 axis_a = rotate_vector(ba.orientation, j.local_axis_a.xyz);
                    j.ang_c0.x = dot(axis_a, theta);
                }
                else if (j.type == joint_type::hinge) {
                    float3 axis_a = rotate_vector(ba.orientation, j.local_axis_a.xyz);
                    float3 axis_b = rotate_vector(bb.orientation, j.local_axis_b.xyz);
                    float3 swing_error = cross(axis_a, axis_b);
                    j.ang_c0.x = dot(body_perpendicular(ba.orientation, j.local_axis_a.xyz, 0), swing_error);
                    j.ang_c0.y = dot(body_perpendicular(ba.orientation, j.local_axis_a.xyz, 1), swing_error);
                }
            }
        }
        else if (j.type == joint_type::slider) {
            j.pos_c0.x = dot(body_perpendicular(ba.orientation, j.local_axis_a.xyz, 0), d);
            j.pos_c0.y = dot(body_perpendicular(ba.orientation, j.local_axis_a.xyz, 1), d);

            float4 q_error = quat_mul(bb.orientation, quat_mul(quat_conjugate(ba.orientation), quat_conjugate(j.rest_orientation)));
            float3 theta = to_axis_angle(q_error);
            j.ang_c0 = theta;
        }

        joint_data[ji].pos_lambda = j.pos_lambda;
        joint_data[ji].pos_penalty = j.pos_penalty;
        joint_data[ji].ang_lambda = j.ang_lambda;
        joint_data[ji].ang_penalty = j.ang_penalty;
        joint_data[ji].pos_c0 = j.pos_c0;
        joint_data[ji].ang_c0 = j.ang_c0;
    }
}
