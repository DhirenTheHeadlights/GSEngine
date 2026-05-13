__exported import generated.vbd_physics_types;

public static const uint FLAG_LOCKED = 1;
public static const uint FLAG_UPDATE_ORIENTATION = 2;
public static const uint FLAG_AFFECTED_BY_GRAVITY = 4;
public static const uint SLEEP_THRESHOLD = 60;
public static const uint MAX_BODIES = 2048;
public static const uint MAX_CONTACTS = 16384;
public static const uint MAX_COLLISION_PAIRS = 16384;
public static const uint MAX_COLORS = 16;
public static const uint MAX_JOINTS = 128;

public static const uint JOINT_TYPE_DISTANCE = 0;
public static const uint JOINT_TYPE_FIXED = 1;
public static const uint JOINT_TYPE_HINGE = 2;
public static const uint JOINT_TYPE_SLIDER = 3;

public static const uint MAX_CONTACT_ADJACENCY = MAX_CONTACTS * 2;
public static const uint MAX_JOINT_ADJACENCY = MAX_JOINTS * 2;

public static const uint CONVERGENCE_MAX_DELTA_INDEX = 4;
public static const uint CONVERGED_FLAG_INDEX = 5;

public static const uint GRID_TABLE_SIZE = 4096;
public static const uint GRID_MAX_ENTRIES = MAX_BODIES * 8;

public uint safe_load_uint(RWStructuredBuffer<uint> buf, uint index, uint max_index) {
    return (index < max_index) ? buf[index] : 0;
}

public float3 rotate_vector(float4 q, float3 v) {
    float3 u = q.yzw;
    float s = q.x;
    return 2.0 * dot(u, v) * u + (s * s - dot(u, u)) * v + 2.0 * s * cross(u, v);
}

public float4 quat_mul(float4 a, float4 b) {
    return float4(
        a.x * b.x - a.y * b.y - a.z * b.z - a.w * b.w,
        a.x * b.y + a.y * b.x + a.z * b.w - a.w * b.z,
        a.x * b.z - a.y * b.w + a.z * b.x + a.w * b.y,
        a.x * b.w + a.y * b.z - a.z * b.y + a.w * b.x
    );
}

public float4 quat_conjugate(float4 q) {
    return float4(q.x, -q.y, -q.z, -q.w);
}

public float4 quat_normalize(float4 q) {
    float len = length(q);
    return len > 1e-10 ? q / len : float4(1, 0, 0, 0);
}

public float3 to_axis_angle(float4 q) {
    float q_s = q.x;
    float3 q_v = q.yzw;
    if (q_s < 0.0) { q_s = -q_s; q_v = -q_v; }
    if (q_s > 0.9999) return q_v * 2.0;
    float angle = 2.0 * acos(clamp(q_s, 0.0, 1.0));
    float sin_half = sqrt(1.0 - q_s * q_s);
    return (sin_half > 1e-6) ? q_v * (angle / sin_half) : q_v * 2.0;
}

public typedef float3x3 mat3;

public mat3 outer_product3(float3 a, float3 b) {
    return mat3(a * b.x, a * b.y, a * b.z);
}

public mat3 mat3_identity(float s) {
    return mat3(
        float3(s, 0, 0),
        float3(0, s, 0),
        float3(0, 0, s)
    );
}

public mat3 mat3_add(mat3 a, mat3 b) {
    return mat3(a[0] + b[0], a[1] + b[1], a[2] + b[2]);
}

public float3 mat3_mul_vec(mat3 m, float3 v) {
    return m[0] * v.x + m[1] * v.y + m[2] * v.z;
}

public mat3 mat3_mul(mat3 a, mat3 b) {
    return mat3(
        mat3_mul_vec(a, b[0]),
        mat3_mul_vec(a, b[1]),
        mat3_mul_vec(a, b[2])
    );
}

public mat3 mat3_scale(mat3 m, float s) {
    return mat3(m[0] * s, m[1] * s, m[2] * s);
}

public mat3 mat3_transpose(mat3 m) {
    return mat3(
        float3(m[0].x, m[1].x, m[2].x),
        float3(m[0].y, m[1].y, m[2].y),
        float3(m[0].z, m[1].z, m[2].z)
    );
}

public float mat3_det(mat3 m) {
    return m[0].x * (m[1].y * m[2].z - m[2].y * m[1].z)
         - m[1].x * (m[0].y * m[2].z - m[2].y * m[0].z)
         + m[2].x * (m[0].y * m[1].z - m[1].y * m[0].z);
}

public mat3 mat3_inverse(mat3 m) {
    float d = mat3_det(m);
    float diag_max = max(abs(m[0].x), max(abs(m[1].y), abs(m[2].z)));
    float threshold = diag_max * diag_max * diag_max * 1e-12;
    if (abs(d) < max(threshold, 1e-20)) {
        float s = (diag_max > 1e-20) ? 1.0 / diag_max : 1.0;
        return mat3_identity(s);
    }
    float inv_d = 1.0 / d;
    mat3 result;
    result[0] = float3(
        (m[1].y * m[2].z - m[2].y * m[1].z) * inv_d,
        (m[2].y * m[0].z - m[0].y * m[2].z) * inv_d,
        (m[0].y * m[1].z - m[1].y * m[0].z) * inv_d);
    result[1] = float3(
        (m[2].x * m[1].z - m[1].x * m[2].z) * inv_d,
        (m[0].x * m[2].z - m[2].x * m[0].z) * inv_d,
        (m[1].x * m[0].z - m[0].x * m[1].z) * inv_d);
    result[2] = float3(
        (m[1].x * m[2].y - m[2].x * m[1].y) * inv_d,
        (m[2].x * m[0].y - m[0].x * m[2].y) * inv_d,
        (m[0].x * m[1].y - m[1].x * m[0].y) * inv_d);
    return result;
}

public mat3 rotation_from_quat(float4 q) {
    float3 ax = rotate_vector(q, float3(1, 0, 0));
    float3 ay = rotate_vector(q, float3(0, 1, 0));
    float3 az = rotate_vector(q, float3(0, 0, 1));
    return mat3(ax, ay, az);
}

public bool is_finite3(float3 v) {
    return !any(isinf(v)) && !any(isnan(v));
}
