
static const uint ELEMS_PER_THREAD = (max_bodies + adjacency_workgroup_size - 1) / adjacency_workgroup_size;

groupshared uint gs_contact_counts[max_bodies];
groupshared uint gs_contact_offsets[max_bodies];
groupshared uint gs_joint_counts[max_bodies];
groupshared uint gs_joint_offsets[max_bodies];

// entry:
uint tid = group_thread_id.x;
uint body_count = pc.body_count;
uint num_contacts = min(collision_state[state_contact_count_index], max_contacts);
uint num_joints = min(pc.joint_count, max_joints);

[unroll]
for (uint k0 = 0; k0 < ELEMS_PER_THREAD; k0++) {
    uint bi = tid + k0 * adjacency_workgroup_size;
    if (bi < max_bodies) {
        gs_contact_counts[bi] = 0;
        gs_joint_counts[bi] = 0;
    }
}
for (uint gi = tid; gi < max_grounded_uints; gi += adjacency_workgroup_size) {
    grounded_bits[gi] = 0;
}
AllMemoryBarrierWithGroupSync();

for (uint ci = tid; ci < num_contacts; ci += adjacency_workgroup_size) {
    contact_constraint c = contact_data[ci];
    if (c.body_a < body_count && c.body_b < body_count) {
        uint dummy;
        InterlockedAdd(gs_contact_counts[c.body_a], 1u, dummy);
        InterlockedAdd(gs_contact_counts[c.body_b], 1u, dummy);
        if (c.normal.y > 0.7) {
            InterlockedOr(grounded_bits[c.body_a / 32u], 1u << (c.body_a % 32u), dummy);
        }
        if (c.normal.y < -0.7) {
            InterlockedOr(grounded_bits[c.body_b / 32u], 1u << (c.body_b % 32u), dummy);
        }
    }
}

for (uint ji = tid; ji < num_joints; ji += adjacency_workgroup_size) {
    joint_constraint j = joint_data[ji];
    if (j.body_a < body_count && j.body_b < body_count) {
        uint dummy;
        InterlockedAdd(gs_joint_counts[j.body_a], 1u, dummy);
        InterlockedAdd(gs_joint_counts[j.body_b], 1u, dummy);
    }
}
GroupMemoryBarrierWithGroupSync();

[unroll]
for (uint k1 = 0; k1 < ELEMS_PER_THREAD; k1++) {
    uint bi = tid + k1 * adjacency_workgroup_size;
    if (bi < max_bodies) {
        gs_contact_offsets[bi] = gs_contact_counts[bi];
        gs_joint_offsets[bi] = gs_joint_counts[bi];
    }
}
GroupMemoryBarrierWithGroupSync();

for (uint stride = 1; stride < max_bodies; stride <<= 1) {
    uint cv[ELEMS_PER_THREAD];
    uint jv[ELEMS_PER_THREAD];
    [unroll]
    for (uint kr = 0; kr < ELEMS_PER_THREAD; kr++) {
        uint bi = tid + kr * adjacency_workgroup_size;
        bool valid = bi < max_bodies && bi >= stride;
        cv[kr] = valid ? gs_contact_offsets[bi - stride] : 0u;
        jv[kr] = valid ? gs_joint_offsets[bi - stride] : 0u;
    }
    GroupMemoryBarrierWithGroupSync();
    [unroll]
    for (uint kw = 0; kw < ELEMS_PER_THREAD; kw++) {
        uint bi = tid + kw * adjacency_workgroup_size;
        if (bi < max_bodies) {
            gs_contact_offsets[bi] += cv[kw];
            gs_joint_offsets[bi] += jv[kw];
        }
    }
    GroupMemoryBarrierWithGroupSync();
}

[unroll]
for (uint k2 = 0; k2 < ELEMS_PER_THREAD; k2++) {
    uint bi = tid + k2 * adjacency_workgroup_size;
    if (bi < body_count) {
        uint cs = gs_contact_offsets[bi] - gs_contact_counts[bi];
        uint js = gs_joint_offsets[bi] - gs_joint_counts[bi];
        contact_offsets[bi] = min(cs, max_contact_adjacency);
        joint_offsets[bi] = min(js, max_joint_adjacency);
        gs_contact_counts[bi] = 0;
        gs_joint_counts[bi] = 0;
    }
}
AllMemoryBarrierWithGroupSync();

for (uint ci = tid; ci < num_contacts; ci += adjacency_workgroup_size) {
    contact_constraint c = contact_data[ci];
    if (c.body_a < body_count && c.body_b < body_count) {
        uint slot_a;
        InterlockedAdd(gs_contact_counts[c.body_a], 1u, slot_a);
        uint off_a = contact_offsets[c.body_a] + slot_a;
        if (off_a < max_contact_adjacency) {
            contact_adjacency[off_a] = ci;
        }

        uint slot_b;
        InterlockedAdd(gs_contact_counts[c.body_b], 1u, slot_b);
        uint off_b = contact_offsets[c.body_b] + slot_b;
        if (off_b < max_contact_adjacency) {
            contact_adjacency[off_b] = ci;
        }
    }
}

for (uint ji = tid; ji < num_joints; ji += adjacency_workgroup_size) {
    joint_constraint j = joint_data[ji];
    if (j.body_a < body_count && j.body_b < body_count) {
        uint slot_a;
        InterlockedAdd(gs_joint_counts[j.body_a], 1u, slot_a);
        uint off_a = joint_offsets[j.body_a] + slot_a;
        if (off_a < max_joint_adjacency) {
            joint_adjacency[off_a] = ji;
        }

        uint slot_b;
        InterlockedAdd(gs_joint_counts[j.body_b], 1u, slot_b);
        uint off_b = joint_offsets[j.body_b] + slot_b;
        if (off_b < max_joint_adjacency) {
            joint_adjacency[off_b] = ji;
        }
    }
}
AllMemoryBarrierWithGroupSync();

[unroll]
for (uint k3 = 0; k3 < ELEMS_PER_THREAD; k3++) {
    uint bi = tid + k3 * adjacency_workgroup_size;
    if (bi < body_count) {
        contact_counts[bi] = gs_contact_counts[bi];
        joint_counts[bi] = gs_joint_counts[bi];
    }
}

for (uint ji = tid; ji < num_joints; ji += adjacency_workgroup_size) {
    joint_constraint j = joint_data[ji];
    if (j.body_a >= body_count || j.body_b >= body_count) continue;
    body_state ba = body_data[j.body_a];
    body_state bb = body_data[j.body_b];
    float3 r_aw = rotate_vector(ba.orientation, j.local_anchor_a.xyz);
    float3 r_bw = rotate_vector(bb.orientation, j.local_anchor_b.xyz);
    float3 d = (ba.position.xyz + r_aw) - (bb.position.xyz + r_bw);

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
