#pragma once

#include <chrono>
#include <algorithm>

namespace Engine::Clock {

	// Custom time types
	struct Seconds {
		Seconds() = default;
		explicit Seconds(const float time) : time(time) {}

		explicit operator float() const {
			return time;
		}

		float time = 0.0f;
	};

	struct Milliseconds {
		Milliseconds() = default;
		explicit Milliseconds(const float time) : time(time) {}

		explicit operator float() const {
			return time;
		}

		float time = 0.0f;
	};

	struct Minutes {
		Minutes() = default;
		explicit Minutes(const float time) : time(time) {}

		explicit operator float() const {
			return time;
		}

		float time = 0.0f;
	};

	// Time class to convert between Seconds, Milliseconds, and Minutes
	struct Time {
		Time() = default;
		explicit Time(const Seconds& seconds) : time(seconds.time) {}
		explicit Time(const Milliseconds& milliseconds) : time(milliseconds.time / 1000.0f) {}
		explicit Time(const Minutes& minutes) : time(minutes.time * 60.0f) {}

		float asSeconds() const {
			return time;
		}

		float asMilliseconds() const {
			return time * 1000.0f;
		}

		float asMinutes() const {
			return time / 60.0f;
		}
	private:
		float time = 0.0f;
	};

	// Clock system using custom time types
	inline std::chrono::steady_clock::time_point lastUpdate = std::chrono::steady_clock::now();
	inline Time dt;

	inline int frameRate = 0;
	inline int frameCount = 0;
	inline int numFramesToAverage = 40;
	inline Time frameRateUpdateTime{ Seconds(0.0f) };

	// Call this at the beginning of each frame
	inline void update() {
		const auto now = std::chrono::steady_clock::now();
		const std::chrono::duration<float> deltaTime = now - lastUpdate;
		lastUpdate = now;

		// Update delta time (clamped to a max of 0.16 seconds to avoid big time jumps)
		dt = Time(Seconds(std::min(deltaTime.count(), 0.16f)));

		// Frame rate calculation
		++frameCount;
		frameRateUpdateTime = Time(Seconds(frameRateUpdateTime.asSeconds() + dt.asSeconds()));

		if (frameCount >= numFramesToAverage) {
			frameRate = static_cast<int>(frameCount / frameRateUpdateTime.asSeconds());
			frameRateUpdateTime = Time(Seconds(0.0f)); // Reset frame rate update time
			frameCount = 0;
		}
	}

	// Get delta time for all classes
	inline Time getDeltaTime() {
		return dt;
	}

	// Get frame rate
	inline int getFrameRate() {
		return frameRate;
	}
}
