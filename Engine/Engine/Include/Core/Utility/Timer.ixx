export module gse.utility:timer;

import std;

import :clock;

import gse.physics.math;

export namespace gse {
	class scoped_timer : public clock {
	public:
		scoped_timer(std::string name, const bool print = true) : m_name(std::move(name)), m_print(print) {}
		~scoped_timer();

		auto completed() const -> bool { return m_completed; }
		auto name() const -> std::string { return m_name; }
		auto set_completed() -> void { m_completed = true; }
	private:
		std::string m_name;
		bool m_print = false;
		bool m_completed = true;
	};

	auto add_timer(const std::string& name) -> void;
	auto reset_timer(const std::string& name) -> void;
	auto remove_timer(const std::string& name) -> void;
	auto get_timers() -> std::map<std::string, scoped_timer>&;
}

gse::scoped_timer::~scoped_timer() {
	m_completed = true;
	const time elapsed_time = reset();
	if (m_print) {
		std::cout << m_name << ": " << elapsed_time.as<units::milliseconds>() << "ms\n";
	}
}

std::map<std::string, gse::scoped_timer> g_timers;

auto gse::add_timer(const std::string& name) -> void {
	if (!g_timers.contains(name)) {
		g_timers.emplace(name, scoped_timer{ name, false });
	}
}

auto gse::reset_timer(const std::string& name) -> void {
	if (g_timers.contains(name)) {
		g_timers.at(name).reset();
	}
}

auto gse::remove_timer(const std::string& name) -> void {
	if (g_timers.contains(name)) {
		g_timers.erase(name);
	}
}

auto gse::get_timers() -> std::map<std::string, scoped_timer>& {
	return g_timers;
}