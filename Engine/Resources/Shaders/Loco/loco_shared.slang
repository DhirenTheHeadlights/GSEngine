public float loco_relative_stiffness(float a) {
    return clamp(loco_stiffness_base * exp(a), loco_stiffness_min, loco_stiffness_max) / loco_stiffness_base;
}

public float4 loco_framed_by(float4 q, uint framed, float4 segment_frame) {
    return framed != 0 ? quat_mul(q, segment_frame) : q;
}

public float loco_upright_of(float4 pelvis_q) {
    return clamp(cross(rotate_vector(pelvis_q, float3(1, 0, 0)), rotate_vector(pelvis_q, float3(0, 0, -1))).y, 0.0, 1.0);
}

public bool loco_fallen(float pelvis_y, float upright, float fall_height, float fall_upright) {
    return pelvis_y < fall_height || upright < fall_upright;
}
