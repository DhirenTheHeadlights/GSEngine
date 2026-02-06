export module gse.physics:integrators;

import std;

import :surfaces;
import :collision_component;
import :motion_component;

import gse.platform;
import gse.physics.math;
import gse.utility;

export namespace gse::physics {
	auto apply_force(
		motion_component& component,
		const vec3<force>& force,
		const vec3<length>& world_force_position = { 0.f, 0.f, 0.f }
	) -> void;

	auto apply_impulse(
		motion_component& component,
		const vec3<force>& force,
		const time& duration
	) -> void;

	auto update_object(
		motion_component& component,
		collision_component* collision_component
	) -> void;
}

namespace gse::physics {
    constexpr bool debug = false;
    constexpr auto gravity = gse::vec3<acceleration>(0.f, -9.8f, 0.f);

    using time_step = time_t<float, seconds>;

    auto debug_log(const std::string& msg) -> void {
        if constexpr (debug) {
            static std::ofstream log_file("physics_debug.log", std::ios::app);
            log_file << msg << '\n';
        }
    }

    auto update_friction(
        motion_component& component,
        const surface::property& surface
    ) -> void;

    auto update_gravity(
        motion_component& component,
        collision_component* coll_component
    ) -> void;

    auto update_air_resistance(
        motion_component& component
    ) -> void;

    auto update_velocity(
        motion_component& component
    ) -> void;

    auto update_position(
        motion_component& component
    ) -> void;

    auto update_rotation(
        motion_component& component
    ) -> void;

    auto update_motor(
        motion_component& component,
        const collision_component* coll_component
    ) -> void;

    auto update_bb(
        const motion_component& motion_component,
        collision_component& collision_component
    ) -> void;
}

auto gse::physics::apply_force(motion_component& component, const vec3<force>& force, const vec3<length>& world_force_position) -> void {
    if (is_zero(force)) {
        return;
    }

    const auto acceleration = min(force / std::max(component.mass, kilograms(0.0001f)), vec3<gse::acceleration>(meters_per_second_squared(100.f)));

    const auto com = component.center_of_mass + component.current_position;

    component.accumulators.current_torque += cross(world_force_position - com, force);
    component.accumulators.current_acceleration += acceleration;

    component.sleeping = false;
    component.sleep_time = 0.f;
}

auto gse::physics::apply_impulse(motion_component& component, const vec3<force>& force, const time& duration) -> void {
    if (is_zero(force)) {
        return;
    }

    const auto delta_velocity = force * duration / std::max(component.mass, kilograms(0.0001f));

    component.current_velocity += delta_velocity;

    component.sleeping = false;
    component.sleep_time = 0.f;
}

auto gse::physics::update_friction(motion_component& component, const surface::property& surface) -> void {
    if (component.airborne) {
        return;
    }

    if (is_zero(component.current_velocity)) {
        return;
    }

    const force normal = component.mass * magnitude(gravity);
    force friction = normal * surface.friction_coefficient;


    const vec3 friction_force(-friction * normalize(component.current_velocity));

    apply_force(component, friction_force, component.current_position);
}

