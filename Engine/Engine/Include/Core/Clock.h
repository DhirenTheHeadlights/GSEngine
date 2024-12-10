#pragma once

#include <chrono>
#include <iostream>

#include "Physics/Units/Units.h"

namespace gse {
	class clock {
	public:
		clock() : startTime(std::chrono::steady_clock::now()) {}

		Time reset();
		Time get_elapsed_time() const;
	private:
		std::chrono::steady_clock::time_point startTime;
	};

	class scoped_timer : public clock {
	public:
		scoped_timer(std::string name, const bool print = true) : name(std::move(name)), print(print) {}

		~scoped_timer();

		bool is_completed() const { return completed; }
		std::string get_name() const { return name; }
		void set_completed() { completed = true; }
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

namespace gse::MainClock {
	void update();
	Time getDeltaTime();
	Time getConstantUpdateTime(const float frameRate);
	int getFrameRate();
}