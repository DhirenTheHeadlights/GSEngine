static const uint FLAG_LOCKED = 1;
static const uint MAX_BODIES = 500;
static const uint MAX_CONTACTS = 2000;
static const uint MAX_COLLISION_PAIRS = 8192;
static const uint FEATURE_VERTEX = 0;
static const uint FEATURE_EDGE = 1;
static const uint FEATURE_FACE = 2;

struct push_constants {
    uint body_count;
    uint contact_count;
    uint motor_count;
    uint color_offset;
    uint color_count;
    float h_squared;
    float dt;
    float rho;
    float linear_damping;
    float velocity_sleep_threshold;
    uint substep;
    uint iteration;
};

struct GpuBody {
    float4 position;
    float4 predicted_position;
    float4 inertia_target;
    float4 old_position;
    float4 velocity;
    float4 predicted_velocity;
    float4 orientation;
    float4 predicted_orientation;
    float4 angular_inertia_target;
    float4 old_orientation;
    float4 angular_velocity;
    float4 predicted_angular_velocity;
    float4 motor_target;
    float mass;
    uint flags;
    uint sleep_counter;
    float _pad0;
    float4 inv_inertia_col0;
    float4 inv_inertia_col1;
    float4 inv_inertia_col2;
    float4 half_extents;
    float4 aabb_min;
    float4 aabb_max;
};

struct GpuContact {
    uint body_a;
    uint body_b;
    float initial_separation;
    float lambda;
    float4 normal;
    float4 r_a;
    float4 r_b;
    float friction_coeff;
    float mass_a;
    float mass_b;
    uint feature_packed;
};

struct GpuWarmStart {
    uint body_a;
    uint body_b;
    uint feature_packed;
    float lambda;
};

[[vk::binding(0, 0)]] RWStructuredBuffer<GpuBody> body_data;
[[vk::binding(1, 0)]] RWStructuredBuffer<GpuContact> contact_data;
[[vk::binding(2, 0)]] StructuredBuffer<uint> motor_data;
[[vk::binding(3, 0)]] StructuredBuffer<uint> color_data;
[[vk::binding(4, 0)]] RWStructuredBuffer<uint> body_contact_map;
[[vk::binding(5, 0)]] RWStructuredBuffer<float4> solve_state;
[[vk::binding(6, 0)]] RWStructuredBuffer<uint> collision_pairs;
[[vk::binding(7, 0)]] RWStructuredBuffer<uint> collision_state;
[[vk::binding(8, 0)]] StructuredBuffer<GpuWarmStart> warm_starts;

[[vk::push_constant]]
ConstantBuffer<push_constants> pc;

typedef float3x3 mat3;

float4 quat_conjugate(float4 q) {
    return float4(q.x, -q.y, -q.z, -q.w);
}

float3 rotate_vector(float4 q, float3 v) {
    float3 u = q.yzw;
    float s = q.x;
    return 2.0 * dot(u, v) * u + (s * s - dot(u, u)) * v + 2.0 * s * cross(u, v);
}

mat3 rotation_from_quat(float4 q) {
    float3 ax = rotate_vector(q, float3(1, 0, 0));
    float3 ay = rotate_vector(q, float3(0, 1, 0));
    float3 az = rotate_vector(q, float3(0, 0, 1));
    return mat3(ax, ay, az);
}

struct SatResult {
    bool valid;
    float3 normal;
    float separation;
    bool is_speculative;
};

SatResult sat_speculative(
    float3 center_a, float3 he_a, float3 axes_a[3],
    float3 center_b, float3 he_b, float3 axes_b[3],
    float speculative_margin
) {
    SatResult result;
    result.valid = false;
    result.is_speculative = false;

    float min_overlap = 1e30;
    float3 best_axis = float3(0, 0, 0);
    bool all_positive = true;

    float3 test_axes[15];
    for (int i = 0; i < 3; i++) test_axes[i] = axes_a[i];
    for (int i = 0; i < 3; i++) test_axes[3 + i] = axes_b[i];
    int ax_count = 6;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            test_axes[ax_count++] = cross(axes_a[i], axes_b[j]);
        }
    }

    for (int t = 0; t < 15; t++) {
        float3 axis = test_axes[t];
        float axis_len = length(axis);
        if (axis_len < 1e-6) continue;
        axis /= axis_len;

        float r1 = 0.0;
        float r2 = 0.0;
        for (int i = 0; i < 3; i++) {
            r1 += abs(dot(axis, axes_a[i]) * he_a[i]);
            r2 += abs(dot(axis, axes_b[i]) * he_b[i]);
        }

        float dist = abs(dot(axis, center_a - center_b));
        float overlap = r1 + r2 - dist;

        if (overlap < -speculative_margin) return result;

        if (overlap <= 0.0) all_positive = false;

        if (overlap < min_overlap) {
            min_overlap = overlap;
            best_axis = axis;
        }
    }

    if (dot(best_axis, center_b - center_a) < 0.0) {
        best_axis = -best_axis;
    }

    result.valid = true;
    result.normal = best_axis;
    result.separation = min_overlap;
    result.is_speculative = !all_positive;
    return result;
}

