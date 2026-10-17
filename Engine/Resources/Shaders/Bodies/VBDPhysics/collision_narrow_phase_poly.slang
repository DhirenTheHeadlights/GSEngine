
void collision_narrow_phase_poly(uint3 dispatch_id) {
    uint pair_idx = dispatch_id.y * max_dispatch_row + dispatch_id.x / workgroup_size;
    uint lane = dispatch_id.x % workgroup_size;

    uint pair_count = min(collision_pairs[poly_pair_base], max_collision_pairs);
    if (dispatch_id.x == 0 && dispatch_id.y == 0) {
        InterlockedAdd(collision_state[state_pair_count_index], pair_count);
    }
    if (pair_idx >= pair_count) return;

    uint body_a_idx = collision_pairs[poly_pair_base + 1 + pair_idx * 2];
    uint body_b_idx = collision_pairs[poly_pair_base + 1 + pair_idx * 2 + 1];

    body_state ba = body_data[body_a_idx];
    body_state bb = body_data[body_b_idx];

    mat3 rot_a = rotation_from_quat(ba.orientation);
    mat3 rot_b = rotation_from_quat(bb.orientation);
    float3 axes_a[3] = { rot_a[0], rot_a[1], rot_a[2] };
    float3 axes_b[3] = { rot_b[0], rot_b[1], rot_b[2] };

    Polytope poly_a = make_polytope(ba.shape_kind, body_origin(ba.position.xyz, ba.orientation, ba.com_local.xyz), ba.half_extents.xyz, axes_a, ba.hull_offset, body_a_idx);
    Polytope poly_b = make_polytope(bb.shape_kind, body_origin(bb.position.xyz, bb.orientation, bb.com_local.xyz), bb.half_extents.xyz, axes_b, bb.hull_offset, body_b_idx);
    bool swap_shapes = ba.shape_kind > bb.shape_kind;
    Polytope lo = swap_shapes ? poly_b : poly_a;
    Polytope hi = swap_shapes ? poly_a : poly_b;

    SatResult sat = poly_sat_wave(lo, hi, pair_speculative_margin(ba, bb), lane);
    if (!sat.valid || lane != 0) return;

    ContactResult contacts = poly_contacts(lo, hi, sat.normal);
    if (swap_shapes) {
        sat.normal = -sat.normal;
        swap_contact_sides(contacts);
    }

    uint first_slot = 0;
    if (contacts.count > 0) {
        InterlockedAdd(collision_state[state_contact_count_index], uint(contacts.count), first_slot);
    }
    store_pair_contacts(first_slot, body_a_idx, body_b_idx, ba, bb, sat, contacts);
}
