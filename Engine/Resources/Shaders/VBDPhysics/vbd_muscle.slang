struct muscle_fiber {
    bool valid;
    float cos_pennation;
    float sin_pennation;
    float fiber;
    float fiber_ratio;
    float fiber_velocity;
    float active;
    float active_slope;
    float passive;
    float passive_slope;
    float force_velocity;
    float force_velocity_slope;
};

muscle_fiber muscle_fiber_of(muscle_constraint j, float path, float previous_path, float dt) {
    muscle_fiber f;
    f.valid = false;
    f.cos_pennation = 1.0;
    f.sin_pennation = 0.0;
    f.fiber = 0.0;
    f.fiber_ratio = 0.0;
    f.fiber_velocity = 0.0;
    f.active = 0.0;
    f.active_slope = 0.0;
    f.passive = 0.0;
    f.passive_slope = 0.0;
    f.force_velocity = 0.0;
    f.force_velocity_slope = 0.0;

    float tendon_free = path - j.tendon_slack_length;
    if (tendon_free <= 0.0 || j.optimal_fiber_length <= 0.0 || j.max_force <= 0.0) {
        return f;
    }
    f.valid = true;

    float thickness = j.optimal_fiber_length * sin(j.pennation_at_optimal);
    f.fiber = sqrt(tendon_free * tendon_free + thickness * thickness);
    f.cos_pennation = tendon_free / f.fiber;
    f.sin_pennation = thickness / f.fiber;
    f.fiber_ratio = f.fiber / j.optimal_fiber_length;
    f.fiber_velocity = (path - previous_path) / dt * f.cos_pennation;
    float velocity_ratio = f.fiber_velocity / j.max_contraction_velocity;

    for (int i = 0; i < 3; i++) {
        float width = active_width[i] + active_width_slope[i] * f.fiber_ratio;
        float offset = (f.fiber_ratio - active_center[i]) / width;
        float term = active_height[i] * exp(-0.5 * offset * offset);
        f.active += term;
        f.active_slope -= term * offset * (1.0 - offset * active_width_slope[i]) / width;
    }

    if (f.fiber_ratio > passive_zero_ratio) {
        float rate = passive_shape / passive_strain;
        float scale = 1.0 / (exp(passive_shape) - 1.0);
        float growth = exp(rate * (f.fiber_ratio - 1.0));
        f.passive = (growth - exp(rate * (passive_zero_ratio - 1.0))) * scale;
        f.passive_slope = rate * growth * scale;
    }

    float velocity_arg = velocity_shape[1] * velocity_ratio + velocity_shape[2];
    float velocity_root = sqrt(velocity_arg * velocity_arg + 1.0);
    float velocity_asinh = sign(velocity_arg) * log(abs(velocity_arg) + velocity_root);
    f.force_velocity = velocity_shape[0] * velocity_asinh + velocity_shape[3];
    f.force_velocity_slope = velocity_shape[0] * velocity_shape[1] / velocity_root;
    if (f.force_velocity < 0.0) {
        f.force_velocity = 0.0;
        f.force_velocity_slope = 0.0;
    }
    return f;
}

float2 muscle_tension(muscle_constraint j, float path, float previous_path, float dt) {
    muscle_fiber f = muscle_fiber_of(j, path, previous_path, dt);
    if (!f.valid) {
        return float2(0.0, 0.0);
    }

    float fiber_force = j.activation * f.active * f.force_velocity + f.passive;
    float length_slope = j.activation * f.active_slope * f.force_velocity + f.passive_slope;
    float velocity_slope = j.activation * f.active * f.force_velocity_slope;

    float path_stiffness = j.max_force * (
        f.cos_pennation * f.cos_pennation * (length_slope / j.optimal_fiber_length + velocity_slope / (j.max_contraction_velocity * dt)) +
        fiber_force * f.sin_pennation * f.sin_pennation / f.fiber
    );

    return float2(j.max_force * fiber_force * f.cos_pennation, max(path_stiffness, 0.0));
}
