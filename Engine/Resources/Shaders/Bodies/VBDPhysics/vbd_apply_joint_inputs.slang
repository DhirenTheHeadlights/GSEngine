void vbd_apply_joint_inputs(uint3 dispatch_id) {
    uint ji = dispatch_id.x;
    if (ji >= pc.joint_count) return;

    // A same-shape joint upload carries this tick's inputs (drive targets, activation,
    // limits, anchors) but not the duals: those live on the device and are what the
    // last dispatch converged to. Keep them, as the CPU solver keeps its own.
    joint_constraint input = joint_input_data[ji];
    joint_constraint resident = joint_data[ji];

    input.pos_lambda = resident.pos_lambda;
    input.pos_penalty = resident.pos_penalty;
    input.ang_lambda = resident.ang_lambda;
    input.ang_penalty = resident.ang_penalty;
    input.limit_lambda = resident.limit_lambda;
    input.limit_penalty = resident.limit_penalty;

    joint_data[ji] = input;
}
