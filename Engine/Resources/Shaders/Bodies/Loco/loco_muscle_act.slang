float loco_excitation_of(float action, uint k) {
    float center = loco_action_origin != 0 ? loco_muscle_constants_data[k].excitation_center : loco_excitation_center;
    return 1.0 / (1.0 + exp(-loco_excitation_gain * (action - center)));
}

float loco_drive_of(uint action, uint k, bool dep) {
    if (loco_synergy_side == 0 || dep) return loco_act_data[action + pc.target_dim + k];
    uint map = pc.env_count * pc.act_dim + k * loco_synergy_side;
    uint first = action + pc.target_dim + (k / loco_synergy_side) * loco_synergy_side;
    float drive = 0.0;
    for (uint j = 0; j < loco_synergy_side; ++j) {
        drive += loco_act_data[map + j] * loco_act_data[first + j];
    }
    return drive;
}

void loco_muscle_act(uint3 dispatch_id) {
    uint e = dispatch_id.x;
    if (e >= pc.env_count) return;

    loco_muscle_row row = loco_muscle_table_data[e];
    bool driving = row.driving != 0;
    bool fresh = row.fresh != 0;
    bool dep = row.dep != 0;
    if (!driving && !fresh) return;

    uint action = e * pc.act_dim;
    for (uint k = 0; k < loco_muscle_count; ++k) {
        uint slot = row.slots[k];
        float rest = loco_muscle_constants_data[k].rest_activation;
        float excitation = driving ? (loco_action_space != 0 ? loco_act_data[pc.env_count * pc.act_dim + e * loco_muscle_count + k] : loco_excitation_of(loco_drive_of(action, k, dep), k)) : rest;
        loco_excitation_out_data[slot] = excitation;
        if (fresh) {
            muscle_constraint m = loco_muscle_out_data[slot];
            m.excitation = excitation;
            m.activation = rest;
            loco_muscle_out_data[slot] = m;
        }
    }

    if (!driving) return;

    joint_constraint resident = loco_joint_data[row.lumbar];
    resident.drive_target = clamp(row.rest + float3(loco_act_data[action + 0], loco_act_data[action + 1], loco_act_data[action + 2]) * pc.action_scale, row.lower, row.upper);
    loco_joint_data[row.lumbar] = resident;
}
