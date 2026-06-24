void vbd_apply_body_inputs(uint3 dispatch_id) {
    uint bi = dispatch_id.x;
    if (bi >= pc.body_count) return;

    body_state input = body_input_data[bi];
    if (pc.apply_all_body_inputs != 0 || input.locked != 0) {
        body_data[bi] = input;
    }
}
