float loco_excitation_of(float action) {
    return 1.0 / (1.0 + exp(-loco_excitation_gain * (action - loco_excitation_center)));
}

void loco_muscle_act(uint3 dispatch_id) {
    uint e = dispatch_id.x;
    if (e >= pc.env_count) return;

    loco_muscle_row row = loco_muscle_table_data[e];
    bool driving = row.driving != 0;
    bool fresh = row.fresh != 0;
    if (!driving && !fresh) return;

    uint action = e * pc.act_dim;
    for (uint k = 0; k < loco_muscle_count; ++k) {
        uint slot = row.slots[k];
        float excitation = driving ? loco_excitation_of(loco_act_data[action + pc.target_dim + k]) : 0.0;
        loco_excitation_out_data[slot] = excitation;
        if (fresh) {
            muscle_constraint m = loco_muscle_out_data[slot];
            m.excitation = excitation;
            m.activation = 0.0;
            loco_muscle_out_data[slot] = m;
        }
    }

    if (!driving) return;

    joint_constraint resident = loco_joint_data[row.lumbar];
    resident.drive_target = clamp(row.rest + float3(loco_act_data[action + 0], loco_act_data[action + 1], loco_act_data[action + 2]) * pc.action_scale, row.lower, row.upper);
    loco_joint_data[row.lumbar] = resident;
}
