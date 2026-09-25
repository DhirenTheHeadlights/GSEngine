void vbd_gather_snapshot(uint3 dispatch_id) {
    uint bi = dispatch_id.x;
    if (bi >= pc.body_count) return;

    body_state body = snapshot_source[bi];
    body_snapshot out;
    out.position = body.position;
    out.sleep_counter = body.sleep_counter;
    out.velocity = body.velocity;
    out.accel_weight = body.accel_weight;
    out.orientation = body.orientation;
    out.angular_velocity = body.angular_velocity;
    out.com_local = body.com_local;
    snapshot_target[bi] = out;
}
