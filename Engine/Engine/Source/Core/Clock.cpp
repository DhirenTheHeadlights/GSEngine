#include "Core/Clock.h"

#include <iostream>
#include <map>

#include "imgui.h"
#include "Graphics/Debug.h"

/// Clock

Engine::Time Engine::Clock::reset() {
	const Time elapsedTime = getElapsedTime();
	startTime = std::chrono::steady_clock::now();
	return elapsedTime;
}

Engine::Time Engine::Clock::getElapsedTime() const {
	const auto now = std::chrono::steady_clock::now();
	const std::chrono::duration<float> elapsedTime = now - startTime;
	return Seconds(elapsedTime.count());
}

/// ScopedTimer

Engine::ScopedTimer::~ScopedTimer() {
	completed = true;
	const Time elapsedTime = reset();
	std::cout << name << ": " << elapsedTime.as<Milliseconds>() << "ms\n";
}

/// Global Timer State

std::map<std::string, std::unique_ptr<Engine::ScopedTimer>> timers;

void Engine::addTimer(const std::string& name) {
	if (!timers.contains(name)) {
		timers[name] = std::make_unique<ScopedTimer>(name); // Add if not present
	}
}

void Engine::resetTimer(const std::string& name) {
	if (timers.contains(name)) {
		timers[name]->reset();
	}
}

void Engine::removeTimer(const std::string& name) {
	if (timers.contains(name)) {
		timers.erase(name);
	}
}

void Engine::displayTimers() {
	Debug::createWindow("Timers");
	for (auto it = timers.begin(); it != timers.end();) {
		const auto& timer = it->second;
		Debug::printValue(timer->getName(), timer->getElapsedTime().as<Milliseconds>(), Milliseconds::units());
		if (timer->isCompleted()) {
			it = timers.erase(it); // Remove completed timers
		}
		else {
			++it;
		}
	}
	ImGui::End();
}

/// Main Clock

std::chrono::steady_clock::time_point lastUpdate = std::chrono::steady_clock::now();
Engine::Time dt;

int frameRate = 0;
float frameCount = 0;
float numFramesToAverage = 40;
Engine::Time frameRateUpdateTime;

void Engine::MainClock::update() {
	const auto now = std::chrono::steady_clock::now();
	const std::chrono::duration<float> deltaTime = now - lastUpdate;
	lastUpdate = now;

	// Update delta time (clamped to a max of 0.16 seconds to avoid big time jumps)
	dt = Time(Seconds(std::min(deltaTime.count(), 0.16f)));

	// Frame rate calculation
	++frameCount;
	frameRateUpdateTime += dt;

	if (frameCount >= numFramesToAverage) {
		frameRate = static_cast<int>(frameCount / frameRateUpdateTime.as<Seconds>());
		frameRateUpdateTime = Time();
		frameCount = 0.f;
	}
}

Engine::Time Engine::MainClock::getDeltaTime() {
	return dt;
}

Engine::Time Engine::MainClock::getConstantUpdateTime(const float frameRate) {
	return Seconds(1.0f / frameRate);
}

int Engine::MainClock::getFrameRate() {
	return frameRate;
}