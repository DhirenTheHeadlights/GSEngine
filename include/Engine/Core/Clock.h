#pragma once

#include <chrono>
#include <algorithm>

#include "Engine/Physics/Units/UnitTemplate.h"

namespace Engine {
	using Milliseconds = Unit<float, 0.001f>;
	using Seconds = Unit<float, 1.0f>;
	using Minutes = Unit<float, 60.0f>;
	using Hours = Unit<float, 3600.0f>;

	struct Time : Quantity<Seconds> {
		using Quantity::Quantity;
	};

	class Clock {
	public:
		Clock() : startTime(std::chrono::steady_clock::now()) {}

		Time reset() {
			const Time elapsedTime = getElapsedTime();
			startTime = std::chrono::steady_clock::now();
			return elapsedTime;
		}

		Time getElapsedTime() const {
			const auto now = std::chrono::steady_clock::now();
			const std::chrono::duration<float> elapsedTime = now - startTime;
			return Time(Seconds(elapsedTime.count()));
		}
	private:
		std::chrono::steady_clock::time_point startTime;
	};
}

namespace Engine::MainClock {
	inline std::chrono::steady_clock::time_point lastUpdate = std::chrono::steady_clock::now();
	inline Time dt;

	inline int frameRate = 0;
	inline float frameCount = 0;
	inline float numFramesToAverage = 40;
	inline Time frameRateUpdateTime;

	// Call this at the beginning of each frame
	inline void update() {
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

	inline Time getDeltaTime() {
		return dt;
	}

	inline int getFrameRate() {
		return frameRate;
	}
}