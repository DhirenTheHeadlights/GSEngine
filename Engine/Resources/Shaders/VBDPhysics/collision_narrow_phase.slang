import common;
import vbd_shared;
import vbd_physics;

[[vk::push_constant]]
ConstantBuffer<vbd_push_constants> pc;

static const uint COLLISION_STATE_HEADER_UINTS = 8;
static const uint NARROW_PHASE_DEBUG_RECORD_UINTS = 8;
static const uint MAX_NARROW_PHASE_DEBUG_RECORDS = 32;
static const uint FEATURE_VERTEX = 0;
static const uint FEATURE_EDGE = 1;
static const uint FEATURE_FACE = 2;
static const uint FEATURE_SIDE_NONE = 0xFF;
static const float SAT_AXIS_RELATIVE_TOLERANCE = 0.95;
static const float SAT_AXIS_ABSOLUTE_TOLERANCE = 0.01;
static const uint SAT_AXIS_FACE = 0;
static const uint SAT_AXIS_CROSS = 1;

float contact_effective_mass(
    GpuBody body_a,
    GpuBody body_b,
    float3 rAW,
    float3 rBW,
    float3 dir
) {
    float inv_mass_sum = 0.0;

    if ((body_a.flags & FLAG_LOCKED) == 0 && body_a.mass > 1e-10) {
        inv_mass_sum += 1.0 / body_a.mass;
    }
    if ((body_b.flags & FLAG_LOCKED) == 0 && body_b.mass > 1e-10) {
        inv_mass_sum += 1.0 / body_b.mass;
    }

    if ((body_a.flags & FLAG_UPDATE_ORIENTATION) != 0 && (body_a.flags & FLAG_LOCKED) == 0) {
        mat3 inv_i_a = mat3_inverse(body_a.inertia);
        float3 ang_j_a = cross(rAW, dir);
        inv_mass_sum += dot(cross(mat3_mul_vec(inv_i_a, ang_j_a), rAW), dir);
    }

    if ((body_b.flags & FLAG_UPDATE_ORIENTATION) != 0 && (body_b.flags & FLAG_LOCKED) == 0) {
        mat3 inv_i_b = mat3_inverse(body_b.inertia);
        float3 ang_j_b = cross(rBW, dir);
        inv_mass_sum += dot(cross(mat3_mul_vec(inv_i_b, ang_j_b), rBW), dir);
    }

    if (isinf(inv_mass_sum) || isnan(inv_mass_sum) || inv_mass_sum <= 1e-10) {
        return 0.0;
    }

    return 1.0 / inv_mass_sum;
}

struct SatResult {
    bool valid;
    float3 normal;
    float separation;
    bool is_speculative;
};

struct SatAxisChoice {
    float3 axis;
    float overlap;
    float extent_scale;
    uint source;
    bool valid;
};

float projected_radius(float3 axis, float3 he, float3 axes[3]) {
    float radius = 0.0;
    for (int i = 0; i < 3; i++) {
        radius += abs(dot(axis, axes[i]) * he[i]);
    }
    return radius;
}

bool should_replace_sat_choice(SatAxisChoice best, float overlap, float extent_scale) {
    if (!best.valid) {
        return true;
    }

    if (overlap >= best.overlap) {
        return false;
    }

    float replace_threshold =
        best.overlap * SAT_AXIS_RELATIVE_TOLERANCE -
        SAT_AXIS_ABSOLUTE_TOLERANCE * max(extent_scale, 1e-3);

    return overlap < replace_threshold;
}

void update_sat_choice(inout SatAxisChoice best, float3 axis, float overlap, uint source, float extent_scale) {
    if (should_replace_sat_choice(best, overlap, extent_scale)) {
        best.axis = axis;
        best.overlap = overlap;
        best.extent_scale = extent_scale;
        best.source = source;
        best.valid = true;
    }
}

SatAxisChoice prefer_face_sat_axis(SatAxisChoice best, SatAxisChoice best_face) {
    if (!best.valid || best.source != SAT_AXIS_CROSS || !best_face.valid) {
        return best;
    }

    float cross_replace_threshold =
        best_face.overlap * SAT_AXIS_RELATIVE_TOLERANCE -
        SAT_AXIS_ABSOLUTE_TOLERANCE * max(best_face.extent_scale, 1e-3);

    if (best.overlap >= cross_replace_threshold) {
        return best_face;
    }

    return best;
}