auto gse::physics::update_gravity(motion_component& component, collision_component* coll_component) -> void {
    if (!component.affected_by_gravity) {
        return;
    }
    const auto gravity_force = gravity * component.mass;

    apply_force(component, gravity_force, component.current_position);

    if constexpr (debug) {
        debug_log(std::format("  [GRAVITY] pos={} mass={} airborne={} vel={} ang_vel={}",
            component.current_position, component.mass, component.airborne,
            component.current_velocity, component.angular_velocity));
        debug_log(std::format("  [GRAVITY] post_apply_force: accel={} torque={}",
            component.accumulators.current_acceleration, component.accumulators.current_torque));
    }

    if (!component.airborne) {
        component.accumulators.current_acceleration.y() = std::max(meters_per_second_squared(0.f), component.accumulators.current_acceleration.y());
        if (!component.motor.active) {
            update_friction(component, properties(surface::type::concrete));
        }

        if constexpr (debug) {
            debug_log(std::format("  [GRAVITY] grounded: post_friction accel={} torque={}",
                component.accumulators.current_acceleration, component.accumulators.current_torque));
        }

        // Apply gravitational tipping torque about the contact support.
        if (coll_component && coll_component->collision_information.colliding &&
            !coll_component->collision_information.collision_points.empty()) {

            const auto& contact_points = coll_component->collision_information.collision_points;
            const auto& normal = coll_component->collision_information.collision_normal;

            if constexpr (debug) {
                debug_log(std::format("  [GRAVITY] collision: normal={} penetration={} #contacts={}",
                    normal, coll_component->collision_information.penetration, contact_points.size()));
                for (size_t i = 0; i < contact_points.size(); ++i) {
                    debug_log(std::format("  [GRAVITY]   contact[{}]={}", i, contact_points[i]));
                }
            }

            vec3<length> contact_centroid = { 0.f, 0.f, 0.f };
            for (const auto& cp : contact_points) {
                contact_centroid += cp;
            }
            contact_centroid /= static_cast<float>(contact_points.size());

            const auto world_com = component.current_position + component.center_of_mass;
            const auto lever_arm = world_com - contact_centroid;

            // Split lever arm into the supported direction (along collision
            // normal) and the tangential/tipping direction.
            const auto lever_along_normal = dot(lever_arm, normal) * normal;
            const auto lever_tangential = lever_arm - lever_along_normal;
            const auto com_offset = magnitude(lever_tangential);

            if constexpr (debug) {
                debug_log(std::format("  [GRAVITY] centroid={} world_com={} lever_arm={}",
                    contact_centroid, world_com, lever_arm));
                debug_log(std::format("  [GRAVITY] lever_along_normal={} lever_tangential={} com_offset={}",
                    lever_along_normal, lever_tangential, com_offset));
            }

            constexpr length min_offset = meters(0.01f);
            if (com_offset > min_offset) {
                const auto tipping_dir = normalize(lever_tangential);

                // Measure how far the contact points extend in the tipping
                // direction from their centroid — this is the support radius
                // that can resist tipping.
                length support_extent = meters(0.f);
                for (const auto& cp : contact_points) {
                    const length proj = dot(cp - contact_centroid, tipping_dir);
                    support_extent = std::max(support_extent, proj);
                }

                // Single contact point can't define a support polygon.
                // Use smallest half-extent as a conservative stability estimate
                // to prevent extreme tipping torque from poor contact generation.
                if (contact_points.size() <= 1) {
                    const auto he = coll_component->bounding_box.half_extents();
                    const length min_extent = std::min({he.x(), he.y(), he.z()});
                    support_extent = std::max(support_extent, min_extent);
                }

                if constexpr (debug) {
                    debug_log(std::format("  [GRAVITY] tipping_dir={} support_extent={}",
                        tipping_dir, support_extent));
                }

                // Only apply torque for the fraction of the COM that
                // overhangs past the support edge.  Scale inversely
                // with object speed so that the tipping torque fills
                // the gap at low velocity (where collision impulses
                // are too weak) but fades out as the collision system
                // takes over, preventing feedback-loop jitter.
                if (com_offset > support_extent) {
                    const float speed = magnitude(component.current_velocity).as<meters_per_second>();
                    const float ang_speed = magnitude(component.angular_velocity).as<radians_per_second>();
                    constexpr float tipping_speed_threshold = 2.f;
                    const float velocity_scale = std::clamp(1.f - (speed + ang_speed) / tipping_speed_threshold, 0.f, 1.f);

                    const float overhang_fraction = (com_offset - support_extent) / com_offset;
                    const auto gravity_torque = cross(lever_arm, gravity_force);
                    component.accumulators.current_torque += gravity_torque * overhang_fraction * velocity_scale;

                    if constexpr (debug) {
                        debug_log(std::format("  [GRAVITY] TIPPING: overhang_frac={:.4f} vel_scale={:.4f} gravity_torque={} applied={}",
                            overhang_fraction, velocity_scale, gravity_torque, gravity_torque * overhang_fraction * velocity_scale));
                    }
                } else if constexpr (debug) {
                    debug_log(std::format("  [GRAVITY] NO_TIP: com_offset({}) <= support_extent({})",
                        com_offset, support_extent));
                }
            } else if constexpr (debug) {
                debug_log(std::format("  [GRAVITY] NO_TIP: com_offset({}) <= min_offset(0.01)", com_offset));
            }
        } else if constexpr (debug) {
            const bool has_coll = coll_component != nullptr;
            const bool colliding = has_coll && coll_component->collision_information.colliding;
            const size_t npts = has_coll ? coll_component->collision_information.collision_points.size() : 0;
            debug_log(std::format("  [GRAVITY] no tipping check: has_coll={} colliding={} #contacts={}",
                has_coll, colliding, npts));
        }

        if constexpr (debug) {
            debug_log(std::format("  [GRAVITY] final: accel={} torque={}",
                component.accumulators.current_acceleration, component.accumulators.current_torque));
        }
    }
}

auto gse::physics::update_air_resistance(motion_component& component) -> void {
    for (int i = 0; i < 3; ++i) {
        const velocity v = component.current_velocity[i];
        if (v == meters_per_second(0.0f)) {
            continue;
        }

        constexpr density air_density = kilograms_per_cubic_meter(1.225f);
        const float drag_coefficient = component.airborne ? 0.47f : 1.05f;
        constexpr area cross_sectional_area = square_meters(1.0f);

        const force drag_force_magnitude = 0.5f * drag_coefficient * air_density * cross_sectional_area * v * v;
        const float direction = v > meters_per_second(0.f) ? -1.0f : 1.0f;

        auto drag_force = gse::vec3<force>(
            i == 0 ? drag_force_magnitude * direction : 0.0f,
            i == 1 ? drag_force_magnitude * direction : 0.0f,
            i == 2 ? drag_force_magnitude * direction : 0.0f
        );

        apply_force(component, drag_force, component.current_position);
    }
}

