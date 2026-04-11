public static const uint FLAG_LOCKED = 1;
public static const uint FLAG_UPDATE_ORIENTATION = 2;
public static const uint FLAG_AFFECTED_BY_GRAVITY = 4;
public static const uint SLEEP_THRESHOLD = 60;
public static const uint MAX_BODIES = 500;
public static const uint MAX_CONTACTS = 2000;
public static const uint MAX_COLLISION_PAIRS = 8192;
public static const uint MAX_COLORS = 32;
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

public struct GpuBody {
    public float4 position;
    public float4 predicted_position;
    public float4 inertia_target;
    public float4 old_position;
    public float4 velocity;
    public float4 predicted_velocity;
    public float4 orientation;
    public float4 predicted_orientation;
    public float4 angular_inertia_target;
    public float4 old_orientation;
    public float4 angular_velocity;
    public float4 predicted_angular_velocity;
    public float4 motor_target;
    public float mass;
    public uint flags;
    public uint sleep_counter;
    public float accel_weight;
    public float3x3 inertia;
    public float4 half_extents;
    public float4 aabb_min;
    public float4 aabb_max;
};

public struct GpuContact {
    public uint body_a;
    public uint body_b;
    public uint feature_key_hi;
    public uint feature_key_lo;
    public uint sticking;
    public float4 normal;
    public float4 tangent_u;
    public float4 tangent_v;
    public float4 r_a;
    public float4 r_b;
    public float4 c0_friction;
    public float4 lambda;
    public float4 penalty;
};

public struct GpuMotor {
    public uint body_index;
    public float compliance;
    public float max_force;
    public uint horizontal_only;
    public float4 target_velocity;
};

public struct GpuWarmStart {
    public uint body_a;
    public uint body_b;
    public uint feature_key_hi;
    public uint feature_key_lo;
    public uint sticking;
    public float4 normal;
    public float4 tangent_u;
    public float4 tangent_v;
    public float4 local_anchor_a;
    public float4 local_anchor_b;
    public float4 lambda;
    public float4 penalty;
};

public struct GpuJoint {
    public uint body_a;
    public uint body_b;
    public uint type;
    public uint limits_enabled;
    public float4 local_anchor_a;
    public float4 local_anchor_b;
    public float4 local_axis_a;
    public float4 local_axis_b;
    public float target_distance;
    public float compliance;
    public float damping;
    public float _pad0;
    public float limit_lower;
    public float limit_upper;
    public float _pad1;
    public float _pad2;
    public float4 rest_orientation;
    public float4 pos_lambda;
    public float4 pos_penalty;
    public float4 ang_lambda;
    public float4 ang_penalty;
    public float limit_lambda;
    public float limit_penalty;
    public float _pad3;
    public float _pad4;
    public float4 pos_c0;
    public float4 ang_c0;
    public float limit_c0;
    public float _pad5;
    public float _pad6;
    public float _pad7;
};

public struct vbd_push_constants {
    public uint body_count;
    public uint contact_count;
    public uint motor_count;
    public uint color_offset;
    public uint color_count;
    public uint warm_start_count;
    public uint post_stabilize;
    public uint joint_count;
    public float h_squared;
    public float dt;
    public float beta;
    public float ang_beta;
    public float linear_damping;
    public float velocity_sleep_threshold;
    public float angular_sleep_threshold;
    public float current_alpha;
    public float collision_margin;
    public float friction_coefficient;
    public float penalty_min;
    public float penalty_max;
    public float gamma;
    public float solver_alpha;
    public float speculative_margin;
    public float stick_threshold;
    public uint substep;
    public uint iteration;
    public float convergence_threshold;
    public uint min_iterations;
    public float grid_cell_size;
};

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