SatResult sat_speculative(
    float3 center_a, float3 he_a, float3 axes_a[3],
    float3 center_b, float3 he_b, float3 axes_b[3],
    float speculative_margin
) {
    SatResult result;
    result.valid = false;
    result.is_speculative = false;
    result.normal = float3(0, 0, 0);
    result.separation = 0.0;

    SatAxisChoice best_axis;
    best_axis.axis = float3(0, 0, 0);
    best_axis.overlap = 0.0;
    best_axis.extent_scale = 0.0;
    best_axis.source = SAT_AXIS_FACE;
    best_axis.valid = false;

    SatAxisChoice best_face_axis = best_axis;
    bool all_positive = true;

    for (int i = 0; i < 3; i++) {
        float3 axis = axes_a[i];
        float axis_len = length(axis);
        if (axis_len >= 1e-6) {
            axis /= axis_len;

            float r1 = projected_radius(axis, he_a, axes_a);
            float r2 = projected_radius(axis, he_b, axes_b);
            float dist = abs(dot(axis, center_a - center_b));
            float overlap = r1 + r2 - dist;

            if (overlap < -speculative_margin) return result;
            if (overlap <= 0.0) all_positive = false;

            update_sat_choice(best_axis, axis, overlap, SAT_AXIS_FACE, he_a[i]);
            update_sat_choice(best_face_axis, axis, overlap, SAT_AXIS_FACE, he_a[i]);
        }
    }

    for (int i = 0; i < 3; i++) {
        float3 axis = axes_b[i];
        float axis_len = length(axis);
        if (axis_len >= 1e-6) {
            axis /= axis_len;

            float r1 = projected_radius(axis, he_a, axes_a);
            float r2 = projected_radius(axis, he_b, axes_b);
            float dist = abs(dot(axis, center_a - center_b));
            float overlap = r1 + r2 - dist;

            if (overlap < -speculative_margin) return result;
            if (overlap <= 0.0) all_positive = false;

            update_sat_choice(best_axis, axis, overlap, SAT_AXIS_FACE, he_b[i]);
            update_sat_choice(best_face_axis, axis, overlap, SAT_AXIS_FACE, he_b[i]);
        }
    }

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            float3 axis = cross(axes_a[i], axes_b[j]);
            float axis_len = length(axis);
            if (axis_len < 1e-6) continue;
            axis /= axis_len;

            float r1 = projected_radius(axis, he_a, axes_a);
            float r2 = projected_radius(axis, he_b, axes_b);
            float dist = abs(dot(axis, center_a - center_b));
            float overlap = r1 + r2 - dist;

            if (overlap < -speculative_margin) return result;
            if (overlap <= 0.0) all_positive = false;

            update_sat_choice(best_axis, axis, overlap, SAT_AXIS_CROSS, 0.0);
        }
    }

    best_axis = prefer_face_sat_axis(best_axis, best_face_axis);
    if (!best_axis.valid) {
        return result;
    }

    if (dot(best_axis.axis, center_b - center_a) < 0.0) {
        best_axis.axis = -best_axis.axis;
    }

    result.valid = true;
    result.normal = best_axis.axis;
    result.separation = best_axis.overlap;
    result.is_speculative = !all_positive;
    return result;
}

struct FaceInfo {
    uint face_index;
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
    info.face_index = 0;
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

    info.face_index = best_face;
    info.normal = face_normals[best_face];
    get_face_vertices(center, he, axes, best_face, info.vertices);
    return info;
}

static const int MAX_CLIP_VERTS = 9;

struct ActiveSides {
    uint values[2];
    uint count;
};

struct ClipVertexData {
    float3 point;
    ActiveSides reference_sides;
    ActiveSides incident_sides;
};

struct ClipVertexPoly {
    ClipVertexData verts[MAX_CLIP_VERTS];
    int count;
};

struct ClippedFaceContacts {
    ClipVertexPoly vertices;
    bool reference_is_a;
    uint reference_face;
    uint incident_face;
    bool valid;
};

struct PlaneData {
    float3 normal;
    float distance;
};

struct FeatureDetail {
    uint type;
    uint face;
    uint side0;
    uint side1;
};

