export module gse.utility:timer;

import std;

import :clock;

import gse.physics.math;

export namespace gse {
	class scoped_timer : public clock {
	public:
		explicit scoped_timer(std::string name, const bool print = true) : m_name(std::move(name)), m_print(print) {}
		~scoped_timer();

		auto completed() const -> bool { return m_completed; }
		auto name() const -> std::string { return m_name; }
		auto set_completed() -> void { m_completed = true; }
	private:
		std::string m_name;
		bool m_print = false;
		bool m_completed = true;
	};

	auto add_timer(
		const std::string& name, 
		const std::function<void()>& to_time
	) -> void;

	auto timers() -> std::map<std::string, scoped_timer>&;
}

namespace gse {
	std::map<std::string, scoped_timer> global_timers;
}

gse::scoped_timer::~scoped_timer() {
	m_completed = true;
	const time elapsed_time = reset();
	if (m_print) {
		std::println("Timer '{}' completed in {}ms", m_name, elapsed_time.as<milliseconds>());
	}
}


auto gse::add_timer(const std::string& name, const std::function<void()>& to_time) -> void {
	if (!global_timers.contains(name)) {
		global_timers.emplace(name, scoped_timer{ name, false });
	}

	to_time();

	if (global_timers.contains(name)) {
		global_timers.at(name).reset();
	}
}

auto gse::timers() -> std::map<std::string, scoped_timer>& {
	return global_timers;
}