void vbd_apply_joint_drive_inputs(uint3 dispatch_id) {
    uint k = dispatch_id.x;
    if (k >= joint_drive_input_index_data[0]) return;

    uint ji = joint_drive_input_index_data[1 + k];
    joint_drive_input input = joint_drive_input_data[k];
    joint_constraint resident = joint_data[ji];

    if (input.device_target == 0) {
        resident.drive_target = input.drive_target;
    }
    resident.activation = input.activation;
    resident.drive_stiffness = input.drive_stiffness;
    resident.drive_damping = input.drive_damping;
    resident.drive_max_torque = input.drive_max_torque;

    joint_data[ji] = resident;
}