FeatureDetail classify_feature(float3 center, float3 he, float3 axes[3], float3 support_dir, float3 world_point) {
    float3 face_normals[6];
    face_normals[0] = axes[0];
    face_normals[1] = -axes[0];
    face_normals[2] = axes[1];
    face_normals[3] = -axes[1];
    face_normals[4] = axes[2];
    face_normals[5] = -axes[2];

    uint face_index = 0;
    float best_dot = -1e30;
    for (uint i = 0; i < 6; i++) {
        float score = dot(face_normals[i], support_dir);
        if (score > best_dot) {
            best_dot = score;
            face_index = i;
        }
    }

    float3 local = float3(
        dot(world_point - center, axes[0]),
        dot(world_point - center, axes[1]),
        dot(world_point - center, axes[2])
    );

    uint axis_idx = face_index / 2;
    uint u_axis = (axis_idx + 1) % 3;
    uint v_axis = (axis_idx + 2) % 3;
    float tangent_scale = max(he[u_axis], he[v_axis]);
    float boundary_threshold = clamp(tangent_scale * 0.002, 0.0005, 0.01);

    FeatureDetail result;
    result.type = FEATURE_FACE;
    result.face = face_index;
    result.side0 = FEATURE_SIDE_NONE;
    result.side1 = FEATURE_SIDE_NONE;

    float half_extent_u = he[u_axis];
    float dist_u = half_extent_u - clamp(abs(local[u_axis]), 0.0, half_extent_u);
    if (dist_u <= boundary_threshold) {
        result.side0 = local[u_axis] >= 0.0 ? 0u : 1u;
    }

    float half_extent_v = he[v_axis];
    float dist_v = half_extent_v - clamp(abs(local[v_axis]), 0.0, half_extent_v);
    if (dist_v <= boundary_threshold) {
        uint side = local[v_axis] >= 0.0 ? 2u : 3u;
        if (result.side0 == FEATURE_SIDE_NONE) {
            result.side0 = side;
        } else if (result.side0 != side) {
            result.side1 = side;
        }
    }

    result.type =
        result.side0 == FEATURE_SIDE_NONE
            ? FEATURE_FACE
            : (result.side1 == FEATURE_SIDE_NONE ? FEATURE_EDGE : FEATURE_VERTEX);

    if (result.side1 != FEATURE_SIDE_NONE && result.side1 < result.side0) {
        uint tmp = result.side0;
        result.side0 = result.side1;
        result.side1 = tmp;
    }

    return result;
}

void init_active_sides(out ActiveSides sides) {
    sides.values[0] = FEATURE_SIDE_NONE;
    sides.values[1] = FEATURE_SIDE_NONE;
    sides.count = 0;
}

void add_side(inout ActiveSides sides, uint side) {
    if (side == FEATURE_SIDE_NONE) return;

    for (uint i = 0; i < sides.count; i++) {
        if (sides.values[i] == side) return;
    }

    if (sides.count >= 2) return;
    sides.values[sides.count++] = side;

    if (sides.count == 2 && sides.values[1] < sides.values[0]) {
        uint tmp = sides.values[0];
        sides.values[0] = sides.values[1];
        sides.values[1] = tmp;
    }
}

ActiveSides shared_sides(ActiveSides lhs, ActiveSides rhs) {
    ActiveSides shared;
    init_active_sides(shared);
    for (uint i = 0; i < lhs.count; i++) {
        for (uint j = 0; j < rhs.count; j++) {
            if (lhs.values[i] == rhs.values[j]) {
                add_side(shared, lhs.values[i]);
            }
        }
    }
    return shared;
}

ActiveSides vertex_side_set(uint ordinal) {
    ActiveSides sides;
    init_active_sides(sides);
    switch (ordinal % 4) {
    case 0:
        add_side(sides, 0);
        add_side(sides, 2);
        break;
    case 1:
        add_side(sides, 1);
        add_side(sides, 2);
        break;
    case 2:
        add_side(sides, 1);
        add_side(sides, 3);
        break;
    default:
        add_side(sides, 0);
        add_side(sides, 3);
        break;
    }
    return sides;
}

uint feature_type_from_sides(ActiveSides sides) {
    return sides.count == 0 ? FEATURE_FACE : (sides.count == 1 ? FEATURE_EDGE : FEATURE_VERTEX);
}

uint reference_edge_side_id(uint edge_index) {
    static const uint side_ids[4] = { 2, 1, 3, 0 };
    return side_ids[edge_index % 4];
}

