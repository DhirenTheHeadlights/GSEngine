static const uint wrap_no_wrap = 0;
static const uint wrap_inside = 1;
static const uint wrap_wrapped = 2;
static const uint wrap_mandatory = 3;

struct wrap_obstacle {
    float3 center;
    float4 orientation;
    float radius;
    float half_length;
    float2 side;
    uint first_segment;
    uint segment_end;
};

struct wrap_tangents {
    uint result;
    float3 t1;
    float3 t2;
    float arc;
};

struct wrap_contact {
    uint segment;
    uint order;
    float3 t1;
    float3 t2;
    float arc;
};

struct wrap_contacts {
    wrap_contact c0;
    wrap_contact c1;
    float path_length;
};

struct wrap_node {
    float3 position;
    float3 lever;
    uint body;
    bool arc_follows;
};

interface IWrapPoses {
    float3 body_position(uint body);
    float4 body_orientation(uint body);
};

struct start_wrap_poses : IWrapPoses {
    float3 body_position(uint body) {
        return body_data[body].position.xyz;
    }

    float4 body_orientation(uint body) {
        return body_data[body].orientation;
    }
};

wrap_contact no_wrap_contact() {
    wrap_contact c;
    c.segment = no_wrap_segment;
    c.order = 0;
    c.t1 = float3(0, 0, 0);
    c.t2 = float3(0, 0, 0);
    c.arc = 0.0;
    return c;
}

wrap_contact contact_at(wrap_contacts contacts, uint w) {
    return w == 0 ? contacts.c0 : contacts.c1;
}

void set_contact(inout wrap_contacts contacts, uint w, wrap_contact c) {
    if (w == 0) {
        contacts.c0 = c;
    }
    else {
        contacts.c1 = c;
    }
}

float3 wrap_anchor<P : IWrapPoses>(P poses, muscle_constraint m) {
    muscle_path_point p = muscle_path_data[m.path_first];
    return poses.body_position(p.body) + rotate_vector(poses.body_orientation(p.body), p.local_point.xyz);
}

float3 wrap_path_point<P : IWrapPoses>(P poses, muscle_constraint m, float3 anchor, uint k) {
    muscle_path_point p = muscle_path_data[m.path_first + k];
    return (poses.body_position(p.body) - anchor) + rotate_vector(poses.body_orientation(p.body), p.local_point.xyz);
}

wrap_obstacle wrap_obstacle_of<P : IWrapPoses>(P poses, muscle_constraint m, float3 anchor, uint w) {
    muscle_path_point p = muscle_path_data[m.path_first + m.path_count + w];
    float4 q = poses.body_orientation(p.body);
    wrap_obstacle o;
    o.center = (poses.body_position(p.body) - anchor) + rotate_vector(q, p.local_point.xyz);
    o.orientation = quat_mul(q, curve_quat(p.frame_words));
    o.radius = curve_float(p.frame_words + wrap_radius_index);
    o.half_length = curve_float(p.frame_words + wrap_half_length_index);
    o.side = float2(curve_float(p.frame_words + wrap_side_index), curve_float(p.frame_words + wrap_side_index + 1));
    o.first_segment = curve_word(p.frame_words + wrap_first_segment_index);
    o.segment_end = curve_word(p.frame_words + wrap_segment_end_index);
    return o;
}

int wrap_side_of(wrap_obstacle o, float x, float y) {
    float s = x * o.side.x + y * o.side.y;
    return s > 0.0 ? 1 : (s < 0.0 ? -1 : 0);
}

