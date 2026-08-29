
float joint_effective_mass(body_state body_a, body_state body_b, float3 rAW, float3 rBW, float3 dir) {
    float inv_mass_sum = 0.0;
    if (body_a.locked == 0 && body_a.mass > 1e-10) inv_mass_sum += 1.0 / body_a.mass;
    if (body_b.locked == 0 && body_b.mass > 1e-10) inv_mass_sum += 1.0 / body_b.mass;
    if (body_a.update_orientation != 0 && body_a.locked == 0) {
        float3 ang_j_a = cross(rAW, dir);
        inv_mass_sum += dot(cross(mat3_mul_vec(body_a.inv_inertia, ang_j_a), rAW), dir);
    }
    if (body_b.update_orientation != 0 && body_b.locked == 0) {
        float3 ang_j_b = cross(rBW, dir);
        inv_mass_sum += dot(cross(mat3_mul_vec(body_b.inv_inertia, ang_j_b), rBW), dir);
    }
    if (isinf(inv_mass_sum) || isnan(inv_mass_sum) || inv_mass_sum <= 1e-10) return 0.0;
    return 1.0 / inv_mass_sum;
}

void collision_sort_adjacency(uint3 dispatch_id) {
    uint idx = dispatch_id.x;
    uint body_count = pc.body_count;
    uint num_joints = min(pc.joint_count, max_joints);

    if (idx < body_count && body_data[idx].locked == 0) {
        uint coff = contact_offsets[idx];
        uint ccnt = min(contact_counts[idx], max_contact_adjacency - coff);
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
        uint jcnt = min(joint_counts[idx], max_joint_adjacency - joff);
        for (uint ja = 1u; ja < jcnt; ja++) {
            uint ji_a = joint_adjacency[joff + ja];
            joint_constraint jca = joint_data[ji_a];
            uint joa = (jca.body_a == idx) ? jca.body_b : jca.body_a;
            float3 jka = (jca.body_a == idx) ? jca.local_anchor_a.xyz : jca.local_anchor_b.xyz;
            uint jb = ja;
            while (jb > 0u) {
                uint ji_p = joint_adjacency[joff + jb - 1u];
                joint_constraint jcp = joint_data[ji_p];
                uint jop = (jcp.body_a == idx) ? jcp.body_b : jcp.body_a;
                float3 jkp = (jcp.body_a == idx) ? jcp.local_anchor_a.xyz : jcp.local_anchor_b.xyz;
                bool jgt = (jop > joa) || (jop == joa && (jkp.x > jka.x || (jkp.x == jka.x && (jkp.y > jka.y || (jkp.y == jka.y && (jkp.z > jka.z || (jkp.z == jka.z && ji_p > ji_a)))))));
                if (!jgt) break;
                joint_adjacency[joff + jb] = ji_p;
                jb--;
            }
            joint_adjacency[joff + jb] = ji_a;
        }
    }

    if (idx < num_joints) {
        uint ji = idx;
        joint_constraint j = joint_data[ji];
        if (j.body_a >= body_count || j.body_b >= body_count) return;
        body_state ba = body_data[j.body_a];
        body_state bb = body_data[j.body_b];
        float3 r_aw = rotate_vector(ba.orientation, j.local_anchor_a.xyz);
        float3 r_bw = rotate_vector(bb.orientation, j.local_anchor_b.xyz);
        float3 d = (ba.position.xyz + r_aw) - (bb.position.xyz + r_bw);

        {
            float wgamma = solver_config_data[0].gamma;
            float wh2 = pc.h_squared;

            j.pos_lambda *= wgamma;
            j.ang_lambda *= wgamma;
            if (j.limits_enabled != 0) j.limit_lambda *= wgamma;

            int num_pos_rows = 3;
            if (j.type == joint_type::distance || j.type == joint_type::muscle) num_pos_rows = 1;
            else if (j.type == joint_type::slider) num_pos_rows = 2;

            for (int wk = 0; wk < num_pos_rows; wk++) {
                float3 wdir;
                if (j.type == joint_type::distance || j.type == joint_type::muscle) {
                    wdir = (length(d) > 1e-7) ? normalize(d) : float3(0, 1, 0);
                }
                else if (j.type == joint_type::slider) {
                    float3 axis_w = rotate_vector(ba.orientation, j.local_axis_a.xyz);
                    float3 perp0 = cross(axis_w, float3(0, 1, 0));
                    if (length(perp0) < 1e-6) perp0 = cross(axis_w, float3(1, 0, 0));
                    perp0 = normalize(perp0);
                    wdir = (wk == 0) ? perp0 : normalize(cross(axis_w, perp0));
                }
                else {
                    wdir = (wk == 0) ? float3(1, 0, 0) : ((wk == 1) ? float3(0, 1, 0) : float3(0, 0, 1));
                }
                float eff = joint_effective_mass(ba, bb, r_aw, r_bw, wdir) / wh2;
                float cap = solver_config_data[0].penalty_max;
                if (j.compliance > 0.0) cap = min(1.0 / max(j.compliance * wh2, 1e-10), solver_config_data[0].penalty_max);
                j.pos_penalty[wk] = clamp(max(j.pos_penalty[wk] * wgamma, eff), solver_config_data[0].penalty_min, cap);
            }

            int num_ang_rows = 3;
            if (j.type == joint_type::distance || j.type == joint_type::muscle || j.type == joint_type::ball) num_ang_rows = 0;
            else if (j.type == joint_type::hinge) num_ang_rows = 2;
            else if (j.type == joint_type::universal) num_ang_rows = 1;

            for (int wk = 0; wk < num_ang_rows; wk++) {
                float3 wadir;
                if (j.type == joint_type::hinge) {
                    float3 axis_a = rotate_vector(ba.orientation, j.local_axis_a.xyz);
                    float3 perp_u = cross(axis_a, float3(0, 1, 0));
                    if (length(perp_u) < 1e-6) perp_u = cross(axis_a, float3(1, 0, 0));
                    perp_u = normalize(perp_u);
                    wadir = (wk == 0) ? perp_u : normalize(cross(axis_a, perp_u));
                }
                else if (j.type == joint_type::universal) {
                    wadir = rotate_vector(ba.orientation, j.local_axis_a.xyz);
                }
                else {
                    wadir = (wk == 0) ? float3(1, 0, 0) : ((wk == 1) ? float3(0, 1, 0) : float3(0, 0, 1));
                }
                float inv_i_sum = 0.0;
                if (ba.locked == 0) inv_i_sum += dot(wadir, mat3_mul_vec(ba.inv_inertia, wadir));
                if (bb.locked == 0) inv_i_sum += dot(wadir, mat3_mul_vec(bb.inv_inertia, wadir));
                float eff_ang = (inv_i_sum > 1e-10) ? (1.0 / inv_i_sum / wh2) : solver_config_data[0].ang_penalty_min;
                j.ang_penalty[wk] = clamp(max(j.ang_penalty[wk] * wgamma, eff_ang), solver_config_data[0].ang_penalty_min, solver_config_data[0].ang_penalty_max);
            }

            if (j.limits_enabled != 0) {
                float3 limit_axis = rotate_vector(ba.orientation, j.local_axis_a.xyz);
                float inv_i_sum = 0.0;
                if (ba.locked == 0) inv_i_sum += dot(limit_axis, mat3_mul_vec(ba.inv_inertia, limit_axis));
                if (bb.locked == 0) inv_i_sum += dot(limit_axis, mat3_mul_vec(bb.inv_inertia, limit_axis));
                float eff_limit = (inv_i_sum > 1e-10) ? (1.0 / inv_i_sum / wh2) : solver_config_data[0].ang_penalty_min;
                j.limit_penalty = clamp(max(j.limit_penalty * wgamma, eff_limit), solver_config_data[0].ang_penalty_min, solver_config_data[0].ang_penalty_max);
            }
        }

        if (j.type == joint_type::distance) {
            j.pos_c0.x = length(d) - j.target_distance;
        }
        else if (j.type == joint_type::fixed || j.type == joint_type::hinge || j.type == joint_type::ball || j.type == joint_type::universal) {
            j.pos_c0 = d;

            if (j.type != joint_type::ball) {
                float4 q_error = quat_mul(bb.orientation, quat_mul(quat_conjugate(ba.orientation), quat_conjugate(j.rest_orientation)));
                float3 theta = to_axis_angle(q_error);

                if (j.type == joint_type::fixed) {
                    j.ang_c0 = theta;
                }
                else if (j.type == joint_type::universal) {
                    float3 axis_a = rotate_vector(ba.orientation, j.local_axis_a.xyz);
                    j.ang_c0.x = dot(axis_a, theta);
                }
                else {
                    float3 axis_a = rotate_vector(ba.orientation, j.local_axis_a.xyz);
                    float3 axis_b = rotate_vector(bb.orientation, j.local_axis_b.xyz);
                    float3 swing_error = cross(axis_a, axis_b);
                    float3 perp_u = cross(axis_a, float3(0, 1, 0));
                    if (length(perp_u) < 1e-6) perp_u = cross(axis_a, float3(1, 0, 0));
                    perp_u = normalize(perp_u);
                    float3 perp_v = normalize(cross(axis_a, perp_u));
                    j.ang_c0.x = dot(perp_u, swing_error);
                    j.ang_c0.y = dot(perp_v, swing_error);

                    if (j.limits_enabled != 0) {
                        float twist = dot(axis_a, theta);
                        if (twist < j.limit_lower) j.limit_c0 = twist - j.limit_lower;
                        else if (twist > j.limit_upper) j.limit_c0 = twist - j.limit_upper;
                        else j.limit_c0 = 0.0;
                    }
                }
            }
        }
        else if (j.type == joint_type::slider) {
            float3 axis_w = normalize(rotate_vector(ba.orientation, j.local_axis_a.xyz));
            float3 perp0 = cross(axis_w, float3(0, 1, 0));
            if (length(perp0) < 1e-6) perp0 = cross(axis_w, float3(1, 0, 0));
            perp0 = normalize(perp0);
            float3 perp1 = normalize(cross(axis_w, perp0));
            j.pos_c0.x = dot(perp0, d);
            j.pos_c0.y = dot(perp1, d);

            float4 q_error = quat_mul(bb.orientation, quat_mul(quat_conjugate(ba.orientation), quat_conjugate(j.rest_orientation)));
            float3 theta = to_axis_angle(q_error);
            j.ang_c0 = theta;
        }

        joint_data[ji] = j;
    }
}
