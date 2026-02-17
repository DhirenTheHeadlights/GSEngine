export module gse.utility:timer;

import std;

import :clock;

import gse.math;

export namespace gse {
	class scoped_timer : public clock {
	public:
		explicit scoped_timer(std::string name, const bool print = true);
		~scoped_timer();

		auto completed() const -> bool ;
		auto name() const -> std::string ;
		auto set_completed() -> void ;

	private:
		std::string m_name;
		bool m_print = false;
		bool m_completed = true;
	};
}

gse::scoped_timer::scoped_timer(std::string name, const bool print): m_name(std::move(name)), m_print(print) {}

gse::scoped_timer::~scoped_timer() {
	if (!m_completed) {
		const auto elapsed_time = elapsed<float>().as<milliseconds>();
		if (m_print) {
			std::println("[Timer] {} took {}", m_name, elapsed_time);
		}
	}
}

auto gse::scoped_timer::completed() const -> bool {
	return m_completed;
}

auto gse::scoped_timer::name() const -> std::string {
	return m_name;
}

auto gse::scoped_timer::set_completed() -> void {
	m_completed = true;
}

