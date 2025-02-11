export module gse.physics.update;

import std;

import gse.core.main_clock;
import gse.core.object_registry;
import gse.physics.broad_phase_collision;
import gse.physics.motion_component;
import gse.physics.math.units.duration;
import gse.physics.system;

export namespace gse::physics {
	auto update() -> void;
}

constexpr gse::time g_max_time_step = gse::seconds(0.25);
gse::time g_accumulator;

auto gse::physics::update() -> void {
	const time fixed_update_time = main_clock::get_constant_update_time();
	time frame_time = main_clock::get_raw_delta_time();

	if (frame_time > g_max_time_step) {
		frame_time = g_max_time_step;
	}

	g_accumulator += frame_time;

	while (g_accumulator >= fixed_update_time) {
		for (int i = 0; i < 5; i++) {
			broad_phase_collision::update();
		}

		for (auto& object : gse::registry::get_components<motion_component>()) {
			update_object(object);
		}

		g_accumulator -= fixed_update_time;
	}
}