ClipVertexPoly clip_feature_polygon(
    ClipVertexPoly subject,
    PlaneData p,
    bool keep_greater,
    bool tag_reference_side,
    uint reference_side
) {
    ClipVertexPoly out_poly;
    out_poly.count = 0;
    if (subject.count == 0) return out_poly;

    float clip_epsilon = 1e-4;
    ClipVertexData prev = subject.verts[subject.count - 1];
    float prev_dist = dot(p.normal, prev.point) - p.distance;
    bool prev_inside = keep_greater ? prev_dist >= -clip_epsilon : prev_dist <= clip_epsilon;

    for (int i = 0; i < subject.count; i++) {
        ClipVertexData curr = subject.verts[i];
        float curr_dist = dot(p.normal, curr.point) - p.distance;
        bool curr_inside = keep_greater ? curr_dist >= -clip_epsilon : curr_dist <= clip_epsilon;

        if (prev_inside != curr_inside) {
            float denom = prev_dist - curr_dist;
            if (abs(denom) > 1e-6 && out_poly.count < MAX_CLIP_VERTS) {
                float t = prev_dist / denom;
                ClipVertexData intersection;
                intersection.point = prev.point + (curr.point - prev.point) * t;
                intersection.reference_sides = shared_sides(prev.reference_sides, curr.reference_sides);
                intersection.incident_sides = shared_sides(prev.incident_sides, curr.incident_sides);
                if (tag_reference_side) {
                    add_side(intersection.reference_sides, reference_side);
                }
                out_poly.verts[out_poly.count++] = intersection;
            }
        }

        if (curr_inside && out_poly.count < MAX_CLIP_VERTS) {
            if (tag_reference_side && abs(curr_dist) <= clip_epsilon) {
                add_side(curr.reference_sides, reference_side);
            }
            out_poly.verts[out_poly.count++] = curr;
        }

        prev = curr;
        prev_dist = curr_dist;
        prev_inside = curr_inside;
    }

    return out_poly;
}

ClippedFaceContacts build_clipped_face_contacts(
    float3 center_a, float3 he_a, float3 axes_a[3],
    float3 center_b, float3 he_b, float3 axes_b[3],
    float3 collision_normal
) {
    ClippedFaceContacts result;
    result.vertices.count = 0;
    result.reference_is_a = true;
    result.reference_face = 0;
    result.incident_face = 0;
    result.valid = false;

    float3 n = normalize(collision_normal);
    if (dot(n, center_b - center_a) < 0.0) {
        n = -n;
    }

    FaceInfo info1 = find_best_face(center_a, he_a, axes_a, n);
    FaceInfo info2 = find_best_face(center_b, he_b, axes_b, -n);

    if (max(info1.max_dot, info2.max_dot) < 0.85) {
        return result;
    }

    bool reference_is_a = info1.max_dot >= info2.max_dot - 1e-4;
    FaceInfo reference_info = reference_is_a ? info1 : info2;
    FaceInfo incident_info = reference_is_a ? info2 : info1;

    ClipVertexPoly polygon;
    polygon.count = 4;
    for (uint i = 0; i < 4; i++) {
        polygon.verts[i].point = incident_info.vertices[i];
        init_active_sides(polygon.verts[i].reference_sides);
        polygon.verts[i].incident_sides = vertex_side_set(i);
    }

    float3 reference_normal = normalize(reference_info.normal);
    float3 reference_center = (reference_info.vertices[0] + reference_info.vertices[1] + reference_info.vertices[2] + reference_info.vertices[3]) * 0.25;

    for (uint i = 0; i < 4; i++) {
        if (polygon.count == 0) break;

        float3 v1 = reference_info.vertices[i];
        float3 v2 = reference_info.vertices[(i + 1) % 4];
        float3 edge_vec = v2 - v1;
        if (length(edge_vec) < 1e-6) continue;

        float3 edge = normalize(edge_vec);
        float3 plane_normal = cross(reference_normal, edge);
        float plane_length = length(plane_normal);
        if (plane_length < 1e-6) continue;
        plane_normal /= plane_length;

        if (dot(plane_normal, reference_center - v1) < 0.0) {
            plane_normal = -plane_normal;
        }

        PlaneData plane_data;
        plane_data.normal = plane_normal;
        plane_data.distance = dot(plane_normal, v1);
        polygon = clip_feature_polygon(polygon, plane_data, true, true, reference_edge_side_id(i));
    }

    result.valid = true;
    result.reference_is_a = reference_is_a;
    result.reference_face = reference_info.face_index;
    result.incident_face = incident_info.face_index;

    if (polygon.count == 0) {
        return result;
    }

    PlaneData reference_plane;
    reference_plane.normal = reference_normal;
    reference_plane.distance = dot(reference_normal, reference_info.vertices[0]);

    polygon = clip_feature_polygon(
        polygon,
        reference_plane,
        false,
        false,
        FEATURE_SIDE_NONE
    );

    float face_alignment = dot(reference_normal, -normalize(incident_info.normal));
    if (polygon.count > 0 && face_alignment < 0.985) {
        float max_incident_span = 0.0;
        for (uint i = 0; i < 4; i++) {
            float edge_length = length(incident_info.vertices[(i + 1) % 4] - incident_info.vertices[i]);
            max_incident_span = max(max_incident_span, edge_length);
        }

        float contact_band = clamp(max_incident_span * 0.1, 0.01, 0.1);
        float closest_plane_distance = -1e30;
        for (int i = 0; i < polygon.count; i++) {
            closest_plane_distance = max(
                closest_plane_distance,
                dot(reference_normal, polygon.verts[i].point) - reference_plane.distance
            );
        }

        ClipVertexPoly filtered;
        filtered.count = 0;
        for (int i = 0; i < polygon.count && filtered.count < MAX_CLIP_VERTS; i++) {
            float dist = dot(reference_normal, polygon.verts[i].point) - reference_plane.distance;
            if (closest_plane_distance - dist <= contact_band) {
                filtered.verts[filtered.count++] = polygon.verts[i];
            }
        }

        if (filtered.count > 0) {
            polygon = filtered;
        }
    }

    for (int i = 0; i < polygon.count; i++) {
        float dist = dot(reference_normal, polygon.verts[i].point) - reference_plane.distance;
        polygon.verts[i].point -= reference_normal * dist;
    }

    result.vertices = polygon;
    return result;
}

