#pragma once

#include <chrono>
#include <iostream>

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
		ScopedTimer(std::string name, const bool print = true) : name(std::move(name)), print(print) {}

		~ScopedTimer();

		bool isCompleted() const { return completed; }
		std::string getName() const { return name; }
		void setCompleted() { completed = true; }
	private:
		std::string name;
		bool print = false;
		bool completed = true;
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