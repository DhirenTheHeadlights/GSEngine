#pragma once

#include <chrono>
#include <iostream>

#include "Physics/Units/Units.h"

namespace gse {
	class clock {
	public:
		clock() : m_start_time(std::chrono::steady_clock::now()) {}

		auto reset() -> time;
		auto get_elapsed_time() const -> time;
	private:
		std::chrono::steady_clock::time_point m_start_time;
	};

	class scoped_timer : public clock {
	public:
		scoped_timer(std::string name, const bool print = true) : m_name(std::move(name)), m_print(print) {}

		~scoped_timer();

		auto is_completed() const -> bool { return m_completed; }
		auto get_name() const -> std::string { return m_name; }
		auto set_completed() -> void { m_completed = true; }
	private:
		std::string m_name;
		bool m_print = false;
		bool m_completed = true;
	};


	auto add_timer(const std::string& name) -> void;
	auto reset_timer(const std::string& name) -> void;
	auto remove_timer(const std::string& name) -> void;
	auto display_timers() -> void;
}

namespace gse::main_clock {
	auto update() -> void;
	auto get_raw_delta_time() -> time;
	auto get_current_time() -> time;
	auto get_constant_update_time() -> time;
	auto get_frame_rate() -> int;
}