uint pack_feature_component(uint type, uint face, uint side0, uint side1) {
    return (type << 24) | (face << 16) | (side0 << 8) | side1;
}

bool feature_less(uint lhs_hi, uint lhs_lo, uint rhs_hi, uint rhs_lo) {
    return lhs_hi < rhs_hi || (lhs_hi == rhs_hi && lhs_lo < rhs_lo);
}

bool feature_equal(uint lhs_hi, uint lhs_lo, uint rhs_hi, uint rhs_lo) {
    return lhs_hi == rhs_hi && lhs_lo == rhs_lo;
}

struct ContactCandidate {
    float3 position_on_a;
    float3 position_on_b;
    uint feature_hi;
    uint feature_lo;
};

struct ContactResult {
    ContactCandidate candidates[4];
    int count;
    uint clipped_vertex_count;
    uint raw_candidate_count;
    uint unique_candidate_count;
    uint reference_face;
    uint incident_face;
    bool clipped_valid;
    bool used_fallback;
    bool reference_is_a;
};

float3 support_obb(float3 center, float3 he, float3 axes[3], float3 dir) {
    float3 result = center;
    for (int i = 0; i < 3; i++) {
        float d = dot(dir, axes[i]);
        float s = 0.0;
        if (d > 1e-6) s = 1.0;
        if (d < -1e-6) s = -1.0;
        result += axes[i] * he[i] * s;
    }
    return result;
}

int quantize_value(float value) {
    return int(floor(value * 10000.0 + (value >= 0.0 ? 0.5 : -0.5)));
}

float3 candidate_midpoint(ContactCandidate candidate) {
    return (candidate.position_on_a + candidate.position_on_b) * 0.5;
}

void build_feature_hi_lo_from_clip_vertex(
    ClippedFaceContacts clipped,
    ClipVertexData vertex,
    out uint feature_hi,
    out uint feature_lo
) {
    if (clipped.reference_is_a) {
        feature_hi = pack_feature_component(
            feature_type_from_sides(vertex.reference_sides),
            clipped.reference_face,
            vertex.reference_sides.values[0],
            vertex.reference_sides.values[1]
        );
        feature_lo = pack_feature_component(
            feature_type_from_sides(vertex.incident_sides),
            clipped.incident_face,
            vertex.incident_sides.values[0],
            vertex.incident_sides.values[1]
        );
        return;
    }

    feature_hi = pack_feature_component(
        feature_type_from_sides(vertex.incident_sides),
        clipped.incident_face,
        vertex.incident_sides.values[0],
        vertex.incident_sides.values[1]
    );
    feature_lo = pack_feature_component(
        feature_type_from_sides(vertex.reference_sides),
        clipped.reference_face,
        vertex.reference_sides.values[0],
        vertex.reference_sides.values[1]
    );
}

bool candidate_less(ContactCandidate lhs, ContactCandidate rhs, float3 tangent_u, float3 tangent_v, float3 normal) {
    if (feature_less(lhs.feature_hi, lhs.feature_lo, rhs.feature_hi, rhs.feature_lo)) {
        return true;
    }
    if (feature_less(rhs.feature_hi, rhs.feature_lo, lhs.feature_hi, lhs.feature_lo)) {
        return false;
    }

    float3 lhs_mid = candidate_midpoint(lhs);
    float3 rhs_mid = candidate_midpoint(rhs);

    int lhs_u = quantize_value(dot(lhs_mid, tangent_u));
    int rhs_u = quantize_value(dot(rhs_mid, tangent_u));
    if (lhs_u != rhs_u) return lhs_u < rhs_u;

    int lhs_v = quantize_value(dot(lhs_mid, tangent_v));
    int rhs_v = quantize_value(dot(rhs_mid, tangent_v));
    if (lhs_v != rhs_v) return lhs_v < rhs_v;

    return dot(lhs_mid, normal) < dot(rhs_mid, normal);
}

