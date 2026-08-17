void vbd_apply_body_inputs(uint3 dispatch_id) {
    uint bi = dispatch_id.x;
    if (bi >= pc.body_count) return;

    body_state input = body_input_data[bi];
    if (pc.apply_all_body_inputs != 0 || input.locked != 0 || input.reset_pending != 0) {
        if (pc.preserve_accel_weight != 0) {
            input.accel_weight = body_data[bi].accel_weight;
        }
        if (pc.apply_all_body_inputs == 0 && input.reset_pending == 0) {
            input.sleep_counter = body_data[bi].sleep_counter;
            input.accel_weight = body_data[bi].accel_weight;
        }
        body_data[bi] = input;
        body_data[bi].reset_pending = 0;
    }
}