wrap_tangents wrap_segment(float3 p1, float3 p2, wrap_obstacle o) {
    wrap_tangents none;
    none.result = wrap_no_wrap;
    none.t1 = float3(0, 0, 0);
    none.t2 = float3(0, 0, 0);
    none.arc = 0.0;
    float r = o.radius;
    float rho1 = sqrt(p1.x * p1.x + p1.y * p1.y);
    float rho2 = sqrt(p2.x * p2.x + p2.y * p2.y);
    if (rho1 < r || rho2 < r) {
        none.result = wrap_inside;
        return none;
    }

    float3 d = p2 - p1;
    float planar_squared = d.x * d.x + d.y * d.y;
    float t12 = planar_squared > 0.0 ? -(p1.x * d.x + p1.y * d.y) / planar_squared : 0.0;
    float near_x = p1.x + d.x * t12;
    float near_y = p1.y + d.y * t12;
    bool hit = near_x * near_x + near_y * near_y < r * r && t12 > 0.0 && t12 < 1.0;

    bool constrained = o.side.x != 0.0 || o.side.y != 0.0;
    uint result = wrap_wrapped;
    if (!constrained) {
        if (!hit) return none;
        result = wrap_mandatory;
    }

    float l1 = sqrt(max(rho1 * rho1 - r * r, 0.0));
    float l2 = sqrt(max(rho2 * rho2 - r * r, 0.0));
    float2 c0 = float2(p1.x * r - p1.y * l1, p1.y * r + p1.x * l1) / (rho1 * rho1);
    float2 c1 = float2(p1.x * r + p1.y * l1, p1.y * r - p1.x * l1) / (rho1 * rho1);
    float2 c2 = float2(p2.x * r - p2.y * l2, p2.y * r + p2.x * l2) / (rho2 * rho2);
    float2 c3 = float2(p2.x * r + p2.y * l2, p2.y * r - p2.x * l2) / (rho2 * rho2);
    float2 tangent1 = c0;
    float2 tangent2 = c3;
    bool long_wrap = false;
    if (constrained) {
        int s1 = wrap_side_of(o, p1.x, p1.y);
        int s2 = wrap_side_of(o, p2.x, p2.y);
        if (s1 == 1 || s2 == 1) {
            if (hit) {
                result = wrap_mandatory;
            }
            else if (s1 != s2 && wrap_side_of(o, near_x, near_y) != 1) {
                result = wrap_wrapped;
            }
            else {
                return none;
            }
        }
        bool on0 = wrap_side_of(o, r * c0.x, r * c0.y) == 1;
        bool on1 = wrap_side_of(o, r * c1.x, r * c1.y) == 1;
        bool on2 = wrap_side_of(o, r * c2.x, r * c2.y) == 1;
        bool on3 = wrap_side_of(o, r * c3.x, r * c3.y) == 1;
        bool short_pair = on0 && on1 ? !on2 : (on0 ? true : (on1 ? false : (on2 ? false : (on3 ? true : dot(c0, c3) < dot(c1, c2)))));
        tangent1 = short_pair ? c0 : c1;
        tangent2 = short_pair ? c3 : c2;
        float2 t1 = r * tangent1;
        float2 t2 = r * tangent2;
        long_wrap = (t1.x - p1.x + t2.x - p2.x) * (t1.x + t2.x) + (t1.y - p1.y + t2.y - p2.y) * (t1.y + t2.y) < 0.0;
    }
    else {
        float v0 = dot(c0, c2);
        float v1 = dot(c0, c3);
        float v2 = dot(c1, c2);
        float v3 = dot(c1, c3);
        uint best = 3;
        if (v0 > v1 && v0 > v2 && v0 > v3) {
            best = 0;
        }
        else if (v1 > v2 && v1 > v3) {
            best = 1;
        }
        else if (v2 > v3) {
            best = 2;
        }
        tangent1 = best < 2 ? c0 : c1;
        tangent2 = best % 2 == 0 ? c2 : c3;
    }

    float full_turn = 6.28318530717958647692;
    float theta = atan2(tangent1.x * tangent2.y - tangent1.y * tangent2.x, dot(tangent1, tangent2));
    float sweep = long_wrap ? full_turn - theta : abs(theta);
    float arc_planar = r * sweep;
    float planar = l1 + arc_planar + l2;
    float dz = p2.z - p1.z;
    float z1 = p1.z + dz * (l1 / planar);
    float z2 = p1.z + dz * ((l1 + arc_planar) / planar);
    float half_length = o.half_length;
    if ((z1 < -half_length || z1 > half_length) && (z2 < -half_length || z2 > half_length)) return none;
    wrap_tangents t;
    t.result = result;
    t.t1 = float3(r * tangent1, z1);
    t.t2 = float3(r * tangent2, z2);
    float rise = z2 - z1;
    t.arc = sqrt(arc_planar * arc_planar + rise * rise);
    return t;
}

