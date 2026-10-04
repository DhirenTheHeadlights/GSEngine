public float loco_relative_stiffness(float a) {
    return clamp(loco_stiffness_base * exp(a), loco_stiffness_min, loco_stiffness_max) / loco_stiffness_base;
}
