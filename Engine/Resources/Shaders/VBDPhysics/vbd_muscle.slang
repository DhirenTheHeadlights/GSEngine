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

float passive_corner_value(float4 p, float u) {
    float w = 1.0 - u;
    return w * w * w * w * w * p[0] + 5.0 * w * w * w * u * (w + 2.0 * u) * p[1] + 5.0 * w * u * u * u * (2.0 * w + u) * p[2] + u * u * u * u * u * p[3];
}

float passive_corner_slope(float4 p, float u) {
    float w = 1.0 - u;
    return 5.0 * (w * w * w * w * (p[1] - p[0]) + 6.0 * w * w * u * u * (p[2] - p[1]) + u * u * u * u * (p[3] - p[2]));
}

float2 passive_fiber_of(float fiber_ratio) {
    if (fiber_ratio <= passive_start) {
        return float2(0.0, 0.0);
    }
    if (fiber_ratio >= passive_end) {
        return float2(1.0 + passive_stiffness * (fiber_ratio - passive_end), passive_stiffness);
    }

    bool toe = fiber_ratio < passive_knee;
    float4 xs = toe ? passive_toe_x : passive_rise_x;
    float4 ys = toe ? passive_toe_y : passive_rise_y;
    float u = (fiber_ratio - xs[0]) / (xs[3] - xs[0]);
    for (uint i = 0; i < passive_iterations; i++) {
        u = clamp(u - (passive_corner_value(xs, u) - fiber_ratio) / passive_corner_slope(xs, u), 0.0, 1.0);
    }
    return float2(passive_corner_value(ys, u), passive_corner_slope(ys, u) / passive_corner_slope(xs, u));
}

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

    float2 passive = passive_fiber_of(f.fiber_ratio);
    f.passive = passive.x;
    f.passive_slope = passive.y;

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

float2 damped_fiber_tension(muscle_constraint j, float path, float previous_path, float dt, float damping) {
    muscle_fiber f = muscle_fiber_of(j, path, previous_path, dt);
    if (!f.valid) {
        return float2(0.0, 0.0);
    }

    float fiber_force = j.activation * f.active * f.force_velocity + f.passive + damping * (f.fiber_velocity / j.max_contraction_velocity);
    float length_slope = j.activation * f.active_slope * f.force_velocity + f.passive_slope;
    float velocity_slope = j.activation * f.active * f.force_velocity_slope + damping;

    float path_stiffness = j.max_force * (
        f.cos_pennation * f.cos_pennation * (length_slope / j.optimal_fiber_length + velocity_slope / (j.max_contraction_velocity * dt)) +
        fiber_force * f.sin_pennation * f.sin_pennation / f.fiber
    );

    return float2(j.max_force * fiber_force * f.cos_pennation, max(path_stiffness, 0.0));
}

float2 tendon_response_of(muscle_constraint j, float tendon) {
    float growth = tendon_scale * exp(j.tendon_shape * (tendon / j.tendon_slack_length - tendon_offset));
    if (growth <= tendon_shift) {
        return float2(0.0, 0.0);
    }
    return float2(j.max_force * (growth - tendon_shift), j.max_force * (j.tendon_shape * growth) / j.tendon_slack_length);
}

float3 compliant_fiber_of(muscle_constraint j, float path, float previous_projection, float dt) {
    bool settled = previous_projection > 0.0;
    float damping = settled ? fiber_damping : 0.0;
    float thickness = j.optimal_fiber_length * sin(j.pennation_at_optimal);
    float shortest = max(thickness * (fiber_min_cos / sqrt(1.0 - fiber_min_cos * fiber_min_cos)), j.optimal_fiber_length * fiber_min_ratio);
    float zero_stretch = tendon_offset + log(tendon_shift / tendon_scale) / j.tendon_shape;
    float taut = path - j.tendon_slack_length * zero_stretch;
    float longest = settled ? max(taut, previous_projection) : taut;
    if (longest <= shortest) {
        return float3(shortest, 0.0, 0.0);
    }

    float lo = shortest;
    float hi = longest;
    float x = settled ? clamp(previous_projection, lo, hi) : hi;
    float step = hi - lo;
    float prior = step;
    float2 tendon = tendon_response_of(j, path - x);
    float2 fiber = damped_fiber_tension(j, x + j.tendon_slack_length, (settled ? previous_projection : x) + j.tendon_slack_length, dt, damping);
    float best = x;
    float2 best_tendon = tendon;
    float2 best_fiber = fiber;
    for (uint i = 0; i < fiber_iterations; i++) {
        float residual = tendon.x - fiber.x;
        if (abs(residual) <= j.max_force * fiber_tolerance) {
            break;
        }
        if (residual > 0.0) {
            lo = x;
        } else {
            hi = x;
        }
        float slope = tendon.y + fiber.y;
        bool newton = slope > 0.0 && x + residual / slope >= lo && x + residual / slope <= hi && abs(residual) * 2.0 <= abs(prior * slope);
        prior = step;
        step = newton ? residual / slope : (hi - lo) * 0.5;
        x = newton ? x + step : lo + step;
        tendon = tendon_response_of(j, path - x);
        fiber = damped_fiber_tension(j, x + j.tendon_slack_length, (settled ? previous_projection : x) + j.tendon_slack_length, dt, damping);
        if (abs(tendon.x - fiber.x) < abs(best_tendon.x - best_fiber.x)) {
            best = x;
            best_tendon = tendon;
            best_fiber = fiber;
        }
    }

    float series = best_tendon.y + best_fiber.y;
    return float3(best, best_tendon.x, series > 0.0 ? best_tendon.y * (best_fiber.y / series) : 0.0);
}

float2 muscle_tension(muscle_constraint j, float path, float previous_path, float dt) {
    if (j.tendon_shape > 0.0) {
        return compliant_fiber_of(j, path, j.fiber_projection_start, dt).yz;
    }
    return damped_fiber_tension(j, path, previous_path, dt, 0.0);
}

float muscle_fiber_path(muscle_constraint j) {
    return j.tendon_shape > 0.0 ? j.fiber_projection_start + j.tendon_slack_length : j.path_length_start;
}

float muscle_isometric_stiffness(muscle_constraint j, float dt) {
    if (j.tendon_shape <= 0.0) {
        return muscle_tension(j, j.path_length_start, j.path_length_start, dt).y;
    }
    float fiber_path = muscle_fiber_path(j);
    float tendon = tendon_response_of(j, j.path_length_start - j.fiber_projection_start).y;
    float fiber = damped_fiber_tension(j, fiber_path, fiber_path, dt, 0.0).y;
    float series = tendon + fiber;
    return series > 0.0 ? tendon * (fiber / series) : 0.0;
}

void advance_muscle_fiber(inout muscle_constraint j, float path, float dt) {
    if (j.tendon_shape <= 0.0) {
        return;
    }
    j.fiber_projection_start = compliant_fiber_of(j, path, j.fiber_projection_start, dt).x;
}
