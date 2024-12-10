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
		std::cout << m_name << ": " << elapsed_time.as<Milliseconds>() << "ms\n";
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
		debug::print_value(timer->get_name(), timer->get_elapsed_time().as<milliseconds>(), milliseconds::UnitName);
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
	std::chrono::steady_clock::time_point lastUpdate = std::chrono::steady_clock::now();
	gse::time dt;

	int frameRate = 0;
	float frameCount = 0;
	float numFramesToAverage = 40;
	gse::time frameRateUpdateTime;
}

void gse::MainClock::update() {
	const auto now = std::chrono::steady_clock::now();
	const std::chrono::duration<float> deltaTime = now - lastUpdate;
	lastUpdate = now;

	// Update delta time (clamped to a max of 0.16 seconds to avoid big time jumps)
	dt = seconds(std::min(deltaTime.count(), 0.16f));

	++frameCount;
	frameRateUpdateTime += dt;

	if (frameCount >= numFramesToAverage) {
		frameRate = static_cast<int>(frameCount / frameRateUpdateTime.as<Seconds>());
		frameRateUpdateTime = time();
		frameCount = 0.f;
	}
}

gse::time gse::MainClock::getDeltaTime() {
	return dt;
}

gse::time gse::MainClock::getConstantUpdateTime(const float frameRate) {
	return seconds(1.0f / frameRate);
}

int gse::MainClock::getFrameRate() {
	return frameRate;
}