struct FaceInfo {
    float3 vertices[4];
    float3 normal;
    float max_dot;
};

void get_face_vertices(float3 center, float3 he, float3 axes[3], int face_idx, out float3 verts[4]) {
    int axis_idx = face_idx / 2;
    float sign = (face_idx % 2 == 0) ? 1.0 : -1.0;

    int u_idx = (axis_idx + 1) % 3;
    int v_idx = (axis_idx + 2) % 3;

    float3 face_center = center + axes[axis_idx] * (he[axis_idx] * sign);
    float h_u = he[u_idx];
    float h_v = he[v_idx];

    verts[0] = face_center + axes[u_idx] * h_u + axes[v_idx] * h_v;
    verts[1] = face_center - axes[u_idx] * h_u + axes[v_idx] * h_v;
    verts[2] = face_center - axes[u_idx] * h_u - axes[v_idx] * h_v;
    verts[3] = face_center + axes[u_idx] * h_u - axes[v_idx] * h_v;
}

FaceInfo find_best_face(float3 center, float3 he, float3 axes[3], float3 dir) {
    FaceInfo info;
    info.max_dot = -1e30;

    float3 face_normals[6];
    face_normals[0] = axes[0];
    face_normals[1] = -axes[0];
    face_normals[2] = axes[1];
    face_normals[3] = -axes[1];
    face_normals[4] = axes[2];
    face_normals[5] = -axes[2];

    int best_face = 0;
    for (int i = 0; i < 6; i++) {
        float d = dot(face_normals[i], dir);
        if (d > info.max_dot) {
            info.max_dot = d;
            best_face = i;
        }
    }

    info.normal = face_normals[best_face];
    get_face_vertices(center, he, axes, best_face, info.vertices);
    return info;
}

static const int MAX_CLIP_VERTS = 8;

struct ClipPoly {
    float3 verts[MAX_CLIP_VERTS];
    int count;
};

ClipPoly clip_polygon(ClipPoly subject, float3 plane_normal, float plane_dist) {
    ClipPoly out_poly;
    out_poly.count = 0;

    if (subject.count == 0) return out_poly;

    float3 prev = subject.verts[subject.count - 1];
    float prev_dist = dot(plane_normal, prev) - plane_dist;
    bool prev_inside = prev_dist >= -1e-4;

    for (int i = 0; i < subject.count; i++) {
        float3 curr = subject.verts[i];
        float curr_dist = dot(plane_normal, curr) - plane_dist;
        bool curr_inside = curr_dist >= -1e-4;

        if (prev_inside != curr_inside) {
            float denom = prev_dist - curr_dist;
            if (abs(denom) > 1e-6 && out_poly.count < MAX_CLIP_VERTS) {
                float t = prev_dist / denom;
                out_poly.verts[out_poly.count++] = prev + (curr - prev) * t;
            }
        }

        if (curr_inside && out_poly.count < MAX_CLIP_VERTS) {
            out_poly.verts[out_poly.count++] = curr;
        }

        prev = curr;
        prev_dist = curr_dist;
        prev_inside = curr_inside;
    }

    return out_poly;
}

struct ContactResult {
    float3 points[4];
    int count;
    float3 tangent_u;
    float3 tangent_v;
};