auto gse::physics::update_velocity(motion_component& component) -> void {
    const auto delta_time = gse::system_clock::constant_update_time<time_step>();

    component.current_velocity += component.accumulators.current_acceleration * delta_time;

    if (!component.airborne && !component.motor.active) {
        for (int i = 0; i < 3; ++i) {
	        if (const velocity v = component.current_velocity[i]; abs(v) < meters_per_second(0.01f)) {
                component.current_velocity[i] = meters_per_second(0.f);
            }
        }
    }

    if (!component.motor.active && magnitude(component.current_velocity) > component.max_speed && !component.airborne) {
        component.current_velocity = normalize(component.current_velocity) * component.max_speed;
    }

    if (is_zero(component.current_velocity)) {
        component.current_velocity = { 0.f, 0.f, 0.f };
    }
}

auto gse::physics::update_motor(motion_component& component, const collision_component* coll_component) -> void {
    if (!component.motor.active) {
        return;
    }

    const auto dt = gse::system_clock::constant_update_time<time_step>();
    const float dt_s = dt.as<seconds>();
    const auto& target = component.motor.target_velocity;
    const bool has_target = !is_zero(target);

    // Ground movement
    if (!component.airborne) {
        if (has_target) {
            const float t = std::min(1.f, component.motor.ground_response * dt_s);
            component.current_velocity.x() += (target.x() - component.current_velocity.x()) * t;
            component.current_velocity.z() += (target.z() - component.current_velocity.z()) * t;
        } else {
            const float t = std::min(1.f, component.motor.stopping_response * dt_s);
            component.current_velocity.x() *= (1.f - t);
            component.current_velocity.z() *= (1.f - t);
        }
    } else if (has_target) {
        // Air control: steer existing momentum toward desired direction, no speed gain
        const auto hspeed = magnitude(vec3<velocity>(
            component.current_velocity.x(), meters_per_second(0.f), component.current_velocity.z()
        ));
        if (hspeed > meters_per_second(0.001f)) {
            const auto dir = normalize(vec3<velocity>(target.x(), meters_per_second(0.f), target.z()));
            const auto air_target = hspeed * unitless::vec3(dir.x().as<meters_per_second>(), 0.f, dir.z().as<meters_per_second>());
            const float t = std::min(1.f, component.motor.air_steering * dt_s);
            component.current_velocity.x() += (air_target.x() - component.current_velocity.x()) * t;
            component.current_velocity.z() += (air_target.z() - component.current_velocity.z()) * t;
        }
    }

    // Wall-sliding: remove velocity component going into active collisions
    if (coll_component && coll_component->collision_information.colliding) {
        const auto& n = coll_component->collision_information.collision_normal;
        const auto vn = n.x() * component.current_velocity.x() + n.z() * component.current_velocity.z();
        if (vn < meters_per_second(0.f)) {
            component.current_velocity.x() -= vn * n.x();
            component.current_velocity.z() -= vn * n.z();
        }
    }

    // Jump
    if (component.motor.jump_requested && !component.airborne) {
        component.current_velocity.y() = component.motor.jump_speed;
        component.airborne = true;
    }
    component.motor.jump_requested = false;

    // Clamp horizontal speed
    const auto hspeed = magnitude(vec3<velocity>(
        component.current_velocity.x(), meters_per_second(0.f), component.current_velocity.z()
    ));
    if (hspeed > component.max_speed) {
        const float scale = component.max_speed / hspeed;
        component.current_velocity.x() *= scale;
        component.current_velocity.z() *= scale;
    }
}

auto gse::physics::update_position(motion_component& component) -> void {
    component.current_position += (component.current_velocity * gse::system_clock::constant_update_time<time_step>()) + component.accumulators.position_correction;
}

