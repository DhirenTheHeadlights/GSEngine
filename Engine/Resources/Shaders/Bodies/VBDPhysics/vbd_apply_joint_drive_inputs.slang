void vbd_apply_joint_drive_inputs(uint3 dispatch_id) {
    uint ji = dispatch_id.x;
    if (ji >= pc.joint_count) return;

    joint_drive_input input = joint_drive_input_data[ji];
    joint_constraint resident = joint_data[ji];

    resident.drive_target = input.drive_target;
    resident.activation = input.activation;
    resident.drive_stiffness = input.drive_stiffness;
    resident.drive_damping = input.drive_damping;
    resident.drive_max_torque = input.drive_max_torque;

    joint_data[ji] = resident;
}
