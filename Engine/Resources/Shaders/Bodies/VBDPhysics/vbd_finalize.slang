
void vbd_finalize(uint3 dispatch_id) {
    uint bi = dispatch_id.x;
    if (bi >= pc.body_count) return;

    body_state body = body_data[bi];
    bool locked = body.locked != 0;
    bool sleeping = body.sleep_counter >= sleep_threshold;
    bool update_orient = body.update_orientation != 0;

    if (sleeping) return;

    if (locked) {
        body.position = body.predicted_position;
        body.orientation = body.predicted_orientation;
    }
    else {
        bool bad_pp = isinf(body.predicted_position.x) || isinf(body.predicted_position.y) || isinf(body.predicted_position.z)
                   || isnan(body.predicted_position.x) || isnan(body.predicted_position.y) || isnan(body.predicted_position.z);
        if (bad_pp) {
            body.predicted_position = body.old_position;
            body.predicted_orientation = body.old_orientation;
            body.velocity = float3(0, 0, 0);
            body.angular_velocity = float3(0, 0, 0);
            body.position = body.old_position;
            body.orientation = body.old_orientation;
            body_data[bi] = body;
            return;
        }

        float q_len_sq = body.predicted_orientation.x * body.predicted_orientation.x
                       + body.predicted_orientation.y * body.predicted_orientation.y
                       + body.predicted_orientation.z * body.predicted_orientation.z
                       + body.predicted_orientation.w * body.predicted_orientation.w;
        if (q_len_sq < 0.5 || isnan(q_len_sq)) {
            body.predicted_orientation = body.orientation;
            body.angular_velocity = float3(0, 0, 0);
        }

        body.position = body.predicted_position;
        if (update_orient) {
            body.orientation = body.predicted_orientation;
        }
    }

    float3 he = body.half_extents.xyz;
    if (dot(he, he) > 0.0) {
        float4 q = body.orientation;
        float3 ax = rotate_vector(q, float3(1, 0, 0));
        float3 ay = rotate_vector(q, float3(0, 1, 0));
        float3 az = rotate_vector(q, float3(0, 0, 1));

        float3 aabb_he;
        aabb_he.x = abs(ax.x) * he.x + abs(ay.x) * he.y + abs(az.x) * he.z;
        aabb_he.y = abs(ax.y) * he.x + abs(ay.y) * he.y + abs(az.y) * he.z;
        aabb_he.z = abs(ax.z) * he.x + abs(ay.z) * he.y + abs(az.z) * he.z;

        body.aabb_min = body.position.xyz - aabb_he;
        body.aabb_max = body.position.xyz + aabb_he;
    }

    body_data[bi] = body;
}
