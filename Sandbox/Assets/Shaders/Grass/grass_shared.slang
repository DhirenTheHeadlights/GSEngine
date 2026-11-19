float3 rotate_between(float3 from_dir, float3 to_dir, float3 v) {
    float c = dot(from_dir, to_dir);
    if (c < -0.999) {
        return v;
    }
    float3 axis = cross(from_dir, to_dir);
    return v * c + cross(axis, v) + axis * (dot(axis, v) / (1.0 + c));
}

float3 rotate_toward(float3 from_dir, float3 to_dir, float max_angle) {
    float c = clamp(dot(from_dir, to_dir), -1.0, 1.0);
    float between = acos(c);
    if (between <= max_angle) {
        return to_dir;
    }
    float3 axis = cross(from_dir, to_dir);
    float axis_len = length(axis);
    if (axis_len < 1e-6) {
        return from_dir;
    }
    axis /= axis_len;
    return from_dir * cos(max_angle) + cross(axis, from_dir) * sin(max_angle);
}

void grass_body_frame(body_state body, grass_params p, float lag, out float3 center, out float4 q) {
    float4 inv_field = quat_conjugate(p.field_orientation);
    center = rotate_vector(inv_field, body_origin(body.position.xyz - body.velocity.xyz * lag, body.orientation, body.com_local.xyz) - p.field_position);
    q = quat_mul(inv_field, body.orientation);
}

bool line_sphere_entry(float3 o, float3 d, float r, out float t) {
    float b = dot(o, d);
    float disc = b * b - (dot(o, o) - r * r);
    t = 0.0;
    if (disc < 0.0) {
        return false;
    }
    t = -b - sqrt(disc);
    return true;
}

bool shape_underside(body_state body, grass_params p, float2 xz, float lag, out float underside) {
    float3 center;
    float4 q;
    grass_body_frame(body, p, lag, center, q);
    float4 inv_q = quat_conjugate(q);
    float3 o = rotate_vector(inv_q, float3(xz.x, 0.0, xz.y) - center);
    float3 d = rotate_vector(inv_q, float3(0.0, 1.0, 0.0));
    underside = 0.0;

    if (body.shape_kind == shape_sphere) {
        return line_sphere_entry(o, d, body.shape_params.x, underside);
    }

    if (body.shape_kind == shape_capsule) {
        float r = body.shape_params.x;
        float hh = body.shape_params.y;
        bool hit = false;
        float best = 1e9;
        float t;
        if (line_sphere_entry(o - float3(0.0, hh, 0.0), d, r, t)) {
            best = t;
            hit = true;
        }
        if (line_sphere_entry(o + float3(0.0, hh, 0.0), d, r, t)) {
            best = min(best, t);
            hit = true;
        }
        float a = d.x * d.x + d.z * d.z;
        if (a > 1e-8) {
            float b = o.x * d.x + o.z * d.z;
            float disc = b * b - a * (o.x * o.x + o.z * o.z - r * r);
            if (disc >= 0.0) {
                t = (-b - sqrt(disc)) / a;
                if (abs(o.y + t * d.y) <= hh) {
                    best = min(best, t);
                    hit = true;
                }
            }
        }
        underside = best;
        return hit;
    }

    float3 he = body.half_extents.xyz;
    float t_lo = -1e9;
    float t_hi = 1e9;
    for (uint axis = 0; axis < 3; ++axis) {
        if (abs(d[axis]) < 1e-6) {
            if (abs(o[axis]) > he[axis]) {
                return false;
            }
            continue;
        }
        float t0 = (-he[axis] - o[axis]) / d[axis];
        float t1 = (he[axis] - o[axis]) / d[axis];
        t_lo = max(t_lo, min(t0, t1));
        t_hi = min(t_hi, max(t0, t1));
    }
    underside = t_lo;
    return t_lo <= t_hi;
}

float shape_distance(body_state body, grass_params p, float3 x, float lag, out float3 normal) {
    float3 center;
    float4 q;
    grass_body_frame(body, p, lag, center, q);
    float3 lp = rotate_vector(quat_conjugate(q), x - center);

    float3 ln = float3(0.0, 1.0, 0.0);
    float d = 0.0;

    if (body.shape_kind == shape_sphere) {
        float len = length(lp);
        d = len - body.shape_params.x;
        if (len > 1e-6) {
            ln = lp / len;
        }
    }
    else if (body.shape_kind == shape_capsule) {
        float hh = body.shape_params.y;
        float3 w = lp - float3(0.0, clamp(lp.y, -hh, hh), 0.0);
        float len = length(w);
        d = len - body.shape_params.x;
        if (len > 1e-6) {
            ln = w / len;
        }
    }
    else {
        float3 he = body.half_extents.xyz;
        float3 qd = abs(lp) - he;
        float3 outside = max(qd, float3(0.0, 0.0, 0.0));
        float out_len = length(outside);
        float3 s = float3(lp.x >= 0.0 ? 1.0 : -1.0, lp.y >= 0.0 ? 1.0 : -1.0, lp.z >= 0.0 ? 1.0 : -1.0);
        if (out_len > 0.0) {
            d = out_len;
            ln = s * outside / out_len;
        }
        else if (qd.x >= qd.y && qd.x >= qd.z) {
            d = qd.x;
            ln = float3(s.x, 0.0, 0.0);
        }
        else if (qd.y >= qd.z) {
            d = qd.y;
            ln = float3(0.0, s.y, 0.0);
        }
        else {
            d = qd.z;
            ln = float3(0.0, 0.0, s.z);
        }
    }

    normal = rotate_vector(q, ln);
    return d;
}

uint grass_root_cell(grass_params p, float3 root) {
    int n = int(p.cells_per_side);
    int cx = clamp(int(floor((root.x + p.half_extent) / p.cell_size)), 0, n - 1);
    int cz = clamp(int(floor((root.z + p.half_extent) / p.cell_size)), 0, n - 1);
    return uint(cz) * p.cells_per_side + uint(cx);
}
