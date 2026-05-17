
uint ii = dispatch_id.x;
if (ii >= pc.impulse_count) return;

impulse_constraint imp = impulse_data[ii];
if (imp.body_index >= pc.body_count) return;

body_state body = body_data[imp.body_index];
if (body.locked != 0) return;

body.velocity += imp.delta_velocity;
body.sleep_counter = 0;
body_data[imp.body_index] = body;
