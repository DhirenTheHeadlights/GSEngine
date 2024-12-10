#include "Core/Clock.h"

#include <iostream>
#include <map>

#include "imgui.h"
#include "Graphics/Debug.h"

/// Clock

gse::Time gse::clock::reset() {
	const Time elapsedTime = get_elapsed_time();
	startTime = std::chrono::steady_clock::now();
	return elapsedTime;
}

gse::Time gse::clock::get_elapsed_time() const {
	const auto now = std::chrono::steady_clock::now();
	const std::chrono::duration<float> elapsedTime = now - startTime;
	return seconds(elapsedTime.count());
}

/// ScopedTimer

gse::scoped_timer::~scoped_timer() {
	completed = true;
	const Time elapsedTime = reset();
	if (print) {
		std::cout << name << ": " << elapsedTime.as<Milliseconds>() << "ms\n";
	}
}

/// Global Timer State

namespace {
	std::map<std::string, std::unique_ptr<gse::scoped_timer>> timers;
}

void gse::addTimer(const std::string& name) {
	if (!timers.contains(name)) {
		timers[name] = std::make_unique<scoped_timer>(name, false);
	}
}

void gse::resetTimer(const std::string& name) {
	if (timers.contains(name)) {
		timers[name]->reset();
	}
}

void gse::removeTimer(const std::string& name) {
	if (timers.contains(name)) {
		timers.erase(name);
	}
}

void gse::displayTimers() {
	ImGui::Begin("Timers");
	for (auto it = timers.begin(); it != timers.end();) {
		const auto& timer = it->second;
		Debug::printValue(timer->get_name(), timer->get_elapsed_time().as<Milliseconds>(), Milliseconds::UnitName);
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
	gse::Time dt;

	int frameRate = 0;
	float frameCount = 0;
	float numFramesToAverage = 40;
	gse::Time frameRateUpdateTime;
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
		frameRateUpdateTime = Time();
		frameCount = 0.f;
	}
}

gse::Time gse::MainClock::getDeltaTime() {
	return dt;
}

gse::Time gse::MainClock::getConstantUpdateTime(const float frameRate) {
	return seconds(1.0f / frameRate);
}

int gse::MainClock::getFrameRate() {
	return frameRate;
}