ContactResult generate_contacts(
    float3 center_a, float3 he_a, float3 axes_a[3],
    float3 center_b, float3 he_b, float3 axes_b[3],
    float3 normal,
    float separation
) {
    ContactResult result;
    result.count = 0;
    result.clipped_vertex_count = 0;
    result.raw_candidate_count = 0;
    result.unique_candidate_count = 0;
    result.reference_face = 0;
    result.incident_face = 0;
    result.clipped_valid = false;
    result.used_fallback = false;
    result.reference_is_a = true;

    float contact_gap = -separation;

    float3 tangent_u;
    if (abs(normal.x) < 0.9) tangent_u = normalize(cross(normal, float3(1, 0, 0)));
    else tangent_u = normalize(cross(normal, float3(0, 1, 0)));
    float3 tangent_v = cross(normal, tangent_u);

    ContactCandidate candidates[MAX_CLIP_VERTS];
    int candidate_count = 0;

    ClippedFaceContacts clipped = build_clipped_face_contacts(
        center_a, he_a, axes_a,
        center_b, he_b, axes_b,
        normal
    );

    if (clipped.valid) {
        result.clipped_valid = true;
        result.clipped_vertex_count = clipped.vertices.count;
        result.reference_face = clipped.reference_face;
        result.incident_face = clipped.incident_face;
        result.reference_is_a = clipped.reference_is_a;
        for (int i = 0; i < clipped.vertices.count && candidate_count < MAX_CLIP_VERTS; i++) {
            ClipVertexData vertex = clipped.vertices.verts[i];

            ContactCandidate candidate;
            candidate.position_on_a =
                clipped.reference_is_a
                    ? vertex.point
                    : (vertex.point - normal * contact_gap);
            candidate.position_on_b =
                clipped.reference_is_a
                    ? (vertex.point + normal * contact_gap)
                    : vertex.point;
            build_feature_hi_lo_from_clip_vertex(clipped, vertex, candidate.feature_hi, candidate.feature_lo);

            candidates[candidate_count++] = candidate;
        }
    }

    result.raw_candidate_count = candidate_count;

    for (int i = 1; i < candidate_count; i++) {
        ContactCandidate key = candidates[i];
        int j = i - 1;
        while (j >= 0 && candidate_less(key, candidates[j], tangent_u, tangent_v, normal)) {
            candidates[j + 1] = candidates[j];
            j--;
        }
        candidates[j + 1] = key;
    }

    int result_count = 0;
    for (int i = 0; i < candidate_count && result_count < 4; i++) {
        if (result_count > 0 &&
            feature_equal(
                result.candidates[result_count - 1].feature_hi,
                result.candidates[result_count - 1].feature_lo,
                candidates[i].feature_hi,
                candidates[i].feature_lo
            )) {
            continue;
        }

        result.candidates[result_count] = candidates[i];
        result_count++;
    }
    result.count = result_count;

    uint unique_candidate_count = 0;
    for (int i = 0; i < candidate_count; i++) {
        if (i > 0 &&
            feature_equal(
                candidates[i - 1].feature_hi,
                candidates[i - 1].feature_lo,
                candidates[i].feature_hi,
                candidates[i].feature_lo
            )) {
            continue;
        }
        unique_candidate_count++;
    }
    result.unique_candidate_count = unique_candidate_count;

    if (result.count > 0) {
        return result;
    }

    float3 support_a = support_obb(center_a, he_a, axes_a, normal);
    float3 support_b = support_obb(center_b, he_b, axes_b, -normal);
    float3 cp = (support_a + support_b) * 0.5;

    FeatureDetail detail_a = classify_feature(center_a, he_a, axes_a, normal, cp);
    FeatureDetail detail_b = classify_feature(center_b, he_b, axes_b, -normal, cp);

    result.candidates[0].position_on_a = support_a;
    result.candidates[0].position_on_b = support_b;
    result.candidates[0].feature_hi = pack_feature_component(detail_a.type, detail_a.face, detail_a.side0, detail_a.side1);
    result.candidates[0].feature_lo = pack_feature_component(detail_b.type, detail_b.face, detail_b.side0, detail_b.side1);
    result.count = 1;
    result.unique_candidate_count = 1;
    result.used_fallback = true;
    return result;
}

