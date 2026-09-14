void nn_gae(uint3 dispatch_id) {
    uint e = dispatch_id.x;
    if (e >= pc.env_count) {
        return;
    }
    uint n = pc.sample_count;
    uint dones = pc.aux_offset;
    uint idxs = dones + n;
    uint starts = idxs + n;
    uint boot = starts + pc.env_count + 1;
    uint first = uint(nn_inputs[starts + e]);
    uint last = uint(nn_inputs[starts + e + 1]);
    float last_gae = 0.0;
    for (uint k = last; k-- > first;) {
        uint t = uint(nn_inputs[idxs + k]);
        float mask = 1.0 - nn_inputs[dones + t];
        float value = nn_inputs[pc.target_offset + t];
        float v_next = k + 1 < last ? nn_inputs[pc.target_offset + uint(nn_inputs[idxs + k + 1])] : nn_inputs[boot + e];
        float delta = nn_inputs[pc.input_offset + t] + pc.gamma * v_next * mask - value;
        last_gae = delta + pc.gamma * pc.lam * mask * last_gae;
        nn_inputs[pc.adv_offset + t] = last_gae;
        nn_inputs[pc.return_offset + t] = last_gae + value;
    }
}
