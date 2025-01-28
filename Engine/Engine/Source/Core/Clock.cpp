module;

#include "imgui.h"

module gse.core.clock;

import std;

import gse.graphics.debug;
import gse.physics.math.units;

/// Clock

auto gse::clock::reset() -> time {
	const time elapsed_time = get_elapsed_time();
	m_start_time = std::chrono::steady_clock::now();
	return elapsed_time;
}

auto gse::clock::get_elapsed_time() const -> time {
	const auto now = std::chrono::steady_clock::now();
	const std::chrono::duration<float> elapsed_time = now - m_start_time;
	return seconds(elapsed_time.count());
}

/// ScopedTimer

gse::scoped_timer::~scoped_timer() {
	m_completed = true;
	const time elapsed_time = reset();
	if (m_print) {
		std::cout << m_name << ": " << elapsed_time.as<units::milliseconds>() << "ms\n";
	}
}

/// Global Timer State

std::map<std::string, std::unique_ptr<gse::scoped_timer>> g_timers;

auto gse::add_timer(const std::string& name) -> void {
	if (!g_timers.contains(name)) {
		g_timers[name] = std::make_unique<scoped_timer>(name, false);
	}
}

auto gse::reset_timer(const std::string& name) -> void {
	if (g_timers.contains(name)) {
		g_timers[name]->reset();
	}
}

auto gse::remove_timer(const std::string& name) -> void {
	if (g_timers.contains(name)) {
		g_timers.erase(name);
	}
}

auto gse::display_timers() -> void {
	ImGui::Begin("Timers");
	for (auto it = g_timers.begin(); it != g_timers.end();) {
		const auto& timer = it->second;
		debug::print_value(timer->get_name(), timer->get_elapsed_time().as<units::milliseconds>(), units::milliseconds::unit_name);
		if (timer->is_completed()) {
			it = g_timers.erase(it); // Remove completed timers
		}
		else {
			++it;
		}
	}
	ImGui::End();
}

/// Main Clock

std::chrono::steady_clock::time_point g_last_update = std::chrono::steady_clock::now();
gse::time g_dt;

int g_frame_rate = 0;
float g_frame_count = 0;
float g_num_frames_to_average = 40;
gse::time g_frame_rate_update_time;

gse::clock g_main_clock;

auto gse::main_clock::update() -> void {
	const auto now = std::chrono::steady_clock::now();
	const std::chrono::duration<float> delta_time = now - g_last_update;
	g_last_update = now;

	// Update delta time (clamped to a max of 0.16 seconds to avoid big time jumps)
	g_dt = seconds(std::min(delta_time.count(), 0.16f));

	++g_frame_count;
	g_frame_rate_update_time += g_dt;

	if (g_frame_count >= g_num_frames_to_average) {
		g_frame_rate = static_cast<int>(g_frame_count / g_frame_rate_update_time.as<units::seconds>());
		g_frame_rate_update_time = time();
		g_frame_count = 0.f;
	}
}

auto gse::main_clock::get_raw_delta_time() -> time {
	return g_dt;
}

auto gse::main_clock::get_current_time() -> time {
	return g_main_clock.get_elapsed_time();
}

auto gse::main_clock::get_constant_update_time() -> time {
	return seconds(0.01f);
}

auto gse::main_clock::get_frame_rate() -> int {
	return g_frame_rate;
}