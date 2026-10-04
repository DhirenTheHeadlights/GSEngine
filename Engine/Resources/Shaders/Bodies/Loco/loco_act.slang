void loco_act_write_joint(uint slot, float3 target) {
    joint_constraint resident = loco_joint_data[slot];
    if (pc.action_filter < 1.0) {
        target = resident.drive_target + (target - resident.drive_target) * pc.action_filter;
    }
    resident.drive_target = target;
    loco_joint_data[slot] = resident;
}

void loco_act_write_stiffness(uint slot, float3 stiffness) {
    joint_constraint resident = loco_joint_data[slot];
    resident.drive_stiffness = stiffness;
    loco_joint_data[slot] = resident;
}

void loco_act(uint3 dispatch_id) {
    uint e = dispatch_id.x;
    if (e >= pc.env_count) return;

    uint out_base = e * pc.act_dim;
    uint mean_base = pc.mean_offset + e * pc.act_dim;

    float acting[loco_act_dim];

    for (uint k = 0; k < pc.act_dim; ++k) {
        float sampled = loco_act_mean_data[mean_base + k] + loco_act_sigma_data[k] * loco_act_noise_data[out_base + k];
        loco_act_out_data[out_base + k] = sampled;
        acting[k] = sampled;
    }

    if (pc.ring_len > 1) {
        uint ring_base = e * pc.ring_len * pc.act_dim;
        uint head = pc.ring_head % pc.ring_len;
        uint tail = (head + 1) % pc.ring_len;
        for (uint k = 0; k < pc.act_dim; ++k) {
            loco_act_ring_data[ring_base + head * pc.act_dim + k] = acting[k];
        }
        for (uint k = 0; k < pc.act_dim; ++k) {
            acting[k] = loco_act_ring_data[ring_base + tail * pc.act_dim + k];
        }
    }

    for (uint k = 0; k < loco_target_dim; ++k) {
        acting[k] *= pc.action_scale;
    }

    uint slot_base = e * loco_act_slot_stride;
    if (loco_act_slot_data[slot_base + loco_act_slot_valid] == 0) return;

    loco_act_write_joint(loco_act_slot_data[slot_base + 0], float3(acting[0], acting[1], acting[2]));
    loco_act_write_joint(loco_act_slot_data[slot_base + 1], float3(acting[3], 0.0, 0.0));
    loco_act_write_joint(loco_act_slot_data[slot_base + 2], float3(acting[4], 0.0, 0.0));
    loco_act_write_joint(loco_act_slot_data[slot_base + 3], float3(acting[5], acting[6], acting[7]));
    loco_act_write_joint(loco_act_slot_data[slot_base + 4], float3(acting[8], 0.0, 0.0));
    loco_act_write_joint(loco_act_slot_data[slot_base + 5], float3(acting[9], 0.0, 0.0));

    if (loco_variable_stiffness == 0) return;

    uint s = loco_target_dim;
    loco_act_write_stiffness(loco_act_slot_data[slot_base + 0], float3(loco_k_hip_pitch * loco_relative_stiffness(acting[s + 0]), loco_k_hip_yaw * loco_relative_stiffness(acting[s + 1]), loco_k_hip_roll * loco_relative_stiffness(acting[s + 2])));
    loco_act_write_stiffness(loco_act_slot_data[slot_base + 1], float3(loco_k_knee * loco_relative_stiffness(acting[s + 3]), 0.0, 0.0));
    loco_act_write_stiffness(loco_act_slot_data[slot_base + 2], float3(loco_k_ankle * loco_relative_stiffness(acting[s + 4]), 0.0, 0.0));
    loco_act_write_stiffness(loco_act_slot_data[slot_base + 3], float3(loco_k_hip_pitch * loco_relative_stiffness(acting[s + 5]), loco_k_hip_yaw * loco_relative_stiffness(acting[s + 6]), loco_k_hip_roll * loco_relative_stiffness(acting[s + 7])));
    loco_act_write_stiffness(loco_act_slot_data[slot_base + 4], float3(loco_k_knee * loco_relative_stiffness(acting[s + 8]), 0.0, 0.0));
    loco_act_write_stiffness(loco_act_slot_data[slot_base + 5], float3(loco_k_ankle * loco_relative_stiffness(acting[s + 9]), 0.0, 0.0));
}
