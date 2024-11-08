#pragma once

#include <chrono>
#include "Physics/Units/Units.h"

namespace Engine {
	class Clock {
	public:
		Clock() : startTime(std::chrono::steady_clock::now()) {}

		Time reset();
		Time getElapsedTime() const;
	private:
		std::chrono::steady_clock::time_point startTime;
	};

	class ScopedTimer : public Clock {
	public:
		ScopedTimer(const std::string& name) : name(name), completed(false) {}

		~ScopedTimer();

		bool isCompleted() const { return completed; }
		std::string getName() const { return name; }
		void setCompleted() { completed = true; }
	private:
		std::string name;
		bool completed;
	};


	void addTimer(const std::string& name);
	void resetTimer(const std::string& name);
	void removeTimer(const std::string& name);
	void displayTimers();
}

namespace Engine::MainClock {
	void update();
	Time getDeltaTime();
	Time getConstantUpdateTime(const float frameRate);
	int getFrameRate();
}