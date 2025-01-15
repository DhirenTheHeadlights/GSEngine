export module gse.core.clock;

import std;

import gse.physics.units;

export namespace gse {
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
}

/// Clock

gse::time gse::clock::reset() {
	const time elapsed_time = get_elapsed_time();
	m_start_time = std::chrono::steady_clock::now();
	return elapsed_time;
}

gse::time gse::clock::get_elapsed_time() const {
	const auto now = std::chrono::steady_clock::now();
	const std::chrono::duration<float> elapsed_time = now - m_start_time;
	return seconds(elapsed_time.count());
}

/// ScopedTimer

gse::scoped_timer::~scoped_timer() {
	m_completed = true;
	const time elapsed_time = reset();
	if (m_print) {
		std::cout << m_name << ": " << elapsed_time.as<units::milliseconds>() << "ms\n";
	}
}