void record_narrow_phase_debug(
    uint body_a_idx,
    uint body_b_idx,
    SatResult sat,
    ContactResult contacts
) {
    uint expected_clipped_count = min(contacts.clipped_vertex_count, 4u);
    bool lost_clipped_contacts =
        contacts.clipped_valid &&
        contacts.clipped_vertex_count > 1 &&
        contacts.count < expected_clipped_count;
    bool dedup_collapse =
        contacts.raw_candidate_count > 1 &&
        contacts.unique_candidate_count <= 1;
    bool suspicious =
        contacts.used_fallback ||
        !contacts.clipped_valid ||
        lost_clipped_contacts ||
        dedup_collapse;
    if (!suspicious) {
        return;
    }

    uint debug_index;
    InterlockedAdd(collision_state[2], 1, debug_index);
    if (debug_index >= MAX_NARROW_PHASE_DEBUG_RECORDS) {
        return;
    }

    uint base = COLLISION_STATE_HEADER_UINTS + debug_index * NARROW_PHASE_DEBUG_RECORD_UINTS;
    uint flags = 0;
    if (contacts.clipped_valid) flags |= 0x1u;
    if (contacts.used_fallback) flags |= 0x2u;
    if (sat.is_speculative) flags |= 0x4u;
    if (contacts.reference_is_a) flags |= 0x8u;

    collision_state[base + 0] = body_a_idx;
    collision_state[base + 1] = body_b_idx;
    collision_state[base + 2] = flags;
    collision_state[base + 3] = contacts.clipped_vertex_count;
    collision_state[base + 4] = contacts.raw_candidate_count;
    collision_state[base + 5] = contacts.unique_candidate_count;
    collision_state[base + 6] = contacts.count;
    collision_state[base + 7] = (contacts.reference_face & 0xFFu) | ((contacts.incident_face & 0xFFu) << 8);
}

