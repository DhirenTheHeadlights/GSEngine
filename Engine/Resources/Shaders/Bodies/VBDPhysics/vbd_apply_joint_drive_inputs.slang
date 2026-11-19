void vbd_apply_joint_drive_inputs(uint3 dispatch_id) {
    uint k = dispatch_id.x;
    if (k >= joint_drive_input_index_data[0]) return;

    uint ji = joint_drive_input_index_data[1 + k];
    joint_drive_input input = joint_drive_input_data[k];

    if (input.device_target == 0) {
        joint_data[ji].drive_target = input.drive_target;
    }
    if (input.device_stiffness == 0) {
        joint_data[ji].drive_stiffness = input.drive_stiffness;
    }
    joint_data[ji].drive_damping = input.drive_damping;
    joint_data[ji].drive_max_torque = input.drive_max_torque;
}
