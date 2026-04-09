import common;
import vbd_shared;
import vbd_physics;

[[vk::push_constant]]
ConstantBuffer<vbd_push_constants> pc;

[Layout(LayoutKind::VBDPhysics)]
[shader("compute")]
[numthreads(1, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint num_contacts = min(collision_state[0], MAX_CONTACTS);
    uint body_count = pc.body_count;

    for (uint bi = 0; bi < body_count; bi++) {
        body_contact_map[bi] = 0;
        body_contact_map[MAX_BODIES + bi] = 0;
    }

    for (uint ci = 0; ci < num_contacts; ci++) {
        GpuContact c = contact_data[ci];
        if (c.body_a >= body_count || c.body_b >= body_count) continue;
        body_contact_map[MAX_BODIES + c.body_a]++;
        body_contact_map[MAX_BODIES + c.body_b]++;
    }

    uint running_offset = 0;
    for (uint bi = 0; bi < body_count; bi++) {
        body_contact_map[bi] = running_offset;
        running_offset += body_contact_map[MAX_BODIES + bi];
    }

    uint slot_offsets[MAX_BODIES];
    for (uint bi = 0; bi < body_count; bi++) {
        slot_offsets[bi] = 0;
    }

    for (uint ci = 0; ci < num_contacts; ci++) {
        GpuContact c = contact_data[ci];
        if (c.body_a >= body_count || c.body_b >= body_count) continue;
        uint off_a = body_contact_map[c.body_a] + slot_offsets[c.body_a];
        body_contact_map[MAX_BODIES * 2 + off_a] = ci;
        slot_offsets[c.body_a]++;

        uint off_b = body_contact_map[c.body_b] + slot_offsets[c.body_b];
        body_contact_map[MAX_BODIES * 2 + off_b] = ci;
        slot_offsets[c.body_b]++;
    }

    uint num_joints = pc.joint_count;
    for (uint bi = 0; bi < body_count; bi++) {
        body_contact_map[JOINT_MAP_BASE + bi] = 0;
        body_contact_map[JOINT_MAP_BASE + MAX_BODIES + bi] = 0;
    }

    for (uint ji = 0; ji < num_joints; ji++) {
        GpuJoint j = joint_data[ji];
        if (j.body_a >= body_count || j.body_b >= body_count) continue;
        body_contact_map[JOINT_MAP_BASE + MAX_BODIES + j.body_a]++;
        body_contact_map[JOINT_MAP_BASE + MAX_BODIES + j.body_b]++;
    }

    uint joint_running_offset = 0;
    for (uint bi = 0; bi < body_count; bi++) {
        body_contact_map[JOINT_MAP_BASE + bi] = joint_running_offset;
        joint_running_offset += body_contact_map[JOINT_MAP_BASE + MAX_BODIES + bi];
    }

    for (uint bi = 0; bi < body_count; bi++) {
        slot_offsets[bi] = 0;
    }

    for (uint ji = 0; ji < num_joints; ji++) {
        GpuJoint j = joint_data[ji];
        if (j.body_a >= body_count || j.body_b >= body_count) continue;
        uint off_a = body_contact_map[JOINT_MAP_BASE + j.body_a] + slot_offsets[j.body_a];
        body_contact_map[JOINT_MAP_BASE + MAX_BODIES * 2 + off_a] = ji;
        slot_offsets[j.body_a]++;

        uint off_b = body_contact_map[JOINT_MAP_BASE + j.body_b] + slot_offsets[j.body_b];
        body_contact_map[JOINT_MAP_BASE + MAX_BODIES * 2 + off_b] = ji;
        slot_offsets[j.body_b]++;
    }

    for (uint ji = 0; ji < num_joints; ji++) {
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

    uint body_color[MAX_BODIES];
    for (uint bi = 0; bi < body_count; bi++) {
        body_color[bi] = 0xFFFFFFFF;
    }

    uint color_group_count[MAX_COLORS];
    uint color_group_bodies[MAX_BODIES];
    for (uint c = 0; c < MAX_COLORS; c++) {
        color_group_count[c] = 0;
    }

    for (uint bi = 0; bi < body_count; bi++) {
        bool locked = (body_data[bi].flags & FLAG_LOCKED) != 0;
        if (locked) continue;

        uint cc = body_contact_map[MAX_BODIES + bi];
        uint jc = body_contact_map[JOINT_MAP_BASE + MAX_BODIES + bi];
        if (cc == 0 && jc == 0) continue;

        uint neighbor_colors = 0;
        uint off = body_contact_map[bi];
        for (uint k = 0; k < cc; k++) {
            uint ci = body_contact_map[MAX_BODIES * 2 + off + k];
            GpuContact con = contact_data[ci];
            uint other = (con.body_a == bi) ? con.body_b : con.body_a;
            if (body_color[other] < MAX_COLORS) {
                neighbor_colors |= (1u << body_color[other]);
            }
        }

        uint j_off = body_contact_map[JOINT_MAP_BASE + bi];
        for (uint k = 0; k < jc; k++) {
            uint ji = body_contact_map[JOINT_MAP_BASE + MAX_BODIES * 2 + j_off + k];
            GpuJoint jnt = joint_data[ji];
            uint other = (jnt.body_a == bi) ? jnt.body_b : jnt.body_a;
            if (body_color[other] < MAX_COLORS) {
                neighbor_colors |= (1u << body_color[other]);
            }
        }

        uint chosen = 0;
        while (chosen < MAX_COLORS && (neighbor_colors & (1u << chosen)) != 0) {
            chosen++;
        }

        if (chosen < MAX_COLORS) {
            body_color[bi] = chosen;
        }
    }

    uint max_contact_color = 0;
    for (uint bi = 0; bi < body_count; bi++) {
        if (body_color[bi] < MAX_COLORS && body_color[bi] + 1 > max_contact_color) {
            max_contact_color = body_color[bi] + 1;
        }
    }

    uint motor_only_color = max_contact_color < MAX_COLORS ? max_contact_color : 0;
    bool used_motor_only_color = false;

    uint motor_map_offset = MAX_BODIES * 2 + MAX_CONTACTS * 2;
    for (uint bi = 0; bi < body_count; bi++) {
        bool locked = (body_data[bi].flags & FLAG_LOCKED) != 0;
        if (locked) continue;

        uint cc = body_contact_map[MAX_BODIES + bi];
        bool has_motor = body_contact_map[motor_map_offset + bi] != 0xFFFFFFFF;
        uint jc = body_contact_map[JOINT_MAP_BASE + MAX_BODIES + bi];

        if (cc == 0 && !has_motor && jc == 0) continue;

        if (body_color[bi] == 0xFFFFFFFF) {
            body_color[bi] = motor_only_color;
            used_motor_only_color = true;
        }
    }

    uint free_body_color =
        (used_motor_only_color && motor_only_color + 1 < MAX_COLORS)
            ? (motor_only_color + 1)
            : motor_only_color;
    for (uint bi = 0; bi < body_count; bi++) {
        bool locked = (body_data[bi].flags & FLAG_LOCKED) != 0;
        if (locked) continue;

        uint cc = body_contact_map[MAX_BODIES + bi];
        bool has_motor = body_contact_map[motor_map_offset + bi] != 0xFFFFFFFF;
        uint jc = body_contact_map[JOINT_MAP_BASE + MAX_BODIES + bi];

        if (cc != 0 || has_motor || jc != 0) continue;

        if (body_color[bi] == 0xFFFFFFFF) {
            body_color[bi] = free_body_color;
        }
    }

    uint max_used_color = 0;
    for (uint bi = 0; bi < body_count; bi++) {
        if (body_color[bi] < MAX_COLORS) {
            color_group_count[body_color[bi]]++;
            if (body_color[bi] + 1 > max_used_color) {
                max_used_color = body_color[bi] + 1;
            }
        }
    }

    collision_state[1] = max_used_color;

    uint color_offsets[MAX_COLORS];
    uint idx = 0;
    for (uint c = 0; c < max_used_color; c++) {
        color_offsets[c] = idx;
        idx += color_group_count[c];
    }

    uint fill_offsets[MAX_COLORS];
    for (uint c = 0; c < max_used_color; c++) {
        fill_offsets[c] = 0;
    }

    for (uint bi = 0; bi < body_count; bi++) {
        uint c = body_color[bi];
        if (c >= max_used_color) continue;
        uint pos = color_offsets[c] + fill_offsets[c];
        color_group_bodies[pos] = bi;
        fill_offsets[c]++;
    }

    for (uint c = 0; c < max_used_color; c++) {
        color_data[c] = color_offsets[c];
        color_data[MAX_COLORS + c] = color_group_count[c];
    }
    for (uint c = max_used_color; c < MAX_COLORS; c++) {
        color_data[MAX_COLORS + c] = 0;
    }
    for (uint i = 0; i < idx; i++) {
        color_data[MAX_COLORS * 2 + i] = color_group_bodies[i];
    }
}