bool warm_start_lookup(uint ba, uint bb, uint feature_hi, uint feature_lo, uint ws_count, out GpuWarmStart result) {
    result = (GpuWarmStart)0;
    if (ws_count == 0) return false;

    uint lo = 0;
    uint hi = ws_count;
    while (lo < hi) {
        uint mid = (lo + hi) / 2;
        GpuWarmStart ws = warm_starts[mid];
        if (ws.body_a < ba || (ws.body_a == ba && ws.body_b < bb) ||
            (ws.body_a == ba && ws.body_b == bb && feature_less(ws.feature_key_hi, ws.feature_key_lo, feature_hi, feature_lo))) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    if (lo < ws_count) {
        GpuWarmStart ws = warm_starts[lo];
        if (ws.body_a == ba && ws.body_b == bb && feature_equal(ws.feature_key_hi, ws.feature_key_lo, feature_hi, feature_lo)) {
            result = ws;
            return true;
        }
    }

    return false;
}

[Layout(LayoutKind::VBDPhysics)]
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

    SatResult sat = sat_speculative(center_a, he_a, axes_a, center_b, he_b, axes_b, pc.speculative_margin);

    if (!sat.valid) return;

    float3 tangent_u;
    if (abs(sat.normal.x) < 0.9) tangent_u = normalize(cross(sat.normal, float3(1, 0, 0)));
    else tangent_u = normalize(cross(sat.normal, float3(0, 1, 0)));
    float3 tangent_v = cross(sat.normal, tangent_u);
    float3 constraint_normal = -sat.normal;

    ContactResult contacts = generate_contacts(center_a, he_a, axes_a, center_b, he_b, axes_b, sat.normal, sat.separation);
    record_narrow_phase_debug(body_a_idx, body_b_idx, sat, contacts);

    for (int p = 0; p < contacts.count; p++) {
        uint ci;
        InterlockedAdd(collision_state[0], 1, ci);
        if (ci >= MAX_CONTACTS) {
            return;
        }

        ContactCandidate candidate = contacts.candidates[p];
        float3 world_anchor_a = candidate.position_on_a;
        float3 world_anchor_b = candidate.position_on_b;
        uint feature_hi = candidate.feature_hi;
        uint feature_lo = candidate.feature_lo;

        float3 world_r_a = world_anchor_a - center_a;
        float3 world_r_b = world_anchor_b - center_b;

        float3 local_r_a = rotate_vector(quat_conjugate(ba.orientation), world_r_a);
        float3 local_r_b = rotate_vector(quat_conjugate(bb.orientation), world_r_b);

        GpuWarmStart cached_ws;
        bool has_cached = warm_start_lookup(body_a_idx, body_b_idx, feature_hi, feature_lo, pc.warm_start_count, cached_ws);
        float3 current_d = world_anchor_a - world_anchor_b;
        float current_normal_gap = dot(constraint_normal, current_d) + pc.collision_margin;
        bool reuse_cached_normal =
            has_cached &&
            (cached_ws.lambda.x < -1e-3 || current_normal_gap < -1e-4);
        bool reuse_cached_tangent =
            reuse_cached_normal &&
            has_cached;
        bool reuse_cached_sticking =
            reuse_cached_tangent &&
            cached_ws.sticking != 0;

        float3 init_lambda = float3(0, 0, 0);
        float3 init_penalty = float3(pc.penalty_min, pc.penalty_min, pc.penalty_min);
        bool sticking = has_cached && cached_ws.sticking != 0;

        if (reuse_cached_normal) {
            init_penalty.x = max(cached_ws.penalty.x, pc.penalty_min);

            float3 cached_normal_force = cached_ws.normal.xyz * cached_ws.lambda.x;
            init_lambda.x = min(dot(cached_normal_force, constraint_normal), 0.0);
        }

        if (reuse_cached_tangent) {
            init_penalty.y = max(cached_ws.penalty.y, pc.penalty_min);
            init_penalty.z = max(cached_ws.penalty.z, pc.penalty_min);

            float3 cached_tangent_force =
                cached_ws.tangent_u.xyz * cached_ws.lambda.y +
                cached_ws.tangent_v.xyz * cached_ws.lambda.z;

            init_lambda.y = dot(cached_tangent_force, tangent_u);
            init_lambda.z = dot(cached_tangent_force, tangent_v);

            float friction_bound = abs(init_lambda.x) * pc.friction_coefficient;
            init_lambda.y = clamp(init_lambda.y, -friction_bound, friction_bound);
            init_lambda.z = clamp(init_lambda.z, -friction_bound, friction_bound);
        }

        if (reuse_cached_sticking) {
            local_r_a = cached_ws.local_anchor_a.xyz;
            local_r_b = cached_ws.local_anchor_b.xyz;
        }

        float3 rAW = rotate_vector(ba.orientation, local_r_a);
        float3 rBW = rotate_vector(bb.orientation, local_r_b);
        float3 pA = center_a + rAW;
        float3 pB = center_b + rBW;
        float3 d = pA - pB;

        float3 C0 = float3(
            dot(constraint_normal, d) + pc.collision_margin,
            dot(tangent_u, d),
            dot(tangent_v, d)
        );

        float base_floor = pc.penalty_min;
        bool persistent_active_normal =
            init_lambda.x < -1e-3 &&
            C0.x < -1e-4;
        float normal_floor =
            persistent_active_normal
                ? clamp(contact_effective_mass(ba, bb, rAW, rBW, constraint_normal) / pc.h_squared, base_floor, pc.penalty_max)
                : base_floor;

        float row_floor_y =
            sticking
                ? clamp(contact_effective_mass(ba, bb, rAW, rBW, tangent_u) / pc.h_squared, pc.penalty_min, pc.penalty_max)
                : pc.penalty_min;
        float row_floor_z =
            sticking
                ? clamp(contact_effective_mass(ba, bb, rAW, rBW, tangent_v) / pc.h_squared, pc.penalty_min, pc.penalty_max)
                : pc.penalty_min;

        float3 penalty = init_penalty;
        float3 lambda = init_lambda;
        if (pc.post_stabilize != 0) {
            penalty.x = clamp(penalty.x * pc.gamma, normal_floor, pc.penalty_max);
            penalty.y = clamp(penalty.y * pc.gamma, row_floor_y, pc.penalty_max);
            penalty.z = clamp(penalty.z * pc.gamma, row_floor_z, pc.penalty_max);
        } else {
            lambda *= (pc.solver_alpha * pc.gamma);
            penalty.x = clamp(penalty.x * pc.gamma, normal_floor, pc.penalty_max);
            penalty.y = clamp(penalty.y * pc.gamma, row_floor_y, pc.penalty_max);
            penalty.z = clamp(penalty.z * pc.gamma, row_floor_z, pc.penalty_max);
        }

        GpuContact c;
        c.body_a = body_a_idx;
        c.body_b = body_b_idx;
        c.feature_key_hi = feature_hi;
        c.feature_key_lo = feature_lo;
        c.sticking = sticking ? 1u : 0u;
        c.normal = float4(constraint_normal, 0);
        c.tangent_u = float4(tangent_u, 0);
        c.tangent_v = float4(tangent_v, 0);
        c.r_a = float4(local_r_a, 0);
        c.r_b = float4(local_r_b, 0);
        c.c0_friction = float4(C0, pc.friction_coefficient);
        c.lambda = float4(lambda, 0);
        c.penalty = float4(penalty, 0);

        contact_data[ci] = c;
    }
}