uint ordered_wrap(muscle_constraint m, wrap_contacts contacts, uint rank) {
    bool swapped = m.wrap_count == 2 && contacts.c1.order < contacts.c0.order;
    return swapped ? 1 - rank : rank;
}

float wrapped_length_of<P : IWrapPoses>(P poses, muscle_constraint m, float3 anchor, wrap_contacts contacts) {
    float total = 0.0;
    float3 previous = wrap_path_point(poses, m, anchor, 0);
    for (uint k = 0; k + 1 < m.path_count; k++) {
        for (uint rank = 0; rank < m.wrap_count; rank++) {
            wrap_contact c = contact_at(contacts, ordered_wrap(m, contacts, rank));
            if (c.segment != k) continue;
            total += length(c.t1 - previous) + c.arc;
            previous = c.t2;
        }
        float3 next = wrap_path_point(poses, m, anchor, k + 1);
        total += length(next - previous);
        previous = next;
    }
    return total;
}

wrap_contacts wrap_muscle_path<P : IWrapPoses>(P poses, muscle_constraint m, float3 anchor) {
    wrap_contacts contacts;
    contacts.c0 = no_wrap_contact();
    contacts.c1 = no_wrap_contact();
    contacts.path_length = 0.0;
    uint count = m.wrap_count;
    uint order0 = 0;
    uint order1 = 1;
    uint result0 = wrap_no_wrap;
    uint result1 = wrap_no_wrap;
    uint passes = count < 2 ? 1 : wrap_passes;
    float last = 0.0;
    bool has_last = false;
    wrap_obstacle o0 = wrap_obstacle_of(poses, m, anchor, 0);
    wrap_obstacle o1 = count == 2 ? wrap_obstacle_of(poses, m, anchor, 1) : o0;
    for (uint pass = 0; pass < passes; pass++) {
        for (uint i = 0; i < count; i++) {
            uint a = i == 0 ? order0 : order1;
            uint b = 1 - a;
            wrap_obstacle o = a == 0 ? o0 : o1;
            float4 inverse = quat_conjugate(o.orientation);
            set_contact(contacts, a, no_wrap_contact());
            wrap_contact other = contact_at(contacts, b);
            uint result = wrap_no_wrap;
            bool found = false;
            wrap_contact best = no_wrap_contact();
            float best_change = 3.402823466e38;
            bool mandatory = false;
            for (uint k = o.first_segment; k < o.segment_end && !mandatory; k++) {
                bool shared = count == 2 && other.segment == k;
                uint pieces = shared ? 2 : 1;
                for (uint piece = 0; piece < pieces; piece++) {
                    float3 from = shared && piece == 1 ? other.t2 : wrap_path_point(poses, m, anchor, k);
                    float3 to = shared && piece == 0 ? other.t1 : wrap_path_point(poses, m, anchor, k + 1);
                    float3 p1 = rotate_vector(inverse, from - o.center);
                    float3 p2 = rotate_vector(inverse, to - o.center);
                    wrap_tangents t = wrap_segment(p1, p2, o);
                    result = t.result;
                    if (t.result != wrap_wrapped && t.result != wrap_mandatory) continue;
                    float change = length(t.t1 - p1) + t.arc + length(p2 - t.t2) - length(p2 - p1);
                    if (t.result == wrap_mandatory || change < best_change) {
                        best_change = change;
                        found = true;
                        best.segment = k;
                        best.order = shared ? piece : 0;
                        best.t1 = o.center + rotate_vector(o.orientation, t.t1);
                        best.t2 = o.center + rotate_vector(o.orientation, t.t2);
                        best.arc = t.arc;
                    }
                    if (t.result == wrap_mandatory) {
                        mandatory = true;
                        break;
                    }
                }
            }
            if (i == 0) {
                result0 = result;
            }
            else {
                result1 = result;
            }
            if (found) {
                set_contact(contacts, a, best);
                if (count == 2 && other.segment == best.segment) {
                    other.order = 1 - best.order;
                    set_contact(contacts, b, other);
                }
            }
        }
        float total = wrapped_length_of(poses, m, anchor, contacts);
        contacts.path_length = total;
        if (has_last && abs(total - last) < wrap_pass_tolerance) break;
        last = total;
        has_last = true;
        if (pass == 0 && count > 1 && result0 == wrap_no_wrap && result1 == wrap_inside) {
            order0 = 1;
            order1 = 0;
            contacts.c0 = no_wrap_contact();
        }
        else if (pass == 0 && count == 2 && (contacts.c1.segment < o0.first_segment || contacts.c1.segment >= o0.segment_end)) {
            break;
        }
    }
    return contacts;
}

