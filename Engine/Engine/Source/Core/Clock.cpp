#include "Core/Clock.h"

#include <iostream>
#include <map>

#include "imgui.h"
#include "Graphics/Debug.h"

/// Clock

gse::time gse::clock::reset() {
	const time elapsed_time = get_elapsed_time();
	m_start_time = std::chrono::steady_clock::now();
	return elapsed_time;
}

gse::time gse::clock::get_elapsed_time() const {
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

namespace {
	std::map<std::string, std::unique_ptr<gse::scoped_timer>> timers;
}

void gse::add_timer(const std::string& name) {
	if (!timers.contains(name)) {
		timers[name] = std::make_unique<scoped_timer>(name, false);
	}
}

void gse::reset_timer(const std::string& name) {
	if (timers.contains(name)) {
		timers[name]->reset();
	}
}

void gse::remove_timer(const std::string& name) {
	if (timers.contains(name)) {
		timers.erase(name);
	}
}

void gse::display_timers() {
	ImGui::Begin("Timers");
	for (auto it = timers.begin(); it != timers.end();) {
		const auto& timer = it->second;
		debug::print_value(timer->get_name(), timer->get_elapsed_time().as<units::milliseconds>(), units::milliseconds::unit_name);
		if (timer->is_completed()) {
			it = timers.erase(it); // Remove completed timers
		}
		else {
			++it;
		}
	}
	ImGui::End();
}

/// Main Clock

namespace {
	std::chrono::steady_clock::time_point g_last_update = std::chrono::steady_clock::now();
	gse::time g_dt;

	int g_frame_rate = 0;
	float g_frame_count = 0;
	float g_num_frames_to_average = 40;
	gse::time g_frame_rate_update_time;
}

void gse::main_clock::update() {
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

gse::time gse::main_clock::get_delta_time() {
	return g_dt;
}

gse::time gse::main_clock::get_constant_update_time(const float frame_rate) {
	return seconds(1.0f / frame_rate);
}

int gse::main_clock::get_frame_rate() {
	return g_frame_rate;
}