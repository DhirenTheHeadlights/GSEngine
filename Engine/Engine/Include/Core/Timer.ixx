module;

#include "imgui.h"

export module gse.core.timer;

import std;

import gse.core.clock;
import gse.graphics.debug;

export namespace gse {
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

gse::scoped_timer::~scoped_timer() {
	m_completed = true;
	const time elapsed_time = reset();
	if (m_print) {
		std::cout << m_name << ": " << elapsed_time.as<units::milliseconds>() << "ms\n";
	}
}

std::map<std::string, std::unique_ptr<gse::scoped_timer>> g_timers;

auto gse::add_timer(const std::string& name) -> void {
	if (!g_timers.contains(name)) {
		g_timers[name] = std::make_unique<scoped_timer>(name, false);
	}
}

auto gse::reset_timer(const std::string& name) -> void {
	if (g_timers.contains(name)) {
		g_timers[name]->reset();
	}
}

auto gse::remove_timer(const std::string& name) -> void {
	if (g_timers.contains(name)) {
		g_timers.erase(name);
	}
}

auto gse::display_timers() -> void {
	ImGui::Begin("Timers");
	for (auto it = g_timers.begin(); it != g_timers.end();) {
		const auto& timer = it->second;
		debug::print_value(timer->get_name(), timer->get_elapsed_time().as<units::milliseconds>(), units::milliseconds::unit_name);
		if (timer->is_completed()) {
			it = g_timers.erase(it); // Remove completed timers
		}
		else {
			++it;
		}
	}
	ImGui::End();
}