wrap_node wrap_point_node<P : IWrapPoses>(P poses, muscle_constraint m, float3 anchor, uint k) {
    muscle_path_point p = muscle_path_data[m.path_first + k];
    wrap_node node;
    node.lever = rotate_vector(poses.body_orientation(p.body), p.local_point.xyz);
    node.position = (poses.body_position(p.body) - anchor) + node.lever;
    node.body = p.body;
    node.arc_follows = false;
    return node;
}

wrap_node wrap_arc_node<P : IWrapPoses>(P poses, muscle_constraint m, float3 anchor, wrap_contact c, uint w, bool arc_start) {
    muscle_path_point p = muscle_path_data[m.path_first + m.path_count + w];
    wrap_node node;
    node.position = arc_start ? c.t1 : c.t2;
    node.lever = node.position - (poses.body_position(p.body) - anchor);
    node.body = p.body;
    node.arc_follows = arc_start;
    return node;
}

struct wrapped_gradient {
    float3 linear;
    float3 angular;
};

void add_gradient_node(wrap_node current, wrap_node next, uint body, inout float3 g, inout wrapped_gradient total) {
    float3 d = next.position - current.position;
    bool joined = !current.arc_follows && length(d) > 1e-7;
    float3 direction = joined ? normalize(d) : float3(0, 0, 0);
    if (current.body == body) {
        if (joined) {
            g -= direction;
        }
        total.linear += g;
        total.angular += cross(current.lever, g);
    }
    g = direction;
}

wrapped_gradient wrapped_body_gradient<P : IWrapPoses>(P poses, muscle_constraint m, float3 anchor, wrap_contacts contacts, uint bi) {
    wrapped_gradient total;
    total.linear = float3(0, 0, 0);
    total.angular = float3(0, 0, 0);
    float3 g = float3(0, 0, 0);
    wrap_node current = wrap_point_node(poses, m, anchor, 0);
    for (uint k = 0; k < m.path_count; k++) {
        if (k > 0) {
            wrap_node node = wrap_point_node(poses, m, anchor, k);
            add_gradient_node(current, node, bi, g, total);
            current = node;
        }
        for (uint rank = 0; rank < m.wrap_count; rank++) {
            uint w = ordered_wrap(m, contacts, rank);
            wrap_contact c = contact_at(contacts, w);
            if (c.segment != k) {
                continue;
            }
            wrap_node arc_start = wrap_arc_node(poses, m, anchor, c, w, true);
            add_gradient_node(current, arc_start, bi, g, total);
            wrap_node arc_end = wrap_arc_node(poses, m, anchor, c, w, false);
            add_gradient_node(arc_start, arc_end, bi, g, total);
            current = arc_end;
        }
    }
    if (current.body == bi) {
        total.linear += g;
        total.angular += cross(current.lever, g);
    }
    return total;
}

float wrapped_start_length(muscle_constraint m) {
    start_wrap_poses poses;
    float3 anchor = wrap_anchor(poses, m);
    return wrap_muscle_path(poses, m, anchor).path_length;
}