ContactResult generate_contacts(
    float3 center_a, float3 he_a, float3 axes_a[3],
    float3 center_b, float3 he_b, float3 axes_b[3],
    float3 normal
) {
    ContactResult result;
    result.count = 0;

    float3 n = normal;
    if (dot(n, center_b - center_a) < 0.0) n = -n;

    FaceInfo info1 = find_best_face(center_a, he_a, axes_a, n);
    FaceInfo info2 = find_best_face(center_b, he_b, axes_b, -n);

    if (info1.max_dot < 0.95 && info2.max_dot < 0.95) {
        float3 sup_a = center_a;
        float3 sup_b = center_b;
        for (int i = 0; i < 3; i++) {
            float da = dot(n, axes_a[i]);
            if (da > 1e-6) sup_a += axes_a[i] * he_a[i];
            else if (da < -1e-6) sup_a -= axes_a[i] * he_a[i];

            float db = dot(-n, axes_b[i]);
            if (db > 1e-6) sup_b += axes_b[i] * he_b[i];
            else if (db < -1e-6) sup_b -= axes_b[i] * he_b[i];
        }
        result.points[0] = (sup_a + sup_b) * 0.5;
        result.count = 1;

        float3 tu;
        if (abs(n.x) < 0.9) tu = cross(n, float3(1, 0, 0));
        else tu = cross(n, float3(0, 1, 0));
        result.tangent_u = normalize(tu);
        result.tangent_v = cross(n, result.tangent_u);
        return result;
    }

    float3 ref_face[4];
    float3 ref_normal;
    ClipPoly inc_poly;
    inc_poly.count = 4;

    if (info1.max_dot > info2.max_dot) {
        for (int i = 0; i < 4; i++) ref_face[i] = info1.vertices[i];
        for (int i = 0; i < 4; i++) inc_poly.verts[i] = info2.vertices[i];
        ref_normal = info1.normal;
    } else {
        for (int i = 0; i < 4; i++) ref_face[i] = info2.vertices[i];
        for (int i = 0; i < 4; i++) inc_poly.verts[i] = info1.vertices[i];
        ref_normal = info2.normal;
    }

    ref_normal = normalize(ref_normal);

    float3 ref_center = (ref_face[0] + ref_face[1] + ref_face[2] + ref_face[3]) * 0.25;

    for (int i = 0; i < 4; i++) {
        if (inc_poly.count == 0) break;

        float3 v1 = ref_face[i];
        float3 v2 = ref_face[(i + 1) % 4];

        float3 edge_vec = v2 - v1;
        if (length(edge_vec) < 1e-6) continue;
        float3 edge = normalize(edge_vec);
        float3 plane_normal = cross(ref_normal, edge);
        float pn_len = length(plane_normal);
        if (pn_len < 1e-6) continue;
        plane_normal /= pn_len;

        if (dot(plane_normal, ref_center - v1) < 0.0) {
            plane_normal = -plane_normal;
        }

        float plane_dist = dot(plane_normal, v1);
        inc_poly = clip_polygon(inc_poly, plane_normal, plane_dist);
    }

    float ref_plane_dist = dot(ref_normal, ref_face[0]);

    for (int i = 0; i < inc_poly.count && result.count < 4; i++) {
        float dist = dot(ref_normal, inc_poly.verts[i]) - ref_plane_dist;
        if (dist <= 1e-3) {
            result.points[result.count++] = inc_poly.verts[i] - ref_normal * dist;
        }
    }

    float3 tu;
    if (abs(n.x) < 0.9) tu = cross(n, float3(1, 0, 0));
    else tu = cross(n, float3(0, 1, 0));
    result.tangent_u = normalize(tu);
    result.tangent_v = cross(n, result.tangent_u);

    return result;
}

uint identify_feature(float3 center, float3 he, float3 axes[3], float3 contact_pt, float3 normal) {
    float3 face_normals[6];
    face_normals[0] = axes[0];
    face_normals[1] = -axes[0];
    face_normals[2] = axes[1];
    face_normals[3] = -axes[1];
    face_normals[4] = axes[2];
    face_normals[5] = -axes[2];

    for (uint i = 0; i < 6; i++) {
        if (abs(dot(normal, face_normals[i])) > 0.98) {
            return (FEATURE_FACE << 8) | i;
        }
    }

    float3 corners[8];
    for (int i = 0; i < 8; i++) {
        float sx = (i & 1) != 0 ? 1.0 : -1.0;
        float sy = (i & 2) != 0 ? 1.0 : -1.0;
        float sz = (i & 4) != 0 ? 1.0 : -1.0;
        corners[i] = center + axes[0] * he.x * sx + axes[1] * he.y * sy + axes[2] * he.z * sz;
    }

    float min_dist = 1e30;
    uint closest_vertex = 0;
    for (uint i = 0; i < 8; i++) {
        float d = length(contact_pt - corners[i]);
        if (d < min_dist) {
            min_dist = d;
            closest_vertex = i;
        }
    }

    if (min_dist < 0.01) {
        return (FEATURE_VERTEX << 8) | closest_vertex;
    }

    static const uint2 edge_indices[12] = {
        uint2(0, 1), uint2(2, 3), uint2(4, 5), uint2(6, 7),
        uint2(0, 2), uint2(1, 3), uint2(4, 6), uint2(5, 7),
        uint2(0, 4), uint2(1, 5), uint2(2, 6), uint2(3, 7)
    };

    float min_edge_dist = 1e30;
    uint closest_edge = 0;

    for (uint i = 0; i < 12; i++) {
        float3 p0 = corners[edge_indices[i].x];
        float3 p1 = corners[edge_indices[i].y];
        float3 edge = p1 - p0;
        float edge_len_sq = dot(edge, edge);
        if (edge_len_sq < 1e-8) continue;

        float t = clamp(dot(contact_pt - p0, edge) / edge_len_sq, 0.0, 1.0);
        float3 closest = p0 + edge * t;
        float d = length(contact_pt - closest);

        if (d < min_edge_dist) {
            min_edge_dist = d;
            closest_edge = i;
        }
    }

    return (FEATURE_EDGE << 8) | closest_edge;
}

