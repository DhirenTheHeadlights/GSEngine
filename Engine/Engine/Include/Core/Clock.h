#pragma once

#include <chrono>
#include <iostream>

#include "Physics/Units/Units.h"

namespace gse {
	class clock {
	public:
		clock() : m_start_time(std::chrono::steady_clock::now()) {}

		time reset();
		time get_elapsed_time() const;
	private:
		std::chrono::steady_clock::time_point m_start_time;
	};

	class scoped_timer : public clock {
	public:
		scoped_timer(std::string name, const bool print = true) : m_name(std::move(name)), m_print(print) {}

		~scoped_timer();

		bool is_completed() const { return m_completed; }
		std::string get_name() const { return m_name; }
		void set_completed() { m_completed = true; }
	private:
		std::string m_name;
		bool m_print = false;
		bool m_completed = true;
	};


	void add_timer(const std::string& name);
	void reset_timer(const std::string& name);
	void remove_timer(const std::string& name);
	void display_timers();
}

namespace gse::main_clock {
	void update();
	time get_delta_time();
	time get_constant_update_time(const float frame_rate);
	int get_frame_rate();
}