void vbd_gather_snapshot(uint3 dispatch_id) {
    uint bi = dispatch_id.x;
    if (bi >= pc.body_count) return;

    body_state body = snapshot_source[bi];
    uint linger = snapshot_linger[bi];
    if (pc.report_all != 0 || (body.locked == 0 && body.sleep_counter < pc.sleep_threshold)) {
        linger = pc.linger_generations;
    }
    if (linger == 0) return;
    snapshot_linger[bi] = linger - 1;

    uint slot;
    InterlockedAdd(snapshot_count[0], 1, slot);
    if (slot >= pc.capacity) return;

    body_snapshot out;
    out.position = body.position;
    out.sleep_counter = body.sleep_counter;
    out.velocity = body.velocity;
    out.accel_weight = body.accel_weight;
    out.orientation = body.orientation;
    out.angular_velocity = body.angular_velocity;
    out.com_local = body.com_local;
    snapshot_indices[slot] = bi;
    snapshot_target[slot] = out;
}
