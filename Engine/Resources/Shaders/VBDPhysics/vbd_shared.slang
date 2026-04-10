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
    public float4 inv_inertia_col0;
    public float4 inv_inertia_col1;
    public float4 inv_inertia_col2;
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