float warm_start_lookup(uint ba, uint bb, uint feature_packed, uint ws_count) {
    if (ws_count == 0) return 0.0;

    uint lo = 0;
    uint hi = ws_count;
    while (lo < hi) {
        uint mid = (lo + hi) / 2;
        GpuWarmStart ws = warm_starts[mid];
        if (ws.body_a < ba || (ws.body_a == ba && ws.body_b < bb) ||
            (ws.body_a == ba && ws.body_b == bb && ws.feature_packed < feature_packed)) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    if (lo < ws_count) {
        GpuWarmStart ws = warm_starts[lo];
        if (ws.body_a == ba && ws.body_b == bb && ws.feature_packed == feature_packed) {
            return ws.lambda * 0.3;
        }
    }

    return 0.0;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint pair_idx = dispatch_id.x;

    uint pair_count = min(collision_pairs[0], MAX_COLLISION_PAIRS);
    if (pair_idx >= pair_count) return;

    uint body_a_idx = collision_pairs[1 + pair_idx * 2];
    uint body_b_idx = collision_pairs[1 + pair_idx * 2 + 1];

    GpuBody ba = body_data[body_a_idx];
    GpuBody bb = body_data[body_b_idx];

    float3 center_a = ba.position.xyz;
    float3 center_b = bb.position.xyz;
    float3 he_a = ba.half_extents.xyz;
    float3 he_b = bb.half_extents.xyz;

    mat3 rot_a = rotation_from_quat(ba.orientation);
    mat3 rot_b = rotation_from_quat(bb.orientation);

    float3 axes_a[3] = { rot_a[0], rot_a[1], rot_a[2] };
    float3 axes_b[3] = { rot_b[0], rot_b[1], rot_b[2] };

    float speculative_margin = asfloat(collision_state[1]);
    float friction_coeff = asfloat(collision_state[2]);
    uint ws_count = collision_state[5];

    SatResult sat = sat_speculative(center_a, he_a, axes_a, center_b, he_b, axes_b, speculative_margin);
    if (!sat.valid) return;

    ContactResult contacts = generate_contacts(center_a, he_a, axes_a, center_b, he_b, axes_b, sat.normal);
    if (contacts.count == 0) {
        float3 sup_a = center_a;
        float3 sup_b = center_b;
        for (int i = 0; i < 3; i++) {
            float da = dot(sat.normal, axes_a[i]);
            if (da > 1e-6) sup_a += axes_a[i] * he_a[i];
            else if (da < -1e-6) sup_a -= axes_a[i] * he_a[i];

            float db = dot(-sat.normal, axes_b[i]);
            if (db > 1e-6) sup_b += axes_b[i] * he_b[i];
            else if (db < -1e-6) sup_b -= axes_b[i] * he_b[i];
        }
        contacts.points[0] = (sup_a + sup_b) * 0.5;
        contacts.count = 1;
    }

    float mass_a = (ba.flags & FLAG_LOCKED) != 0 ? 0.0 : ba.mass;
    float mass_b = (bb.flags & FLAG_LOCKED) != 0 ? 0.0 : bb.mass;

    for (int p = 0; p < contacts.count; p++) {
        uint ci;
        InterlockedAdd(collision_state[0], 1, ci);
        if (ci >= MAX_CONTACTS) return;

        float3 cp = contacts.points[p];

        float3 world_r_a = cp - center_a;
        float3 world_r_b = cp - center_b;

        float3 local_r_a = rotate_vector(quat_conjugate(ba.orientation), world_r_a);
        float3 local_r_b = rotate_vector(quat_conjugate(bb.orientation), world_r_b);

        uint feat_a = identify_feature(center_a, he_a, axes_a, cp, sat.normal);
        uint feat_b = identify_feature(center_b, he_b, axes_b, cp, -sat.normal);
        uint feature_packed = (feat_a << 16) | feat_b;

        float warm_lambda = warm_start_lookup(body_a_idx, body_b_idx, feature_packed, ws_count);

        GpuContact c;
        c.body_a = body_a_idx;
        c.body_b = body_b_idx;
        c.initial_separation = -sat.separation;
        c.lambda = warm_lambda;
        c.normal = float4(sat.normal, 0);
        c.r_a = float4(local_r_a, 0);
        c.r_b = float4(local_r_b, 0);
        c.friction_coeff = friction_coeff;
        c.mass_a = mass_a;
        c.mass_b = mass_b;
        c.feature_packed = feature_packed;

        contact_data[ci] = c;
    }
}