auto gse::physics::update_rotation(motion_component& component) -> void {
    if (!component.update_orientation) {
        return;
	}

    const auto alpha = component.accumulators.current_torque / component.moment_of_inertia;

    const auto delta_time = system_clock::constant_update_time<time_step>();

    if constexpr (debug) {
        debug_log(std::format("  [ROTATION] torque_in={} MoI={} alpha={}",
            component.accumulators.current_torque, component.moment_of_inertia, alpha));
        debug_log(std::format("  [ROTATION] ang_vel_before={}", component.angular_velocity));
    }

    component.angular_velocity += alpha * delta_time;

    if constexpr (debug) {
        debug_log(std::format("  [ROTATION] ang_vel_after_alpha={}", component.angular_velocity));
    }

    constexpr float angular_damping = 1.0f;
    const float factor = std::max(0.f, 1.f - angular_damping * delta_time.as<seconds>());
    component.angular_velocity *= factor;

    if constexpr (debug) {
        debug_log(std::format("  [ROTATION] ang_vel_after_damp={} (factor={:.4f})", component.angular_velocity, factor));
    }

    if (constexpr angular_velocity max_angular_speed = radians_per_second(20.f); magnitude(component.angular_velocity) > max_angular_speed) {
        if constexpr (debug) {
            debug_log(std::format("  [ROTATION] CLAMPED ang_vel from mag={}", magnitude(component.angular_velocity)));
        }
        component.angular_velocity = normalize(component.angular_velocity) * max_angular_speed;
    }

    component.accumulators.current_torque = {};

    const unitless::vec3 angular_velocity_vec = component.angular_velocity.as<radians_per_second>();
    const quat omega_quaternion = { 0.f, angular_velocity_vec.x(), angular_velocity_vec.y(), angular_velocity_vec.z() };

    const quat delta_quaternion = 0.5f * omega_quaternion * component.orientation;
    component.orientation += delta_quaternion * delta_time.as<seconds>();
    component.orientation = normalize(component.orientation);

    if constexpr (debug) {
        debug_log(std::format("  [ROTATION] orientation={}", component.orientation));
    }
}

auto gse::physics::update_bb(const motion_component& motion_component, collision_component& collision_component) -> void {
    collision_component.bounding_box.update(motion_component.current_position, motion_component.orientation);
}

auto gse::physics::update_object(motion_component& component, collision_component* collision_component) -> void {
    const auto delta_time = gse::system_clock::constant_update_time<time_step>();

    if (component.position_locked) {
        component.current_velocity = { 0.f, 0.f, 0.f };
        component.angular_velocity = {};
        component.accumulators = {};
        component.moving = false;
        component.airborne = false;

        if (collision_component) {
            update_bb(component, *collision_component);
        }

        return;
    }

    if (component.sleeping && !component.self_controlled) {
        component.current_velocity = { 0.f, 0.f, 0.f };
        component.angular_velocity = {};
        component.accumulators = {};
        component.moving = false;

        if (collision_component) {
            update_bb(component, *collision_component);
        }

        return;
    }

    if constexpr (debug) {
        static int frame_counter = 0;
        debug_log(std::format("=== OBJECT FRAME {} === pos={} mass={} airborne={} sleeping={}",
            frame_counter++, component.current_position, component.mass,
            component.airborne, component.sleeping));
        debug_log(std::format("  [START] vel={} ang_vel={} orientation={}",
            component.current_velocity, component.angular_velocity, component.orientation));
        debug_log(std::format("  [START] pos_correction={} accel={} torque={}",
            component.accumulators.position_correction, component.accumulators.current_acceleration,
            component.accumulators.current_torque));
    }

    update_gravity(component, collision_component);
    update_air_resistance(component);
    update_velocity(component);
    update_motor(component, collision_component);
    update_position(component);
    update_rotation(component);

    if (collision_component) {
        update_bb(component, *collision_component);
    }

    component.accumulators = {};

    const auto linear_speed = magnitude(component.current_velocity).as<meters_per_second>();
    const auto angular_speed = magnitude(component.angular_velocity).as<radians_per_second>();

    constexpr time linear_sleep_threshold = seconds(0.05f);
    constexpr time angular_sleep_threshold = seconds(0.1f);
    constexpr time sleep_time_threshold = seconds(0.5f);

    if (const bool can_try_sleep = component.can_sleep && !component.self_controlled && !component.airborne; can_try_sleep && linear_speed < linear_sleep_threshold && angular_speed < angular_sleep_threshold) {
        component.sleep_time += delta_time;
        if (component.sleep_time >= sleep_time_threshold) {
            component.sleeping = true;
            component.current_velocity = { 0.f, 0.f, 0.f };
            component.angular_velocity = {};
        }
    }
    else {
        component.sleep_time = 0.f;
        component.sleeping = false;
    }

    constexpr float linear_moving_threshold = 0.01f;
    constexpr float angular_moving_threshold = 0.05f;

    component.moving = linear_speed > linear_moving_threshold || angular_speed > angular_moving_threshold;

    if constexpr (debug) {
        debug_log(std::format("  [END] pos={} vel={} ang_vel={}",
            component.current_position, component.current_velocity, component.angular_velocity));
        debug_log(std::format("  [END] moving={} sleeping={} sleep_time={} lin_speed={:.4f} ang_speed={:.4f}",
            component.moving, component.sleeping, component.sleep_time, linear_speed, angular_speed));
    }
}
