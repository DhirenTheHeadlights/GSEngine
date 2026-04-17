import common;
import vbd_shared;
import vbd_physics;

[[vk::push_constant]]
ConstantBuffer<vbd_push_constants> pc;

static const uint WG_SIZE = 64;

groupshared uint gs_body_flags[MAX_BODIES];
groupshared uint gs_body_color[MAX_BODIES];
groupshared uint gs_color_group_count[MAX_COLORS];
groupshared uint gs_color_group_bodies[MAX_BODIES];
groupshared uint gs_max_used_color;

[Layout(LayoutKind::VBDPhysics)]
[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID) {
    uint tid = group_thread_id.x;
    uint body_count = pc.body_count;
    uint num_contacts = min(collision_state[0], MAX_CONTACTS);
    uint num_joints = min(pc.joint_count, MAX_JOINTS);

    for (uint bi = tid; bi < body_count; bi += WG_SIZE) {
        contact_offsets[bi] = 0;
        contact_counts[bi] = 0;
        gs_body_flags[bi] = body_data[bi].flags;
    }
    AllMemoryBarrierWithGroupSync();

    for (uint ci = tid; ci < num_contacts; ci += WG_SIZE) {
        GpuContact c = contact_data[ci];
        if (c.body_a < body_count && c.body_b < body_count) {
            uint dummy;
            InterlockedAdd(contact_counts[c.body_a], 1u, dummy);
            InterlockedAdd(contact_counts[c.body_b], 1u, dummy);
        }
    }
    AllMemoryBarrierWithGroupSync();

    if (tid == 0) {
        uint running = 0;
        for (uint bi = 0; bi < body_count; bi++) {
            contact_offsets[bi] = running;
            running += contact_counts[bi];
            running = min(running, MAX_CONTACT_ADJACENCY);
        }
    }
    AllMemoryBarrierWithGroupSync();

    for (uint bi = tid; bi < body_count; bi += WG_SIZE) {
        contact_counts[bi] = 0;
    }
    AllMemoryBarrierWithGroupSync();

    for (uint ci = tid; ci < num_contacts; ci += WG_SIZE) {
        GpuContact c = contact_data[ci];
        if (c.body_a < body_count && c.body_b < body_count) {
            uint slot_a;
            InterlockedAdd(contact_counts[c.body_a], 1u, slot_a);
            uint off_a = contact_offsets[c.body_a] + slot_a;
            if (off_a < MAX_CONTACT_ADJACENCY) {
                contact_adjacency[off_a] = ci;
            }

            uint slot_b;
            InterlockedAdd(contact_counts[c.body_b], 1u, slot_b);
            uint off_b = contact_offsets[c.body_b] + slot_b;
            if (off_b < MAX_CONTACT_ADJACENCY) {
                contact_adjacency[off_b] = ci;
            }
        }
    }
    AllMemoryBarrierWithGroupSync();

    for (uint bi = tid; bi < body_count; bi += WG_SIZE) {
        joint_offsets[bi] = 0;
        joint_counts[bi] = 0;
    }
    AllMemoryBarrierWithGroupSync();

    for (uint ji = tid; ji < num_joints; ji += WG_SIZE) {
        GpuJoint j = joint_data[ji];
        if (j.body_a < body_count && j.body_b < body_count) {
            uint dummy;
            InterlockedAdd(joint_counts[j.body_a], 1u, dummy);
            InterlockedAdd(joint_counts[j.body_b], 1u, dummy);
        }
    }
    AllMemoryBarrierWithGroupSync();

    if (tid == 0) {
        uint running = 0;
        for (uint bi = 0; bi < body_count; bi++) {
            joint_offsets[bi] = running;
            running += joint_counts[bi];
            running = min(running, MAX_JOINT_ADJACENCY);
        }
    }
    AllMemoryBarrierWithGroupSync();

    for (uint bi = tid; bi < body_count; bi += WG_SIZE) {
        joint_counts[bi] = 0;
    }
    AllMemoryBarrierWithGroupSync();

    for (uint ji = tid; ji < num_joints; ji += WG_SIZE) {
        GpuJoint j = joint_data[ji];
        if (j.body_a < body_count && j.body_b < body_count) {
            uint slot_a;
            InterlockedAdd(joint_counts[j.body_a], 1u, slot_a);
            uint off_a = joint_offsets[j.body_a] + slot_a;
            if (off_a < MAX_JOINT_ADJACENCY) {
                joint_adjacency[off_a] = ji;
            }

            uint slot_b;
            InterlockedAdd(joint_counts[j.body_b], 1u, slot_b);
            uint off_b = joint_offsets[j.body_b] + slot_b;
            if (off_b < MAX_JOINT_ADJACENCY) {
                joint_adjacency[off_b] = ji;
            }
        }
    }
    AllMemoryBarrierWithGroupSync();

    for (uint ji = tid; ji < num_joints; ji += WG_SIZE) {
        GpuJoint j = joint_data[ji];
        if (j.body_a >= body_count || j.body_b >= body_count) continue;
        GpuBody ba = body_data[j.body_a];
        GpuBody bb = body_data[j.body_b];
        float3 r_aw = rotate_vector(ba.orientation, j.local_anchor_a.xyz);
        float3 r_bw = rotate_vector(bb.orientation, j.local_anchor_b.xyz);
        float3 d = (ba.position.xyz + r_aw) - (bb.position.xyz + r_bw);

        if (j.type == JOINT_TYPE_DISTANCE) {
            j.pos_c0.x = length(d) - j.target_distance;
        }
        else if (j.type == JOINT_TYPE_FIXED || j.type == JOINT_TYPE_HINGE) {
            j.pos_c0 = float4(d.x, d.y, d.z, 0);

            float4 q_error = quat_mul(bb.orientation, quat_mul(quat_conjugate(ba.orientation), quat_conjugate(j.rest_orientation)));
            float3 theta = to_axis_angle(q_error);

            if (j.type == JOINT_TYPE_FIXED) {
                j.ang_c0 = float4(theta, 0);
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
        else if (j.type == JOINT_TYPE_SLIDER) {
            float3 axis_w = normalize(rotate_vector(ba.orientation, j.local_axis_a.xyz));
            float3 perp0 = cross(axis_w, float3(0, 1, 0));
            if (length(perp0) < 1e-6) perp0 = cross(axis_w, float3(1, 0, 0));
            perp0 = normalize(perp0);
            float3 perp1 = normalize(cross(axis_w, perp0));
            j.pos_c0.x = dot(perp0, d);
            j.pos_c0.y = dot(perp1, d);

            float4 q_error = quat_mul(bb.orientation, quat_mul(quat_conjugate(ba.orientation), quat_conjugate(j.rest_orientation)));
            float3 theta = to_axis_angle(q_error);
            j.ang_c0 = float4(theta, 0);
        }

        joint_data[ji] = j;
    }
    AllMemoryBarrierWithGroupSync();

    for (uint bi = tid; bi < body_count; bi += WG_SIZE) {
        gs_body_color[bi] = 0xFFFFFFFF;
    }
    for (uint c = tid; c < MAX_COLORS; c += WG_SIZE) {
        gs_color_group_count[c] = 0;
    }
    if (tid == 0) {
        gs_max_used_color = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    if (tid == 0) {
        for (uint bi = 0; bi < body_count; bi++) {
            if ((gs_body_flags[bi] & FLAG_LOCKED) != 0) continue;

            uint cc = contact_counts[bi];
            uint jc = joint_counts[bi];
            if (cc == 0 && jc == 0) continue;

            uint neighbor_colors = 0;
            uint off = contact_offsets[bi];
            for (uint k = 0; k < cc; k++) {
                uint idx = off + k;
                if (idx >= MAX_CONTACT_ADJACENCY) break;
                uint ci = contact_adjacency[idx];
                if (ci >= num_contacts) continue;
                GpuContact c = contact_data[ci];
                uint other = (c.body_a == bi) ? c.body_b : c.body_a;
                if (other < body_count && gs_body_color[other] < MAX_COLORS) {
                    neighbor_colors |= (1u << gs_body_color[other]);
                }
            }

            uint j_off = joint_offsets[bi];
            for (uint k = 0; k < jc; k++) {
                uint idx = j_off + k;
                if (idx >= MAX_JOINT_ADJACENCY) break;
                uint ji = joint_adjacency[idx];
                if (ji >= num_joints) continue;
                GpuJoint j = joint_data[ji];
                uint other = (j.body_a == bi) ? j.body_b : j.body_a;
                if (other < body_count && gs_body_color[other] < MAX_COLORS) {
                    neighbor_colors |= (1u << gs_body_color[other]);
                }
            }

            uint chosen = 0;
            while (chosen < MAX_COLORS && (neighbor_colors & (1u << chosen)) != 0) {
                chosen++;
            }

            if (chosen < MAX_COLORS) {
                gs_body_color[bi] = chosen;
            }
        }

        uint max_contact_color = 0;
        for (uint bi = 0; bi < body_count; bi++) {
            if (gs_body_color[bi] < MAX_COLORS && gs_body_color[bi] + 1 > max_contact_color) {
                max_contact_color = gs_body_color[bi] + 1;
            }
        }

        uint motor_only_color = max_contact_color < MAX_COLORS ? max_contact_color : 0;
        bool used_motor_only_color = false;

        for (uint bi = 0; bi < body_count; bi++) {
            if ((gs_body_flags[bi] & FLAG_LOCKED) != 0) continue;

            uint cc = contact_counts[bi];
            bool has_motor = motor_map[bi] != 0xFFFFFFFF;
            uint jc = joint_counts[bi];

            if (cc == 0 && !has_motor && jc == 0) continue;

            if (gs_body_color[bi] == 0xFFFFFFFF) {
                gs_body_color[bi] = motor_only_color;
                used_motor_only_color = true;
            }
        }

        uint free_body_color =
            (used_motor_only_color && motor_only_color + 1 < MAX_COLORS)
                ? (motor_only_color + 1)
                : motor_only_color;
        for (uint bi = 0; bi < body_count; bi++) {
            if ((gs_body_flags[bi] & FLAG_LOCKED) != 0) continue;

            uint cc = contact_counts[bi];
            bool has_motor = motor_map[bi] != 0xFFFFFFFF;
            uint jc = joint_counts[bi];

            if (cc != 0 || has_motor || jc != 0) continue;

            if (gs_body_color[bi] == 0xFFFFFFFF) {
                gs_body_color[bi] = free_body_color;
            }
        }

        uint max_used_color = 0;
        for (uint bi = 0; bi < body_count; bi++) {
            if (gs_body_color[bi] < MAX_COLORS) {
                gs_color_group_count[gs_body_color[bi]]++;
                if (gs_body_color[bi] + 1 > max_used_color) {
                    max_used_color = gs_body_color[bi] + 1;
                }
            }
        }

        collision_state[1] = max_used_color;
        gs_max_used_color = max_used_color;

        uint color_offsets_arr[MAX_COLORS];
        uint fill_offsets[MAX_COLORS];
        uint idx = 0;
        for (uint c = 0; c < max_used_color; c++) {
            color_offsets_arr[c] = idx;
            fill_offsets[c] = 0;
            idx += gs_color_group_count[c];
        }

        for (uint bi = 0; bi < body_count; bi++) {
            uint c = gs_body_color[bi];
            if (c >= max_used_color) continue;
            uint pos = color_offsets_arr[c] + fill_offsets[c];
            gs_color_group_bodies[pos] = bi;
            fill_offsets[c]++;
        }

        for (uint c = 0; c < max_used_color; c++) {
            color_data[c] = color_offsets_arr[c];
            color_data[MAX_COLORS + c] = gs_color_group_count[c];
        }
        for (uint c = max_used_color; c < MAX_COLORS; c++) {
            color_data[MAX_COLORS + c] = 0;
        }
        for (uint i = 0; i < idx; i++) {
            color_data[MAX_COLORS * 2 + i] = gs_color_group_bodies[i];
        }
    }
}
